// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(TIME_C)
#define TIME_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_time 1
#elif !defined(TESTING_time)
#define TESTING_time 0
#endif

#include "cbase.h"

int64
strftime2(char *buffer, int64 size, char *format, struct tm *time_info) {
    int64 n;

    n = (int64)strftime(buffer, (size_t)size, format, time_info);
    if ((n <= 0) || (n >= size)) {
        error("Error in strftime(\"%s\") (n = %lld).\n", format, n);
        fatal(EXIT_FAILURE);
    }
    return n;
}

void
print_timings(char *file, int32 line, char *func,
              int64 nitems, struct timespec t0, struct timespec t1) {
    llong seconds = t1.tv_sec - t0.tv_sec;
    llong nanos = t1.tv_nsec - t0.tv_nsec;

    double total_seconds = (double)seconds + (double)nanos / 1.0e9;
    double micros_per = 1e6*(total_seconds / (double)nitems);

    printf("\ntime elapsed %s:%d:%s\n", file, line, func);
    printf("%gs = %gus per item.\n", total_seconds, micros_per);
    return;
}

double
timediff(struct timespec t0, struct timespec t1) {
    llong sec = t1.tv_sec - t0.tv_sec;
    llong nsec = t1.tv_nsec - t0.tv_nsec;
    double diff = (double)sec + (double)nsec*1e-9;
    return diff;
}

void
sleep_ns(int64 nanoseconds) {
    if (nanoseconds <= 0) {
        return;
    }

#if OS_WINDOWS
    while (nanoseconds > 0) {
        int64 chunk = MIN(nanoseconds, (int64)UINT32_MAX*1000000);
        DWORD milliseconds = (DWORD)((chunk + 999999) / 1000000);

        Sleep(milliseconds);
        nanoseconds -= (int64)milliseconds*1000000;
    }
#elif defined(__EMSCRIPTEN__)
    while (nanoseconds > 0) {
        int64 chunk = MIN(nanoseconds, (int64)UINT_MAX*1000000);
        uint milliseconds = (uint)((chunk + 999999) / 1000000);

        emscripten_sleep(milliseconds);
        nanoseconds -= (int64)milliseconds*1000000;
    }
#elif OS_UNIX
    {
        struct timespec requested;
        struct timespec remaining;

        requested.tv_sec = (time_t)(nanoseconds / 1000000000);
        requested.tv_nsec = (long)(nanoseconds % 1000000000);

        while (nanosleep(&requested, &remaining) < 0) {
            if (errno != EINTR) {
                error("Error sleeping: %s.\n", strerror(errno));
                fatal(EXIT_FAILURE);
            }
            requested = remaining;
        }
    }
#elif !defined(__STDC_NO_THREADS__)
    {
        struct timespec requested;
        struct timespec remaining;
        int status;

        requested.tv_sec = (time_t)(nanoseconds / 1000000000);
        requested.tv_nsec = (long)(nanoseconds % 1000000000);

        while ((status = thrd_sleep(&requested, &remaining))
               == thrd_interrupted) {
            requested = remaining;
        }
        if (status != thrd_success) {
            error("Error sleeping.\n");
            fatal(EXIT_FAILURE);
        }
    }
#else
    #error "No sleep_ns implementation for this platform"
#endif
    return;
}

void
sleep_us(int64 microseconds) {
    if (microseconds <= 0) {
        return;
    }
    if (microseconds > (INT64_MAX / 1000)) {
        sleep_ns(INT64_MAX);
    } else {
        sleep_ns(microseconds*1000);
    }
    return;
}

void
sleep_ms(int64 milliseconds) {
    if (milliseconds <= 0) {
        return;
    }
    if (milliseconds > (INT64_MAX / 1000000)) {
        sleep_ns(INT64_MAX);
    } else {
        sleep_ns(milliseconds*1000000);
    }
    return;
}

#if OS_WINDOWS
static int32
windows_time_monotonic(struct timespec *time) {
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    LONGLONG seconds;
    LONGLONG remainder;

    if (!QueryPerformanceFrequency(&frequency)
        || !QueryPerformanceCounter(&counter)) {
        errno = EIO;
        return -1;
    }

    seconds = counter.QuadPart / frequency.QuadPart;
    remainder = counter.QuadPart % frequency.QuadPart;
    time->tv_sec = (time_t)seconds;
    time->tv_nsec = (long)((remainder*1000000000ll) / frequency.QuadPart);
    return 0;
}
#endif

#if defined(__EMSCRIPTEN__)
static int32
emscripten_time_monotonic(struct timespec *time) {
    double seconds;

    seconds = emscripten_get_now() / 1.0e3;
    time->tv_sec = (time_t)seconds;
    time->tv_nsec = (long)((seconds - (double)time->tv_sec)*1.0e9);

    return 0;
}
#endif

