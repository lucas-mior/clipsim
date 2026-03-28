/*
 * Copyright (C) 2025 Mior, Lucas;
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the*License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(SORT_C)
#define SORT_C

#include <stdlib.h>

#include "util.c"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_sort 1
#elif !defined(TESTING_sort)
#define TESTING_sort 0
#endif

#if !defined(COMPARE)
#define COMPARE(A, B) compare_func(A, B)
#endif

#ifndef MAX_NTHREADS
#define MAX_NTHREADS 64
#endif

#if !defined(LENGTH)
#define LENGTH(x) (int64)((sizeof(x) / sizeof(*x)))
#endif

#if !defined(SIZEOF)
#define SIZEOF(X) (int64)sizeof(X)
#endif

typedef struct HeapNode {
    void *value;
    int32 p_index;
    int32 unused;
} HeapNode;

static void
sort_shuffle(void *array, int64 n, int64 size) {
    char *tmp = xmalloc(size);
    char *arr = array;

    if (n > 1) {
        for (int64 i = 0; i < n - 1; i += 1) {
            int64 rnd = rand();
            int64 j = i + rnd / (RAND_MAX / (n - i) + 1);

            memcpy64(tmp, arr + j*size, size);
            memcpy64(arr + j*size, arr + i*size, size);
            memcpy64(arr + i*size, tmp, size);
        }
    }

    free(tmp, size);
    return;
}

static void
sort_heapify(HeapNode *heap, int32 p, int32 i,
             int32 (*compare_func)(const void *a, const void *b)) {
    (void)compare_func;
    while (true) {
        int32 smallest = i;
        int32 left = 2*i + 1;
        int32 right = 2*i + 2;

        if (left >= p) {
            break;
        }

        if (COMPARE(heap[left].value, heap[smallest].value) < 0) {
            smallest = left;
        }
        if ((right < p)
            && COMPARE(heap[right].value, heap[smallest].value) < 0) {
            smallest = right;
        }

        if (smallest == i) {
            break;
        }

        {
            HeapNode temp = heap[i];
            heap[i] = heap[smallest];
            heap[smallest] = temp;
            i = smallest;
        }
    }
    return;
}

static void
sort_merge_subsorted(void *array, int32 n, int32 p, int64 obj_size,
                     void *dummy_last,
                     int32 (*compare)(const void *a, const void *b)) {
    HeapNode heap[MAX_NTHREADS];
    int32 n_sub[MAX_NTHREADS];
    int32 indices[MAX_NTHREADS] = {0};
    int32 offsets[MAX_NTHREADS];
    int64 memory_size = obj_size*n;
    char *output = xmalloc(memory_size);
    char *array2 = array;

    for (int32 k = 0; k < (p - 1); k += 1) {
        n_sub[k] = n / p;
    }
    {
        int32 k = p - 1;
        n_sub[k] = n / p + (n % p);
    }

    offsets[0] = 0;
    for (int32 k = 1; k < p; k += 1) {
        offsets[k] = offsets[k - 1] + n_sub[k - 1];
    }

    for (int32 k = 0; k < p; k += 1) {
        heap[k].value = xmalloc(obj_size);
        memcpy64(heap[k].value, &array2[offsets[k]*obj_size], obj_size);
        heap[k].p_index = k;
    }

    for (int32 k = p / 2 - 1; k >= 0; k -= 1) {
        sort_heapify(heap, p, (int32)k, compare);
    }

    for (int32 i = 0; i < n; i += 1) {
        int32 k = heap[0].p_index;
        int32 i_sub = (indices[k] += 1);

        memcpy64(&output[i*obj_size], heap[0].value, obj_size);

        if (i_sub < n_sub[k]) {
            memcpy64(heap[0].value, &array2[(offsets[k] + i_sub)*obj_size], obj_size);
        } else {
            memcpy64(heap[0].value, dummy_last, obj_size);
        }
        sort_heapify(heap, p, 0, compare);
    }

    memcpy64(array2, output, n*obj_size);
    free(output, memory_size);
    for (int32 i = 0; i < p; i += 1) {
        free(heap[i].value, obj_size);
    }
    return;
}

#define SORT_BENCHMARK 0

#if !TESTING_sort || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__)
static void
sort(FileList *old) {
    int32 p;

    char *last = "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                 "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                 "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF";
    int32 last_length = strlen32(last);
    FileName *dummy_last;
    int64 dummy_size = STRUCT_ARRAY_SIZE(dummy_last, char, last_length + 1);

    dummy_last = xmalloc(dummy_size);
    memset64(dummy_last, 0, sizeof(*dummy_last));
    memcpy64(dummy_last, last, last_length + 1);

#if SORT_BENCHMARK
    struct timespec t0;
    struct timespec t1;

    FileList copy = {0};
    sort_shuffle(old->files, old->length, SIZEOF(*(old->files)));

    memcpy64(&copy, old, sizeof(*old));
    copy.files = xmalloc(copy.length*sizeof(*(old->files)));
    memcpy64(copy.files, old->files, copy.length*sizeof(*(old->files)));
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
#endif

    p = brn2_threads(brn2_threads_work_sort, old->length, old, NULL, NULL, 0,
                     NULL);
    if (p == 1) {
        free(dummy_last, dummy_size);
        return;
    }

    /* qsort(old->files, old->length, sizeof(*(old->files)),
     * brn2_compare); */
    sort_merge_subsorted(old->files, old->length, p, sizeof(*(old->files)),
                         &dummy_last, brn2_compare);

