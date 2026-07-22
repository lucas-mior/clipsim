#if !defined(CBASE_CONFIG_H)
#define CBASE_CONFIG_H

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#define TESTING_arena 0
#define TESTING_assert 0
#define TESTING_generic 0
#define TESTING_hash 0
#define TESTING_memory 0
#define TESTING_minmax 0
#define TESTING_sort 0
#define TESTING_utf8 0
#define TESTING_util 0
#define TESTING_xenums 0

#endif /* CBASE_CONFIG_H */