void
time_monotonic_precise(struct timespec *time) {
    int32 status;

#if OS_WINDOWS
    status = windows_time_monotonic(time);
#elif defined(__EMSCRIPTEN__)
    status = emscripten_time_monotonic(time);
#elif defined(CLOCK_MONOTONIC_RAW)
    status = clock_gettime(CLOCK_MONOTONIC_RAW, time);
#elif defined(CLOCK_MONOTONIC)
    status = clock_gettime(CLOCK_MONOTONIC, time);
#else
    struct timeval timeval;

    status = gettimeofday(&timeval, NULL);
    if (status == 0) {
        time->tv_sec = timeval.tv_sec;
        time->tv_nsec = timeval.tv_usec*1000;
    }
#endif

    if (status < 0) {
        *time = (struct timespec){0};
    }
    return;
}

int64
time_monotonic_now(void) {
    struct timespec time = {0};
    int64 nanoseconds;

    time_monotonic_precise(&time);
    nanoseconds = (int64)time.tv_sec*1000000000ll;
    nanoseconds += (int64)time.tv_nsec;
    return nanoseconds;
}

int64
time_elapsed_ns(int64 start, int64 end) {
    return end - start;
}

int64
time_elapsed_ms(int64 start, int64 end) {
    return time_elapsed_ns(start, end) / 1000000ll;
}

void
time_monotonic_coarse(struct timespec *time) {
    int32 status;

#if OS_WINDOWS
    status = windows_time_monotonic(time);
#elif defined(__EMSCRIPTEN__)
    status = emscripten_time_monotonic(time);
#elif defined(CLOCK_MONOTONIC_COARSE)
    status = clock_gettime(CLOCK_MONOTONIC_COARSE, time);
#elif defined(CLOCK_MONOTONIC)
    status = clock_gettime(CLOCK_MONOTONIC, time);
#elif defined(CLOCK_MONOTONIC_RAW)
    status = clock_gettime(CLOCK_MONOTONIC_RAW, time);
#else
    struct timeval timeval;

    status = gettimeofday(&timeval, NULL);
    if (status == 0) {
        time->tv_sec = timeval.tv_sec;
        time->tv_nsec = timeval.tv_usec*1000;
    }
#endif

    if (status < 0) {
        *time = (struct timespec){0};
    }
    return;
}

#if OS_UNIX
void
timezone_init(void) {
    time_t current_time;
    struct tm local_tm;
    struct tm gm_tm;

    current_time = time(NULL);
    localtime_r(&current_time, &local_tm);
    gmtime_r(&current_time, &gm_tm);

    timezone_offset = (local_tm.tm_hour - gm_tm.tm_hour)*3600;
    timezone_offset += (local_tm.tm_min - gm_tm.tm_min)*60;

    if (local_tm.tm_year < gm_tm.tm_year) {
        timezone_offset -= 24*3600;
    } else if (local_tm.tm_year > gm_tm.tm_year) {
        timezone_offset += 24*3600;
    } else if (local_tm.tm_yday < gm_tm.tm_yday) {
        timezone_offset -= 24*3600;
    } else if (local_tm.tm_yday > gm_tm.tm_yday) {
        timezone_offset += 24*3600;
    }

    timezone_initialized = true;
    return;
}
#endif

#if 0 == TESTING_time
static inline void
time_functions_sink(void) {
    (void)time_functions_sink;
    (void)strftime2;
    (void)print_timings;
    (void)sleep_ms;
    (void)sleep_ns;
    (void)sleep_us;
    (void)timediff;
    (void)time_elapsed_ms;
    (void)time_elapsed_ns;
    (void)time_monotonic_coarse;
    (void)time_monotonic_now;
    (void)time_monotonic_precise;
#if OS_UNIX
    (void)timezone_init;
#endif
    return;
}
#endif

#if TESTING_time
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    struct timespec t0;
    struct timespec t1;
    struct timespec td0 = {.tv_sec = 1, .tv_nsec = 250000000};
    struct timespec td1 = {.tv_sec = 3, .tv_nsec = 750000000};

    {
        char buffer[128];
        struct tm fixed_time = {0};

        fixed_time.tm_year = 126;
        fixed_time.tm_mon = 2;
        fixed_time.tm_mday = 25;
        fixed_time.tm_hour = 12;
        fixed_time.tm_min = 0;
        fixed_time.tm_sec = 0;

        strftime2(buffer, SIZEOF(buffer), "%Y-%m-%d", &fixed_time);
        ASSERT_EQUAL(buffer, "2026-03-25");
    }

    ASSERT_CLOSE(timediff(td0, td1), 2.5);

    sleep_ns(1);
    sleep_us(1);
    sleep_ms(1);

    time_monotonic_precise(&t0);
    time_monotonic_coarse(&t1);
    ASSERT_MORE_EQUAL(time_monotonic_now(), 0);
    ASSERT_EQUAL(time_elapsed_ns(100, 250), 150);
    ASSERT_EQUAL(time_elapsed_ms(1000000, 4000000), 3);
#if OS_UNIX
    timezone_init();
    ASSERT(timezone_initialized);
#endif
    time_monotonic_precise(&t1);
    ASSERT_MORE_EQUAL(timediff(t0, t1), 0.0);
    PRINT_TIMINGS(1, t0, t1);

    exit(EXIT_SUCCESS);
}

#endif

#endif /* TIME_C */