#if SORT_BENCHMARK
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    qsort(copy.files, copy.length, sizeof(*(copy.files)), brn2_compare);
    if (memcmp64(copy.files, old->files, copy.length*sizeof(*(copy.files)))) {
        error("Error in sorting.\n");
        for (int32 i = 0; i < old->length; i += 1) {
            char *name1 = old->files[i]->name;
            char *name2 = copy.files[i]->name;
            if (strcmp(name1, name2)) {
                error("[%u] = %s != %s\n", i, name1, name2);
            }
        }
        fatal(EXIT_FAILURE);
    } else {
        error("Sorting successful.\n");
    }
    brn2_timings("sorting", t0, t1, old->length);
    exit(EXIT_SUCCESS);
#endif

    free(dummy_last, dummy_size);
    return;
}
#endif

#if TESTING_sort

#define MAXI 10000
static const int32 possibleN[] = {31, 32, 33, 50};
static const int32 possibleP[] = {1, 2, 3, 8};

static int32
compare_int(const void *a, const void *b) {
    const int32 *aa = a;
    const int32 *bb = b;
    return *aa - *bb;
}
static int32 dummy = INT32_MAX;

static void
test_sorting(int32 n, int32 p) {
    int32 *array = xmalloc(n*SIZEOF(*array));
    int32 *n_sub = xmalloc(p*SIZEOF(*n_sub));

    if (n < p*2) {
        fprintf(stderr, "n=%d must be larger than p*2=%d*2\n", n, p);
        exit(EXIT_SUCCESS);
    }

    for (int32 i = 0; i < (p - 1); i += 1) {
        n_sub[i] = n / p;
    }
    {
        int32 i = p - 1;
        n_sub[i] = n / p + (n % p);
    }

    printf("n_sub[P - 1] = %d\n", n_sub[p - 1]);

    srand(42);
    for (int32 i = 0; i < n; i += 1) {
        array[i] = rand() % MAXI;
    }

    {
        int32 offset = 0;
        for (int32 i = 0; i < p; i += 1) {
            qsort64(&array[offset], n_sub[i], SIZEOF(*array), compare_int);
            offset += n_sub[i];
        }
    }

    sort_merge_subsorted(array, n, p, sizeof(dummy), &dummy, compare_int);

    for (int32 i = 0; i < n; i += 1) {
        if (i < (n - 1)) {
            ASSERT_LESS_EQUAL(array[i], array[i + 1]);
        }
    }

    free(array, n*SIZEOF(*array));
    free(n_sub, p*SIZEOF(*n_sub));
    return;
}

int
main(void) {
    for (int32 in = 0; in < LENGTH(possibleN); in += 1) {
        for (int32 ip = 0; ip < LENGTH(possibleP); ip += 1) {
            test_sorting(possibleN[in], possibleP[ip]);
        }
    }
    exit(EXIT_SUCCESS);
}

#endif

#endif
