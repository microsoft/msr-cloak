#pragma once
#include <map>
#include <set>
#include <iostream>
#include <string>
#include <immintrin.h>

#ifndef NO_TSX
#define XBEGIN _xbegin()
#define XEND _xend()
#else
#define XBEGIN _XBEGIN_STARTED
#define XEND
#endif

using Status = decltype(_XBEGIN_STARTED);

struct Result
{
	Status s;
	uint64_t time;
};

using ErrorHistogram = std::map<Status, uint32_t>;
std::ostream& operator<<(std::ostream& os, const ErrorHistogram& e);

using TimingHistogram = std::map<size_t, uint32_t>;
std::ostream& operator<<(std::ostream& os, const TimingHistogram& t);

using ErrorTimingHistogram = std::map<Status, TimingHistogram>;
std::ostream& operator<<(std::ostream& os, const ErrorTimingHistogram& et);

double getAverageTime(const TimingHistogram& t);
uint32_t countEntries(const TimingHistogram& t);

std::string statusToString(const unsigned int e);

constexpr int nCacheSets = 64;
constexpr size_t sizeCacheLine = 64;
constexpr size_t sizePage = 1 << 12;
constexpr size_t conflictOffset = nCacheSets*sizeCacheLine;

unsigned int calcIndex(int cacheSet, int stride, int offset);
void read(void* p, int cacheSet, int stride, int offset = 0);
void write(volatile void* p, int cacheSet, int stride, char value = 'x', int offset = 0);
bool check(void* p, int cacheSet, int stride, char value = 'x');
void clean(char* p, int cacheSet, int offset, int nWays=8);
void prefetch(char* p, int cacheSet, int stride, int offset);

void maxPriority();
void pinToCore(int coreIndex);
void writeToDisk(ErrorTimingHistogram& et, int index=-1);

template<int F, typename T>
auto _round(T i)
{
	return ((i + F / 2) / F) * F;
}

template<typename T>
void fillBuffer(char* buf, int size, T opcode)
{
	auto _buf = reinterpret_cast<T*>(buf);
	for (int i = 0; i < (size / sizeof(T)); i++)
	{
		*_buf++ = opcode;
	}
}
