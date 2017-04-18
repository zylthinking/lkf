
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

static uint64_t base;
static double apple_conversion;

__attribute__((constructor))
static void timer_init()
{
    base = mach_absolute_time();
    mach_timebase_info_data_t info;
    kern_return_t err = mach_timebase_info(&info);
    if (0 != err) {
        printf("timer_init failed\n");
        exit(-1);
    }
    apple_conversion = 1e-6 * (double) info.numer / (double) info.denom;
}

unsigned int now()
{
    uint64_t n = mach_absolute_time() - base;
    unsigned int ms = (unsigned int) (n * apple_conversion);
    return ms;
}

#elif defined(__linux__)
#include <time.h>
static struct timespec base;

__attribute__((constructor))
static void timer_init()
{
    int n = clock_gettime(CLOCK_MONOTONIC, &base);
    if (0 != n) {
        printf("timer_init failed\n");
        exit(-1);
    }
}

unsigned int now()
{
    struct timespec tp;
    int n = clock_gettime(CLOCK_MONOTONIC,  &tp);
    uint64_t ms = (tp.tv_sec - base.tv_sec) * 1000 + 1e-6 * (tp.tv_nsec - base.tv_nsec);
    return (unsigned int) ms;
}

#elif defined(_MSC_VER)

static void timer_init()
{
    MMRESULT result = timeBeginPeriod(1);
    if (TIMERR_NOERROR != result) {
        printf("timer_init failed\n");
        exit(-1);
    }
}

#pragma data_seg(".CRT$XIU")
static intptr_t ptr = timer_init;
#pragma data_seg()

unsigned int now()
{
    return (unsigned int) timeGetTime();
}

#endif
