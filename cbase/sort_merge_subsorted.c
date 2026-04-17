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

#if !defined(SORT_MERGE_SUBSORTED_C)
#define SORT_MERGE_SUBSORTED_C

#include <stdlib.h>

#include "util.c"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_sort_merge_subsorted 1
#elif !defined(TESTING_sort_merge_subsorted)
#define TESTING_sort_merge_subsorted 0
#endif

#if !defined(SORT_MERGE_SUBSORTED_COMPARE)
#define SORT_MERGE_SUBSORTED_COMPARE(A, B) compare_func(A, B)
#endif

#ifndef MAX_NTHREADS
#define MAX_NTHREADS 64
#endif

typedef struct HeapNode {
    void *value;
    int32 p_index;
    int32 unused;
} HeapNode;

static void
sort_shuffle(void *array, int64 n, int64 size) {
    char *tmp = malloc2(size);
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

    free2(tmp, size);
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

        if (SORT_MERGE_SUBSORTED_COMPARE(heap[left].value, heap[smallest].value) < 0) {
            smallest = left;
        }
        if ((right < p)
            && SORT_MERGE_SUBSORTED_COMPARE(heap[right].value, heap[smallest].value) < 0) {
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
    char *output = malloc2(memory_size);
    char *array2 = array;
    if (p == 1) {
        return;
    }

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
        heap[k].value = malloc2(obj_size);
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
    free2(output, memory_size);
    for (int32 i = 0; i < p; i += 1) {
        free2(heap[i].value, obj_size);
    }
    return;
}

#if 0 == TESTING_sort_merge_subsorted
static inline void
sort_functions_sink(void) {
    (void)sort_shuffle;
    (void)sort_heapify;
    (void)sort_merge_subsorted;
    return;
}
#endif

#if TESTING_sort_merge_subsorted

#define MAXI 10000
static int32 possibleN[] = {31, 32, 33, 50};
static int32 possibleP[] = {1, 2, 3, 8};

static int32
compare_int(const void *a, const void *b) {
    const int32 *aa = a;
    const int32 *bb = b;
    return *aa - *bb;
}
static int32 dummy = INT32_MAX;

static void
test_sorting(int32 n, int32 p) {
    int32 *array = malloc2(n*SIZEOF(*array));
    int32 *n_sub = malloc2(p*SIZEOF(*n_sub));

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

    sort_shuffle(array, n, sizeof(*array));

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

    free2(array, n*SIZEOF(*array));
    free2(n_sub, p*SIZEOF(*n_sub));
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

#endif /* TESTING_sort_merge_subsorted */

#endif /* SORT_MERGE_SUBSORTED_C */
