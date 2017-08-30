#include <immintrin.h>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <algorithm>
#include <Windows.h>
#include <sstream>
#include <fstream>
#include "primitives.h"

using namespace std;

std::string statusToString(const unsigned int e)
{
	if (e == 0) return "OTHER";
	if (e == _XBEGIN_STARTED) return "STARTED";

	std::string s = "";
	if (e & _XABORT_EXPLICIT) s += "EXPLICIT";
	if (e & _XABORT_RETRY) s += "RETRY_";
	if (e & _XABORT_CONFLICT) s += "CONFLICT";
	if (e & _XABORT_CAPACITY) s += "CAPACITY";
	if (e & _XABORT_DEBUG) s += "DEBUG";
	if (e & _XABORT_NESTED) s += "NESTED";
	return s;
}

std::ostream& operator<<(std::ostream& os, const ErrorHistogram& e)
{
	// print results
	unsigned int total = 0;
	for (auto& code : e)
	{
		total += code.second;
	}

	for (auto& code : e)
	{
		os << statusToString(code.first) << ": " << code.second <<
			" (" << 100.0*code.second/total<< "%) \n";
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const TimingHistogram& t)
{
	for (auto& entry : t)
	{
		// print "<exec time> <number of occurences>"
		os << entry.first << " " << entry.second << "\n";
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const ErrorTimingHistogram& et)
{
	for (auto& code : et)
	{
		os << statusToString(code.first) << ": " 
			<< countEntries(code.second) << " measurements, "
			<< getAverageTime(code.second) << " cycles (average)\n";
	}
	return os;
}

double getAverageTime(const TimingHistogram& t)
{
	size_t sum = 0, n = 0;
	for (auto& entry : t)
	{
		sum += entry.first* entry.second;
		n += entry.second;
	}
	return ((double)sum / (double)n);
}

uint32_t countEntries(const TimingHistogram& t)
{
	uint32_t sum = 0;
	for (auto& entry : t)
	{
		sum += entry.second;
	}
	return sum;
}

volatile char v;

unsigned int calcIndex(int cacheSet, int stride, int offset)
{
	assert(cacheSet < nCacheSets);
	return cacheSet * sizeCacheLine + (stride + offset) * conflictOffset;
}

void read(void* p, int cacheSet, int stride, int offset)
{
	auto i = calcIndex(cacheSet, stride, offset);
	v = ((char*)p)[i];
}

void write(volatile void* p, int cacheSet, int stride, char value, int offset)
{
	auto i = calcIndex(cacheSet, stride, offset);
	((volatile char*)p)[i] = value;
}

void prefetch(char* p, int cacheSet, int stride, int offset)
{
	auto i = calcIndex(cacheSet, stride, offset);
	_m_prefetchw(p + i);
}

bool check(void* p, int cacheSet, int stride, char value)
{
	assert(cacheSet < nCacheSets);
	auto i = cacheSet * sizeCacheLine + stride * conflictOffset;
	return ((char*)p)[i] == value;
}

void clean(char* p, int cacheSet, int offset, int nWays)
{
	p += cacheSet * sizeCacheLine;
	p += offset * conflictOffset;
	for (int i = 0; i < nWays; i++)
	{
		v = *p;
		_mm_mfence();
		_mm_clflush((const void*)&v);
		_mm_clflush(p);
		p += conflictOffset;
	}
	_mm_mfence();
}

void maxPriority()
{
	auto hp = GetCurrentProcess();
	auto ht = GetCurrentThread();
	SetPriorityClass(hp, REALTIME_PRIORITY_CLASS);
	auto p = GetPriorityClass(hp);
	SetThreadPriorityBoost(ht, TRUE);
	SetThreadPriority(ht, THREAD_PRIORITY_TIME_CRITICAL);
}

void pinToCore(int coreIndex)
{
	SetThreadAffinityMask(GetCurrentThread(), 1 << coreIndex);
}

void writeToDisk(ErrorTimingHistogram& et, int index)
{
	for (auto &run : et)
	{
		stringstream ss;
		if (index >= 0) ss << index << "_";
		
		ss << statusToString(run.first) << ".csv";
		auto nameOutFile = ss.str();

		ofstream outFile(nameOutFile, ios_base::trunc);
		if (outFile.fail())
		{
			cerr << "Failed to write to file " << nameOutFile << "\n";
			return;
		}
		outFile << run.second;
		outFile.close();
	}
}