#include <thread>
#include <map>
#include <vector>
#include <future>
#include <numeric>
#include <iostream>
#include <immintrin.h>
#include <Windows.h>

#include "cacheutils.h"

// Constants for configurations

#define READ_OP (1 << 0)
#define WRITE_OP (1 << 1)
#define EXECUTE_OP (1 << 2)

// Configurations

// Uncomment to disable usage of TSX
//#define NO_TSX

// Uncomment to observe cache hits when missing prediction
#define SUCCESSFULL_ATTEMPT

// Use constants to select appropriate preload and secret access modes
#define PRELOAD (READ_OP)
#define SECRET (READ_OP)

// This number varies on different systems
#define MIN_CACHE_MISS_CYCLES (200)

#define NUMBER_OF_ATTEMPTS (500)
#define MAX_FIRST_DELAY (1000)
#define MAX_SECOND_DELAY (1000)
#define PERCENTAGE_THRESHOLD (1)

// TSX macros

#ifndef NO_TSX
#define XBEGIN _xbegin()
#define XEND _xend()
#else
#define XBEGIN _XBEGIN_STARTED
#define XEND
#endif

extern "C" void delayLoop(size_t delay);
extern "C" void recurse(size_t depth);

volatile long long customBarrier;

std::map<size_t, std::map<size_t, size_t> > timings;

constexpr auto TARGETS = 8;

typedef void(*secretFunTy)();
unsigned char secretFunBody[64] = { 0x0F, 0x1F, 0xC3, 0x48, 0x33, 0xC9, 0x48, 0xFF, 0xC1, 0x48, 0x83, 0xF9, 0x1 /*NLOOP*/, 0x76, 0xF7, 0xC3,
                                    0x0F, 0x0B, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90 };



#define DATA_SIZE (2 * 4096)
char *data;


#define READ_LENGTH (16 * 64)

void threadfunc(size_t secret)
{
    if (SetThreadAffinityMask(GetCurrentThread(), (size_t)1 << (((unsigned char)rand() % 2) + 2)) == 0)
    {
        std::cout << "Failed to set affinity of attacker\n";
    }

    // Syncronize start of transaction with start of attack
    InterlockedDecrement64((volatile long long*)&customBarrier);
    while (customBarrier);

    // Delay transaction a bit to capture earliest parts.
    for (int i = 0; i < 100; ++i) maccess((void*)&customBarrier);

    unsigned status = 0;
    // Start transaction
    if ((status = XBEGIN) == _XBEGIN_STARTED) {
        // Preload lots of data
#if (PRELOAD & READ_OP) != 0
        for (int i = 0; i < (READ_LENGTH / 64) / 2; ++i)
        {
            maccess(data + 2 * i * 64);
            maccess(data + (2 * i + 1) * 64);
            maccess(data + 2 * i * 64);
            maccess(data + (2 * i + 1) * 64);
        }
#endif
#if (PRELOAD & WRITE_OP) != 0
        for (int i = 0; i < (READ_LENGTH / 64) / 2; ++i)
        {
            ((volatile char*)data)[2 * i * 64] = 1;
            ((volatile char*)data)[(2 * i + 1) * 64] = 1;
            ((volatile char*)data)[2 * i * 64] = 1;
            ((volatile char*)data)[(2 * i + 1) * 64] = 1;
        }
#endif
#if (PRELOAD & EXECUTE_OP) != 0
        for (int i = 0; i < (READ_LENGTH / 64); ++i)
        {
            ((secretFunTy)(data + i * 64 + 2))();
        }
#endif
        _mm_mfence();

        // Random other code
        for (int i = 0; i < 32; ++i)
        {
            maccess(data + (TARGETS + (i % ((READ_LENGTH / 64) - TARGETS))) * 64);
        }
        for (int i = 0; i < 16; ++i)
        {
            ((secretFunTy)(data + (TARGETS + (i % ((READ_LENGTH / 64) - TARGETS))) * 64 + 2))();
        }
        recurse(16);

        // Secret data access
#if (SECRET & READ_OP) != 0
        maccess(data + secret * 64);
#endif
#if (SECRET & WRITE_OP) != 0
        ((volatile char*)data)[secret * 64] = 1;
#endif
#if (SECRET & EXECUTE_OP) != 0
        ((secretFunTy)(data + secret * 64))();
#endif

        XEND;
    }
}

void warmUp()
{
    for (size_t i = 0; i < DATA_SIZE / 64; ++i)
    {
        memcpy(data + i * 64, secretFunBody, 64);
        maccess(data + i * 64);
        ((secretFunTy)(data + i * 64 + 2))();
    }
    for (int i = 0; i < 10; ++i)
    {
        delayLoop(100);
        delayLoop(0);
    }
}

int main()
{
    if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS) == 0)
    {
        std::cout << "Failed to set priority class of process\n";
    }
    if (SetThreadAffinityMask(GetCurrentThread(), 1 << 1) == 0)
    {
        std::cout << "Failed to set affinity of attacker\n";
    }

    data = (char*)VirtualAlloc(0, DATA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    warmUp();

    for (size_t firstDelay = 0; firstDelay < MAX_FIRST_DELAY; firstDelay += 1)
    {
        for (size_t secondDelay = 0; secondDelay < MAX_SECOND_DELAY; secondDelay += 50)
        {
            for (size_t j = 0; j < NUMBER_OF_ATTEMPTS; ++j)
            {
                customBarrier = 2;
#ifdef SUCCESSFULL_ATTEMPT
                auto victim = std::async(std::launch::async, threadfunc, 0);
#else
                auto victim = std::async(std::launch::async, threadfunc, 7);
#endif

                // Syncronize start of transaction with start of attack
                InterlockedDecrement64((volatile long long*)&customBarrier);
                while (customBarrier);

                // First configurable delay to overlap flush with usage of secret
                delayLoop(firstDelay);

                // Flush
                flush(data + 0 * 64);

                // Second configurable delay to wait before maccess
                delayLoop(secondDelay);

                // Measure memory access time
                size_t time = rdtsc();
                maccess(data + 0 * 64);
                size_t delta = rdtsc() - time;

                // Check for hit and store
                if (delta < MIN_CACHE_MISS_CYCLES)
                {
                    timings[firstDelay][secondDelay]++;
                }

                victim.wait();
            }
        }
    }

    for (auto ait : timings)
    {
        for (auto kit : ait.second)
        {
            size_t percentage = (100 * kit.second) / NUMBER_OF_ATTEMPTS;
            if (percentage >= PERCENTAGE_THRESHOLD)
            {
                printf("%zu, %zu, %zu\n", ait.first, kit.first, percentage);
            }
        }
    }
    fflush(stdout);

    VirtualFree(data, 0, MEM_DECOMMIT | MEM_RELEASE);

    return 0;
}

