#include <Windows.h>
#include <iostream>
#include "..\primitives.h"

using namespace std;

extern "C" unsigned int readMem(char* p, size_t size);

auto allocMem(size_t size)
{
#ifdef LARGE_PAGES
	auto lp = GetLargePageMinimum();
	cout << "Large page size: " << lp << "\n";
	auto _size = ((size + lp - 1) / lp) * lp;
	return VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
#else
	return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE , PAGE_READWRITE);
#endif
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		cerr << "Usage: " << argv[0] << " <read set size>\n";
		return -1;
	}

	maxPriority();
	auto sizeMem = atoll(argv[1]);
	auto buf = (char*)allocMem(sizeMem);
	if (!buf)
	{
		cerr << "Arg mem. Error: " << GetLastError() << "\n";
		return -1;
	}

	memset(buf, 0x24, sizeMem);

	ErrorHistogram e;
	for (int i = 0; i < 100; i++)
	{
		e[readMem(buf, sizeMem)]++;
	}

	cout << e;
	return 0;
}

