#include <windows.h>
#include <iostream>
#include <future>
#include <thread>
#include <functional>
#include "../primitives.h"

using namespace std;
using namespace std::placeholders;

constexpr auto spinWait = 10000;
constexpr auto targetCacheSet = 0;
constexpr auto measurements = 10000;
constexpr auto roundingFactor = 10;

constexpr auto nConflicts = 8;
constexpr auto sizeMemAttacker = sizePage*(nConflicts + 1);

atomic<int> syncBarrier = 0;

void waitForOther()
{
	syncBarrier++;
	while (syncBarrier < 2);
}

template<typename R, typename... ARG>
void wrapperStatus(ErrorHistogram& e, R(*f)(ARG...), ARG... args)
{
	pinToCore(0);
	waitForOther();

	auto s = XBEGIN;
	if (s == _XBEGIN_STARTED)
	{
		f(args...);
		XEND;
	}
	e[s]++;
}

template<typename R, typename... ARG>
void wrapperTiming(ErrorTimingHistogram& et, R(*f)(ARG...), ARG... args)
{
	pinToCore(0);
	waitForOther();

	auto t0 = __rdtsc();
	auto s = XBEGIN;
	if (s == _XBEGIN_STARTED)
	{
		f(args...);
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = _round<roundingFactor>(t1 - t0);
	et[s][d]++;
}

void __forceinline victimCode(char* b)
{
	auto f = (void(*)())b;
	f();
	for (volatile int i = 0; i < spinWait; i++);
	//f();
}

//flushes the target
void attackerFlush(char* target)
{
	waitForOther();
	for (volatile int i = 0; i < (spinWait / 10) * 10; i++);
	_mm_clflushopt(target);
}

//tries to evict the target from the L1 ICACHE via execution.
void attackerConflict(char* target)
{
	waitForOther();
	for (volatile int i = 0; i < (spinWait / 10) * 8; i++);
	for (int i = 0; i < nConflicts-1; i++)
	{
		auto f = (void(*)()) (target + i * sizePage);
		auto g = (void(*)()) (target + (i + 1) * sizePage);
		f();
		g();
		f();
		g();
	}
}

template<typename V, typename A>
void runExp(V victim, A attacker, char* target)
{
	for (auto i = 0; i < measurements; i++)
	{
		// reset the sync barrier
		syncBarrier = 0;
		// start victim 
		auto v = async(std::launch::async, victim);
		// run attacker
		attacker(target);
		v.wait();
	}
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		cerr << "Usage: " << argv[0] << "<attacker core index, victim is 0> <size of code in bytes>\n";
		return -1;
	}

	maxPriority();
	auto attackerCore = atoi(argv[1]);
	pinToCore(attackerCore);

	auto sizeMem = atoi(argv[2]);

	// victim
	//// create victim memory
	auto bVictim = reinterpret_cast<char*>(VirtualAlloc(nullptr, sizeMem, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (!bVictim)
	{
		cerr << "Failed to allocate buffer\n";
		return -1;
	}

	//// fill buffer with "inc eax" and write "ret" as last instruction.
	uint16_t incEax = 0xc0ff;
	fillBuffer<uint16_t>(bVictim, sizeMem, incEax);
	/*fillBuffer<uint8_t>(bVictim, sizeMem, 0x90);*/
	bVictim[sizeMem - 2] = 0xc3;

	//// create victim
	ErrorHistogram e;
	auto victim = [&e, bVictim] () {
		wrapperStatus(e, victimCode, bVictim);
	};

	// attacker
	//// create attacker memory
	auto bAttacker = reinterpret_cast<char*>(VirtualAlloc(nullptr, sizeMemAttacker, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (!bAttacker)
	{
		cerr << "Failed to allocate buffer\n";
		return -1;
	}
	memset(bAttacker, 0xc3, sizeMemAttacker);

	// Run experiments
	e.clear();
	runExp(victim, attackerFlush, bVictim);
	cout << "Experiment 1: attacker flushes CL executed by victim after certain delay.\n" << e << "\n";

	e.clear();
	runExp(victim, attackerConflict, bAttacker);
	cout << "Experiment 2: attacker executes CLs conflicting with victim code (in L1) after certain delay.\n" << e << "\n";

	return 0;
}