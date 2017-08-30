#include <Windows.h>
#include <iostream>
#include <immintrin.h>
#include <thread>
#include "..\primitives.h"

using namespace std;

constexpr int clSize = 64;
constexpr int startSize = 1 << 18;
constexpr int stepSize = 64 * clSize;
constexpr int nRetries = 100;

// ASM funcs
extern "C"
{
	int execTrans(void*);
	int exec2Trans(void*);
	int execTransRead1(void*);
}

__declspec(noinline) auto transaction(char* buf)
{
	auto fp = reinterpret_cast<void(*)()>(buf);
	auto status = _xbegin();
	if (status == _XBEGIN_STARTED)
	{
		fp();
		_xend();
	}
	return status;
}

void exp0(const int memSize)
{
	auto buf = (char*)VirtualAlloc(nullptr, memSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!buf)
	{
		cerr << "Argh 0\n";
		return;
	}

	auto i = startSize;
	ErrorHistogram e;
	for (; i < memSize; i += stepSize)
	{
		memset(buf, 0x90, i);
		buf[i] = 0xc3;
		e.clear();
		bool success = false;
		for (int j = 0; j < nRetries; j++)
		{
			auto status = transaction(buf);
			success = status == _XBEGIN_STARTED;
			if (success) break;
			e[status]++;
			Sleep(0);
		}
		if (!success) break;
	}

	cout << "Max. bytes executed: " << i << "\n"
		<< "Abort reasons:\n" << e << "\n";
}

void exp1(const int memSize)
{
	auto buf = (char*)VirtualAlloc(nullptr, memSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!buf)
	{
		cerr << "Argh 0\n";
		return;
	}

	auto i = startSize;
	ErrorHistogram e;
	for (; i < memSize; i += stepSize)
	{
		constexpr auto longNop = 0x0000000000841f0full;
		fillBuffer(buf, i - 8, longNop);
		buf[i - 8] = 0xC3;

		e.clear();
		bool success = false;
		for (int j = 0; j < nRetries; j++)
		{
			auto status = transaction(buf);
			success = status == _XBEGIN_STARTED;
			if (success) break;
			e[status]++;
			Sleep(0);
		}
		if (!success) break;
	}

	cout << "Max. bytes executed: " << i << "\n"
		<< "Abort reasons:\n" << e << "\n";
}

void exp2(const int memSize)
{
	const auto _memSize = memSize / 2;
	auto buf = (char*)VirtualAlloc(nullptr, _memSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!buf)
	{
		cerr << "Argh 0\n";
		return;
	}

	ErrorHistogram e;
	fillBuffer<uint8_t>(buf, _memSize, 0x90);
	// write "jmp rax" to the end of the buffer
	buf[_memSize - 2] = 255;
	buf[_memSize - 1] = 224;

	_mm_mfence();

	for (int i = 0; i < nRetries; i++)
	{
		e[exec2Trans(buf)]++;
		Sleep(0);
	}
	cout << "Executed 2 x " << _memSize << " nops. Errors:\n" << e << "\n";
}

template<typename F>
void exp(F trans, const int memSize, bool nop=false)
{
	auto buf = (char*)VirtualAlloc(nullptr, memSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!buf)
	{
		cerr << "Argh 0\n";
		return;
	}

	ErrorHistogram e;
	if (nop)
	{
		fillBuffer<uint8_t>(buf, memSize, 0x90);
	}
	else
	{
		constexpr uint16_t andEcxEcx = 0xc921;
		fillBuffer<uint16_t>(buf, memSize, andEcxEcx);
	}

	// write "jmp rax" to the end of the buffer
	buf[memSize - 2] = 255;
	buf[memSize - 1] = 224;

	_mm_mfence();
	double cycles = 0;
	for (int i = 0; i < nRetries; i++)
	{
		auto t0 = __rdtsc();
		auto status = trans(buf);
		auto t1 = __rdtsc();
		e[status]++;
		if (status == _XBEGIN_STARTED) cycles += t1 - t0;
		Sleep(0);
	}
	cout << "Executed " << memSize
		<< " nops. Average cycles on success: " << cycles / e[_XBEGIN_STARTED]
		<< " Errors:\n" << e << "\n";
}

template<typename F>
auto expLong(F trans, const size_t startSize, const size_t stepSize, const size_t maxSize, const int retries, bool nop=false)
{
	auto buf = (char*)VirtualAlloc(nullptr, maxSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!buf)
	{
		cerr << "Argh 0\n";
		exit(-1);
	}

	map<size_t, ErrorHistogram> me;
	if (nop)
	{
		fillBuffer<uint8_t>(buf, maxSize, 0x90);
	}
	else
	{
		constexpr uint32_t instr = 0xd221c921; // and ecx, ecx; and edx, edx
		fillBuffer<uint32_t>(buf, maxSize, instr);
	}

	for (auto currentSize = maxSize; currentSize >= startSize; currentSize -= stepSize)
	{
		auto& e = me[currentSize];
		// write "jmp rax" to the end of the buffer
		buf[currentSize - 2] = 255;
		buf[currentSize - 1] = 224;

		_mm_mfence();
		for (int i = 0; i < retries; i++)
		{
			auto status = trans(buf);
			e[status]++;
		}
	}
	return me;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		cerr << "Usage: " << argv[0] << " <max size of code in MB>\n";
		return -1;
	}
	auto memSize = (1 << 20) * atoi(argv[1]);
	pinToCore(0);
	maxPriority();

	for (auto& e : expLong(execTrans, 1 << 16, 1 << 16, memSize, nRetries, false))
	{
		auto success = e.second[_XBEGIN_STARTED];
		cout << e.first << "," << (float)(nRetries - success) / (float)nRetries << "\n";
	}

	//
	// different variations of the experiment follow as comments
	//

	/*cout << "Exp 1 (long nopsled, strictly no mem access, no calls etc. )\n--------------\n";
	exp(execTrans, memSize, true);*/
	/*cout << "Exp 2 (long nopsled, exactly one read, no calls etc. )\n--------------\n";
	exp(execTransRead1, memSize);*/
	/*cout << "Exp 3 (long nopsled, strictly no mem access, no calls etc. Nopsled is executed twice in one transaction.)\n--------------\n";
	exp3(memSize);*/

	return 0;
}