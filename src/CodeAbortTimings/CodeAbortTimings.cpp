#include <thread>
#include <future>
#include <string>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <windows.h>
#include "..\primitives.h"

extern "C"
{
	void nopfunc();
	void dummyfunc();
	size_t recurse(size_t);
	size_t jmpExec16(size_t);
	void branchTo(void*);
	void nullfunc();
	void nopExec(void*);
}

using namespace std;

constexpr int spinWait = 1000;
constexpr int measurements = 1000000;
constexpr size_t MAX_CYCLES = 3000; // used to filter out interrupts
constexpr int ROUNDING_FACTOR = 1;

void spin()
{
	for (volatile int i = 0; i < spinWait; i++) {}
}

// read nopfunc
Result func0()
{
	auto t0 = __rdtsc();
	auto status = XBEGIN;
	if (status == _XBEGIN_STARTED)
	{
		*(volatile size_t*)nopfunc;
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = t1 - t0;
	return{ status, d };
}

// execute nopfunc
Result func1()
{
	auto t0 = __rdtsc();
	auto status = XBEGIN;
	if (status == _XBEGIN_STARTED)
	{
		nopfunc();
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = t1 - t0;
	return{ status, d };
}


// read nopfunc, spin
Result func2()
{
	auto t0 = __rdtsc();
	auto status = XBEGIN;
	if (status == _XBEGIN_STARTED)
	{
		*(volatile size_t*)nopfunc;
		spin();
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = t1 - t0;
	return{ status, d };
}

// execute nopfunc, spin
Result func3()
{
	auto t0 = __rdtsc();
	auto status = XBEGIN;
	if (status == _XBEGIN_STARTED)
	{
		nopfunc();
		spin();
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = t1 - t0;
	return{ status, d };
}

// Read nopfunc, execute nopfunc, spin
Result func4()
{
	auto t0 = __rdtsc();
	auto status = XBEGIN;
	if (status == _XBEGIN_STARTED)
	{
		*(volatile size_t*)nopfunc;
		*(volatile size_t*)dummyfunc;
		_mm_mfence();
		branchTo(nopfunc);
		spin();
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = t1 - t0;
	//_mm_clflushopt(nopfunc);
	//_mm_clflushopt(dummyfunc);
	return{ status, d };
}

// Read nopfunc, execute nopfunc, spin, no mfence
Result func41()
{
	auto t0 = __rdtsc();
	auto status = XBEGIN;
	if (status == _XBEGIN_STARTED)
	{
		*(volatile size_t*)nopfunc;
		//_mm_mfence();
		nopfunc();
		spin();
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = t1 - t0;
	return{ status, d };
}

// Read nopfunc, execute dummyfunc, spin
Result func5()
{
	auto t0 = __rdtsc();
	auto status = XBEGIN;
	if (status == _XBEGIN_STARTED)
	{
		*(volatile size_t*)nopfunc;
		*(volatile size_t*)dummyfunc;
		_mm_mfence();
		branchTo(dummyfunc);
		spin();
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = t1 - t0;
	//_mm_clflushopt(nopfunc);
	//_mm_clflushopt(dummyfunc);
	return{ status, d };
}

// Read spin, execute nopfunc, spin
Result func6()
{
	auto t0 = __rdtsc();
	auto status = XBEGIN;
	if (status == _XBEGIN_STARTED)
	{
		*(volatile size_t*)spin;
		_mm_mfence();
		spin();
		XEND;
	}
	auto t1 = __rdtsc();
	auto d = t1 - t0;
	return{ status, d };
}

std::atomic<bool> runAttacker = true;
DWORD WINAPI attack(LPVOID target)
{
	while (runAttacker)
	{
		//volatile auto x = *(size_t*)target;
		_mm_clflushopt(target);
		//for (volatile int i = 0; i < 100; i++) {}
	}
	return 0;
}

//ErrorHistogram e;
ErrorTimingHistogram t; // timing histograms for every abort code

DWORD WINAPI execLoop(LPVOID p)
{
	auto victimFunc = reinterpret_cast<Result(*)()>(p);
	for (int i = 0; i < measurements; i++)
	{
		auto r = victimFunc();

		// filter interrupts
		if (r.time > MAX_CYCLES) continue;

		// round d to multiples of 10
		auto d = ((r.time + ROUNDING_FACTOR / 2) / ROUNDING_FACTOR) * ROUNDING_FACTOR;
		t[r.s][d]++;
		//e[r.s]++;
	}
	return 0;
}


template<typename F>
void runExp(int id, F victimFunc, const void* target, int victimCore, int attackerCore, const string name)
{
	// clear histograms
	//e.clear();
	t.clear();

	// create victim and attacker threads
	runAttacker = true;
	auto attacker = CreateThread(
		NULL,
		0,
		attack,
		(LPVOID)target,
		CREATE_SUSPENDED,
		NULL
	);

	if (!attacker) throw "Arg.";
	if (!SetThreadAffinityMask(attacker, 1ull << attackerCore)) throw "Arg.";
	ResumeThread(attacker);

	auto victim = CreateThread(
		NULL,
		0,
		execLoop,
		(LPVOID)victimFunc,
		CREATE_SUSPENDED,
		NULL
	);
	if (!victim) throw "Arg.";
	if (!SetThreadAffinityMask(victim, 1ull << victimCore)) throw "Arg.";
	ResumeThread(victim);

	if (WaitForSingleObject(victim, INFINITE) == WAIT_FAILED) throw "Waiting failed.";
	runAttacker = false;
	if (WaitForSingleObject(attacker, INFINITE) == WAIT_FAILED) throw "Waiting failed.";

	cout << "Exp. " << id << ": " << name << "\n";
	//writeToDisk(t, id);
	cout << t;
	
	cout << "\n";
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		cerr << "Usage: " << argv[0] << "<attacker core index, victim is 0>\n";
		return -1;
	}
	maxPriority();
	auto attackerCore = atoi(argv[1]);
	int victimCore = 0;
	
	cout << "\n++++++++++++++++\n" 
		<< "Core " << victimCore << " vs core " << attackerCore 
		<< "\n++++++++++++++++\n\n";

	int id = 0;
	runExp(id++, func0, nopfunc, victimCore, attackerCore, "Victim: read nopfunc; Attacker: evict nopfunc");
	runExp(id++, func1, nopfunc, victimCore, attackerCore, "Victim: execute nopfunc; Attacker: evict nopfunc");
	runExp(id++, func2, nopfunc, victimCore, attackerCore, "Victim: read nopfunc, spin; Attacker: evict nopfunc");
	runExp(id++, func3, nopfunc, victimCore, attackerCore, "Victim: execute nopfunc, spin; Attacker: evict nopfunc");
	runExp(id++, func3, spin, victimCore, attackerCore, "Victim: execute nopfunc, spin; Attacker: evict spin");
	runExp(id++, func4, nopfunc, victimCore, attackerCore, "Victim: read nopfunc, execute nopfunc, spin; Attacker: evict nopfunc");
	runExp(id++, func41, nopfunc, victimCore, attackerCore, "Victim: read nopfunc, ---no mfence---, execute nopfunc, spin; Attacker: evict nopfunc");
	runExp(id++, func6, spin, victimCore, attackerCore, "Victim: read spin, spin; Attacker: evict spin");
	runExp(id++, func5, nopfunc, victimCore, attackerCore, "Victim: read nopfunc, execute dummyfunc, spin; Attacker: evict nopfunc");
	runExp(id++, func4, dummyfunc, victimCore, attackerCore, "Victim: read nopfunc, execute nopfunc, spin; Attacker: evict dummyfunc (comes 64 bytes after nopfunc)");
	
	cout << "Wrote detailed timing reports to disk (*.csv)\n";

	return 0;
}

