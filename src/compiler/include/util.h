#ifndef UTIL_H_
#define UTIL_H_

#include <ctype.h>   // isalpha(), isalnum()
#include <stdarg.h>  // va_list, va_start(), va_end()
#include <stdbool.h> // bool, true, false
#include <stdint.h>  // uintptr_t
#include <stdio.h>   // fprintf
#include <stdlib.h>  // abort(), malloc(), free(), size_t
#include <string.h>  // memcpy

// alwaysinline
#if !defined(alwaysinline)
#    if defined(_MSC_VER) && _MSC_VER >= 1300
#        define alwaysinline __forceinline
#    elif defined(__GNUC__)
#        define alwaysinline __attribute__((__always_inline__)) inline
#    else
#        define alwaysinline
#    endif
#endif

#if !defined(flatten)
#    if defined(__GNUC__)
#        define flatten __attribute__((flatten))
#    else
#        define flatten
#    endif
#endif

// Utility Macros
#if !defined(MAX)
#    define MAX(x, y) ((x) > (y) ? (x) : (y))
#    define MIN(x, y) ((x) < (y) ? (x) : (y))
#    define unused(x) ((void)(x))
#    define swap(type, x, y) \
        do {                 \
            type tmp = (x);  \
            (x) = (y);       \
            (y) = tmp;       \
        } while (0)
#endif

// Low level macros
#if !defined(offsetof)
#    define offsetof(s, f) ((size_t)(uintptr_t) & (((s *)0)->f))
#    define containerof(ptr, s, f) ((s *)((char *)ptr - offsetof(s, f)))
#    define lengthof(_array) (sizeof((_array)) / sizeof((_array)[0]))
#endif

// likely/unlikely
#if !defined(likely)
#    if defined(__GNUC__)
#        define likely(_expr) __builtin_expect(!!(_expr), 1)
#        define unlikely(_expr) __builtin_expect(!!(_expr), 0)
#    else
#        define likely(_expr)
#        define unlikely(_expr)
#    endif
#endif

// Printf argument check
#if !defined(PRINTF_ARGS)
#    if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#        define PRINTF_ARGS(FMT) __attribute__((format(printf, FMT, (FMT + 1))))
#    else
#        define PRINTF_ARGS(FMT)
#    endif
#endif

// unreachable
#if !defined(unreachable)
#    define unreachable1(file, line)                                                        \
        do {                                                                                \
            fprintf(stderr, "Reached 'unreachable()' at line %d of file %s\n", line, file); \
        } while (0)

#    define unreachable0(file, line) unreachable1(file, line)

#    define unreachable() unreachable0(__FILE__, __LINE__)
#endif

// panic
#if !defined(panic)
#    define panic0(line, file, ...)                                         \
        do {                                                                \
            fprintf(stderr, __VA_ARGS__);                                   \
            fprintf(stderr, "\nPanic at line %d in file %s\n", line, file); \
            abort();                                                        \
        } while (0)
#    define panic(...) panic0(__LINE__, __FILE__, __VA_ARGS__)
#endif

// todo
#if !defined(todo)
#    define todo() panic("TODO")
#endif

// xmalloc/xrealloc
extern void *_xmalloc(size_t size, const char *file, uint64_t line);
extern void *_xrealloc(void *_ptr, size_t size, const char *file, uint64_t line);

#define xmalloc(n) _xmalloc((n), __FILE__, __LINE__)
#define xrealloc(p, n) _xrealloc((p), (n), __FILE__, __LINE__)
#define xfree(p) unused((p) != NULL ? (free((void *)(p)), 0) : 0)

// Memory functions
void memswap(void *a, void *b, size_t size);

// Log2
#define log2(n) _log2(n)
alwaysinline int _log2(int n)
{
    int log = 0;
    while (n >>= 1)
        ++log;
    return log;
}

// strf
PRINTF_ARGS(1)
extern char *strf(const char *fmt, ...);

// UTF-8
typedef uint32_t Rune;
#define is_utf8(c) ((c)&0x80)
#define is_ascii(c) (!is_utf8(c))

#define utf8_bytes(c)                            \
    (((c)&0xF0) == 0xF0                      ? 4 \
            : ((c)&0xE0) == 0xE0             ? 3 \
            : ((c)&0xC0) == 0xC0             ? 2 \
            : ((c) == '\0' || (c) == '\x1A') ? 0 \
                                             : 1)

extern Rune utf8_decode(const char *src);
extern bool utf8_isalpha(const char *src);
extern bool utf8_isalnum(const char *src);

// Dynamic Arrays (aka std::vector in C++)
// Implemented using stretchy buffers (Sean Barrett)
typedef struct ArrayHeader {
    size_t length;
    size_t capacity;
    char *ptr;
} ArrayHeader;

extern void *array_growf(void *a, size_t elem_size, size_t addlen,
    size_t new_cap);
PRINTF_ARGS(2)
extern void array_printf(char **array, const char *fmt, ...);

#define array(t) t *
#define array_free(a) ((a) ? (free(array_header((a))), (a) = NULL) : 0)
#define array_capacity(a) ((a) ? (size_t)array_header((a))->capacity : 0)
#define array_length(a) ((a) ? (size_t)array_header((a))->length : 0)
#define array_reserve(a, n) \
    ((a) ? ((a) = array_growf((a), sizeof(*(a)), 0, (n))) : 0)
#define array_push(a, elem) \
    (array_maybegrow((a), 1), (a)[array_header((a))->length++] = (elem))
#define array_pop(a) \
    (array_header((a))->length--, (a)[array_header((a))->length])
#define array_last(a) ((a)[array_header((a))->length - 1])
#define array_end(a) ((a) + array_length(a))

// Internal details
#define array_header(a) containerof(a, ArrayHeader, ptr)
#define array_maybegrow(a, n)                                \
    ((!(a) || array_length((a)) + (n) > array_capacity((a))) \
            ? array_grow((a), (n))                           \
            : 0)
#define array_grow(a, n) ((a) = array_growf((a), sizeof(*(a)), (size_t)(n), 0))

// Sorting:
// sort() is based on pdqsort and it's the fastest
// heapsort() is a standard heapsort algorithm
// The array_* versions use array_length() as the count
#define sort(a, count, cmp) (_sort((a), (count), sizeof(*(a)), (cmp)))
#define array_sort(a, cmp) sort(a, array_length(a), cmp)
#define heapsort(a, count, cmp) (_heapsort((a), (count), sizeof(*(a)), (cmp)))
#define array_heapsort(a, cmp) heapsort(a, array_length(a), cmp)

typedef bool (*comp_t)(void *a, void *b);
extern void _sort(void *array, size_t length, size_t elem_size, comp_t comp);
extern void _heapsort(void *start, size_t length, size_t elem_size,
    comp_t comp);

/* Stringview */
typedef struct stringview {
    const char *str;
    size_t length;
} stringview;

extern stringview stringview_make(const char *ptr, size_t length);
extern void stringview_trim_space_left(stringview *sv);
extern void stringview_trim_space_right(stringview *sv);
extern void stringview_trim_space(stringview *sv);
extern void stringview_chop_left(stringview *sv, size_t size);
extern void stringview_chop_right(stringview *sv, size_t size);
extern void stringview_chop_while(stringview *sv, bool (*predicate)(char));
extern stringview stringview_take_while(stringview sv, bool (*predicate)(char));
extern bool stringview_starts_with(stringview a, stringview b);
extern bool stringview_starts_with_cstr(stringview a, const char *b);
extern bool stringview_ends_with(stringview a, stringview b);
extern bool stringview_ends_with_cstr(stringview a, const char *b);

#ifndef stringview_from_cstr
#    define stringview_from_cstr(cstr) stringview_make((cstr), strlen((cstr)))
#endif

#ifndef SV_FMT
#    define SV_FMT "%.*s"
#    define SV_ARGS(_sv) (int)(_sv).length, (_sv).str
#endif

#endif // UTIL_H_
#ifdef UTIL_IMPL
#undef UTIL_IMPL
// xmalloc/xrealloc
void *_xmalloc(size_t size, const char *file, uint64_t line)
{
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr,
            "[Allocation Error] Couldn't allocate memory at line %ld of file %s! Aborting...\n", line, file);
        abort();
    }
    return ptr;
}

void *_xrealloc(void *_ptr, size_t size, const char *file, uint64_t line)
{
    void *ptr = realloc(_ptr, size);
    if (!ptr) {
        fprintf(stderr,
            "[Allocation Error] Couldn't allocate memory at line %ld of file %s! Aborting...\n", line, file);
        abort();
    }
    return ptr;
}

// strf
char *strf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);
    char *str = xmalloc(n);
    va_start(args, fmt);
    vsnprintf(str, n, fmt, args);
    va_end(args);
    return str;
}

// UTF-8
// This functions come from the Cone Programming Language C Compiler, licensed
// under the MIT License
// (https://github.com/jondgoodwin/cone/blob/master/src/c-compiler/shared/utf8.c)
/*
 *Copyright (C) 2017-2021  Jonathan Goodwin
 *
 * Permission is hereby granted, free of charge, to any
 * person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without
 * limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice
 * shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
Rune utf8_decode(const char *src)
{
    int len;
    Rune rune;
    if ((*src & 0xF0) == 0xF0) {
        len = 4;
        rune = *src & 0x07;
    } else if ((*src & 0xE0) == 0xE0) {
        len = 3;
        rune = *src & 0x0F;
    } else if ((*src & 0xC0) == 0xC0) {
        len = 2;
        rune = *src & 0x1F;
    } else if ((*src & 0x80) == 0x80) {
        return *src & 0x7F;
    } else {
        return 0;
    }
    while (--len) {
        src++;
        if ((*src & 0xC0) == 0x80) {
            rune = (rune << 6) + (*src & 0x3F);
        }
    }
    return rune;
}

bool utf8_isalpha(const char *src) { return is_utf8(*src) || isalpha(*src); }
bool utf8_isalnum(const char *src) { return is_utf8(*src) || isalnum(*src); }

// Memory Functions
void memswap(void *a, void *b, size_t size)
{
    if (a == b) {
        return;
    }

    char *p1 = a;
    char *p2 = b;

    if (size == 4) {
        swap(uint32_t, *(uint32_t *)a, *(uint32_t *)b);
    } else if (size == 8) {
        swap(uint64_t, *(uint64_t *)a, *(uint64_t *)b);
    } else {
        while (size--) {
            swap(uint8_t, *p1, *p2);
            p1++;
            p2++;
        }
    }
}

// Dynamic Arrays
void *array_growf(void *a, size_t elem_size, size_t add_len, size_t new_cap)
{
    ArrayHeader *b;
    size_t old_len = array_length(a);
    size_t new_len = old_len + add_len;
    new_cap = MAX(new_len, new_cap);

    if (new_cap <= array_capacity(a)) {
        return a;
    }
    new_cap = MAX(MAX(new_cap, 2 * array_capacity(a)), 4);

    // If the pointer argument passed to realloc is NULL, then realloc functions
    // as if it was malloc
    b = (ArrayHeader *)xrealloc((a) ? array_header(a) : NULL,
        offsetof(ArrayHeader, ptr) + new_cap * elem_size);
    b->capacity = new_cap;
    b->length = old_len;

    return (void *)((char *)b + offsetof(ArrayHeader, ptr));
}

void array_printf(char **a, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);
    array_maybegrow(*a, n);
    va_start(args, fmt);
    vsnprintf(array_end(*a), n, fmt, args);
    va_end(args);
    array_header(*a)->length += n - 1;
}

// Sorting(based on pdqsort)

// Defines
#define INSERTION_SORT_THRESHOLD 24
#define NINTHER_THRESHOLD 128
#define PARTIAL_INSERTION_SORT_LIMIT 8

// This struct is equal to std::pair<Iter, bool>
typedef struct PartitionResult {
    char *first;
    bool second;
} PartitionResult;

static inline void insertion_sort(char *begin, char *end, size_t elem_size,
    comp_t comp)
{
    if (begin == end) {
        return;
    }

    char *tmp = xmalloc(elem_size);

    for (char *cur = begin + elem_size; cur != end; cur += elem_size) {
        char *sift = cur;
        char *sift_1 = cur - elem_size;

        if (comp(sift, sift_1)) {
            memcpy(tmp, sift, elem_size);

            do {
                memcpy(sift, sift_1, elem_size);
                sift -= elem_size;
                sift_1 -= elem_size;
            } while (sift != begin && comp(tmp, sift_1));

            memcpy(sift, tmp, elem_size);
        }
    }
    free(tmp);
}

static inline void unguarded_insertion_sort(char *begin, char *end,
    size_t elem_size, comp_t comp)
{
    if (begin == end) {
        return;
    }

    char *tmp = malloc(elem_size);

    for (char *cur = begin + elem_size; cur != end; cur += elem_size) {
        char *sift = cur;
        char *sift_1 = cur - elem_size;

        if (comp(sift, sift_1)) {
            memcpy(tmp, sift, elem_size);

            do {
                memcpy(sift, sift_1, elem_size);
                sift -= elem_size;
                sift_1 -= elem_size;
            } while (comp(tmp, sift_1));

            memcpy(sift, tmp, elem_size);
        }
    }
    free(tmp);
}

static inline bool partial_insertion_sort(char *begin, char *end,
    size_t elem_size, comp_t comp)
{
    if (begin == end) {
        return true;
    }

    char *tmp = xmalloc(elem_size);

    size_t limit = 0;
    for (char *cur = begin + elem_size; cur != end; cur += elem_size) {
        char *sift = cur;
        char *sift_1 = cur - elem_size;

        if (comp(sift, sift_1)) {
            memcpy(tmp, sift, elem_size);

            do {
                memcpy(sift, sift_1, elem_size);
                sift -= elem_size;
                sift_1 -= elem_size;
            } while (sift != begin && comp(tmp, sift_1));

            memcpy(sift, tmp, elem_size);
            limit += cur - sift;
        }

        if (limit > PARTIAL_INSERTION_SORT_LIMIT) {
            free(tmp);
            return false;
        }
    }
    free(tmp);
    return true;
}

static inline void sort2(char *a, char *b, size_t elem_size, comp_t comp)
{
    if (comp(b, a)) {
        memswap(a, b, elem_size);
    }
}

static inline void sort3(char *a, char *b, char *c, size_t elem_size,
    comp_t comp)
{
    sort2(a, b, elem_size, comp);
    sort2(b, c, elem_size, comp);
    sort2(a, c, elem_size, comp);
}

static inline char *partition_left(char *begin, char *end, size_t elem_size,
    comp_t comp)
{
    char *pivot = xmalloc(elem_size);
    memcpy(pivot, begin, elem_size);

    char *first = begin;
    char *last = end;
    do {
        last -= elem_size;
    } while (comp(pivot, last));

    if (last + elem_size == end) {
        do {
            first += elem_size;
        } while (first < last && !comp(pivot, first));
    } else {
        do {
            first += elem_size;
        } while (!comp(pivot, first));
    }

    while (first < last) {
        memswap(first, last, elem_size);
        do {
            last -= elem_size;
        } while (comp(pivot, last));
        do {
            first += elem_size;
        } while (!comp(pivot, first));
    }

    char *pivot_pos = last;

    memcpy(begin, pivot_pos, elem_size);
    memcpy(pivot_pos, pivot, elem_size);
    free(pivot);

    return pivot_pos;
}

static PartitionResult partition_right(char *begin, char *end, size_t elem_size,
    comp_t comp)
{
    char *pivot = xmalloc(elem_size);
    memcpy(pivot, begin, elem_size);

    char *first = begin;
    char *last = end;

    do {
        first += elem_size;
    } while (comp(first, pivot));

    if (first - elem_size == begin) {
        do {
            last -= elem_size;
        } while (first < last && !comp(last, pivot));
    } else {
        do {
            last -= elem_size;
        } while (!comp(last, pivot));
    }

    bool already_partitioned = first >= last;

    while (first < last) {
        memswap(first, last, elem_size);
        do {
            first += elem_size;
        } while (comp(first, pivot));
        do {
            last -= elem_size;
        } while (!comp(last, pivot));
    }

    char *pivot_pos = first - elem_size;

    memcpy(begin, pivot_pos, elem_size);
    memcpy(pivot_pos, pivot, elem_size);
    free(pivot);

    return (PartitionResult) {
        .first = pivot_pos,
        .second = already_partitioned,
    };
}

void heapify(char *a, int index, size_t elem_size, size_t length, comp_t comp)
{
    size_t left = 2 * index + 1;
    size_t right = 2 * index + 2;
    int large = right;

    if (left < length && comp(a + left * elem_size, a + large * elem_size)) {
        large = left;
    }

    if (right < length && comp(a + right * elem_size, a + large * elem_size)) {
        large = right;
    }

    if (large != index) {
        memswap(a + index * elem_size, a + large * elem_size, elem_size);

        heapify(a, large, elem_size, length, comp);
    }
}

void __heapsort(char *begin, char *end, size_t elem_size, comp_t comp)
{
    // make the heap
    size_t length = end - begin;
    for (int i = length / 2 - 1; i >= 0; i--) {
        heapify(begin, i, elem_size, length, comp);
    }

    for (int i = length - 1; i > 0; i--) {
        memswap(begin, begin + i * elem_size, elem_size);

        heapify(begin, 0, elem_size, i, comp);
    }
}

static inline void sort_loop(char *begin, char *end, size_t elem_size,
    comp_t comp, int bad_allowed, bool leftmost)
{
    while (true) {
        // Instead of dividing size by the elem_size, we multiply
        // INSERTION_SORT_THRESHOLD so it's easier to do addition to pointers
        size_t size = (end - begin) / elem_size;
        if (size < INSERTION_SORT_THRESHOLD) {
            if (leftmost) {
                insertion_sort(begin, end, elem_size, comp);
            } else {
                unguarded_insertion_sort(begin, end, elem_size, comp);
            }
            return;
        }

        size_t s2 = size / 2;
        if (size > NINTHER_THRESHOLD) {
            sort3(begin, begin + s2 * elem_size, end - elem_size, elem_size, comp);
            sort3(begin + elem_size, begin + (s2 - 1) * elem_size,
                end - 2 * elem_size, elem_size, comp);
            sort3(begin + 2 * elem_size, begin + (s2 + 1) * elem_size,
                end - 3 * elem_size, elem_size, comp);
            sort3(begin + (s2 - 1) * elem_size, begin + s2 * elem_size,
                begin + (s2 + 1) * elem_size, elem_size, comp);
            memswap(begin, begin + s2 * elem_size, elem_size);
        } else {
            sort3(begin + s2 * elem_size, begin, end - elem_size, elem_size, comp);
        }

        if (!leftmost && !comp(begin - elem_size, begin)) {
            begin = partition_left(begin, end, elem_size, comp) + elem_size;
            continue;
        }
        PartitionResult part_result = partition_right(begin, end, elem_size, comp);
        char *pivot_pos = part_result.first;
        bool already_partitioned = part_result.second;

        size_t l_size = (pivot_pos - begin) / elem_size;
        size_t r_size = (end - (pivot_pos + elem_size)) / elem_size;
        bool highly_balanced = l_size < size / 8 || r_size < size / 8;

        if (highly_balanced) {
            if (--bad_allowed == 0) {
                __heapsort(begin, end, elem_size, comp);
                return;
            }
            if (l_size >= INSERTION_SORT_THRESHOLD) {
                memswap(begin, begin + (l_size / 4) * elem_size, elem_size);
                memswap(pivot_pos - elem_size, pivot_pos - (l_size / 4) * elem_size,
                    elem_size);

                if (l_size > NINTHER_THRESHOLD) {
                    memswap(begin + elem_size, begin + (l_size / 4 + 1) * elem_size,
                        elem_size);
                    memswap(begin + 2 * elem_size, begin + (l_size / 4 + 2) * elem_size,
                        elem_size);
                    memswap(pivot_pos - 2 * elem_size,
                        pivot_pos - (l_size / 4 + 1) * elem_size, elem_size);
                    memswap(pivot_pos - 3 * elem_size,
                        pivot_pos - (l_size / 4 + 2) * elem_size, elem_size);
                }
            }

            if (r_size >= INSERTION_SORT_THRESHOLD) {
                memswap(pivot_pos + elem_size, pivot_pos + (1 + r_size / 4) * elem_size,
                    elem_size);
                memswap(end - elem_size, end - (r_size / 4) * elem_size, elem_size);

                if (r_size > NINTHER_THRESHOLD) {
                    memswap(pivot_pos + 2 * elem_size,
                        pivot_pos + (2 + r_size / 4) * elem_size, elem_size);
                    memswap(pivot_pos + 3 * elem_size,
                        pivot_pos + (3 + r_size / 4) * elem_size, elem_size);
                    memswap(end - 2 * elem_size, end - (1 + r_size / 4) * elem_size,
                        elem_size);
                    memswap(end - 3 * elem_size, end - (2 + r_size / 4) * elem_size,
                        elem_size);
                }
            }
        } else {
            if (already_partitioned && partial_insertion_sort(begin, pivot_pos, elem_size, comp) && partial_insertion_sort(pivot_pos + elem_size, end, elem_size, comp)) {
                return;
            }
        }

        sort_loop(begin, pivot_pos, elem_size, comp, bad_allowed, leftmost);
        begin = pivot_pos + elem_size;
        leftmost = false;
    }
}

void _heapsort(void *a, size_t length, size_t elem_size, comp_t comp)
{
    if (length == 0) {
        return;
    }
    __heapsort((char *)a, (char *)a + (length + 1) * elem_size, elem_size, comp);
}

void _sort(void *a, size_t length, size_t elem_size, comp_t comp)
{
    if (length == 0) {
        return;
    }
    sort_loop((char *)a, (char *)a + length * elem_size, elem_size, comp,
        log2(length), true);
}

/* Stringview */
stringview stringview_make(const char *ptr, size_t length)
{
    stringview result = { ptr, length };
    return result;
}

void stringview_trim_space_left(stringview *sv)
{
    size_t i = 0;
    while (i < sv->length && isspace(sv->str[i])) {
        i++;
    }
    sv->str += i;
    sv->length -= i;
}

void stringview_trim_space_right(stringview *sv)
{
    size_t i = 0;
    while (i < sv->length && isspace(sv->str[sv->length - i - 1])) {
        i++;
    }
    sv->length -= i;
}

void stringview_trim_space(stringview *sv)
{
    stringview_trim_space_left(sv);
    stringview_trim_space_right(sv);
}

void stringview_chop_left(stringview *sv, size_t size)
{
    if (size > sv->length) {
        size = sv->length;
    }
    sv->str += size;
    sv->length -= size;
}

void stringview_chop_right(stringview *sv, size_t size)
{
    if (size > sv->length) {
        size = sv->length;
    }
    sv->length -= size;
}

void stringview_chop_while(stringview *sv, bool (*predicate)(char))
{
    size_t i = 0;
    while (i < sv->length && predicate(sv->str[i])) {
        i++;
    }
    sv->str += i;
    sv->length -= i;
}

stringview stringview_take_while(stringview sv, bool (*predicate)(char))
{
    size_t i = 0;
    while (i < sv.length && predicate(sv.str[i])) {
        i++;
    }
    sv.length = i;
    return sv;
}

bool stringview_starts_with(stringview a, stringview b)
{
    size_t i = 0;
    if (a.length < b.length) {
        return false;
    }
    while (i < b.length) {
        if (a.str[i] != b.str[i]) {
            return false;
        }
        i++;
    }
    return true;
}

bool stringview_starts_with_cstr(stringview a, const char *b)
{
    size_t i = 0;
    size_t length = strlen(b);
    if (a.length < length) {
        return false;
    }
    while (i < length) {
        if (a.str[i] != b[i]) {
            return false;
        }
        i++;
    }
    return true;
}

bool stringview_ends_with(stringview a, stringview b)
{
    size_t i = 0;
    size_t j = 0;
    if (a.length < b.length) {
        return false;
    }
    i = a.length - b.length;
    while (i < a.length) {
        if (a.str[i] != b.str[j]) {
            return false;
        }
        i++;
        j++;
    }
    return true;
}

bool stringview_ends_with_cstr(stringview a, const char *b)
{
    size_t i = 0;
    size_t j = 0;
    size_t length = strlen(b);
    if (a.length < length) {
        return false;
    }
    i = a.length - length;
    while (i < a.length) {
        if (a.str[i] != b[j]) {
            return false;
        }
        i++;
        j++;
    }
    return true;
}

#ifdef TESTING
#    include <assert.h> // assert()
#    include <time.h>   // time()

void test_arrays(void)
{
    array(int) test_array = NULL;
    assert(array_length(test_array) == 0 && "Length of NULL array is different than 0");
    assert(array_capacity(test_array) == 0 && "Capacity of NULL array is different than 0");
    array_push(test_array, 10);
    assert(test_array[0] == 10 && "First element of array is different than 10");
    int popped_elem = array_pop(test_array);
    unused(popped_elem);
    assert(popped_elem == 10 && "Popped element of array is different than 10");
    array_push(test_array, 11);
    int last_elem = array_last(test_array);
    unused(last_elem);
    assert(last_elem == 11 && "Popped element of array is different than 10");
    assert(array_length(test_array) == 1 && "Length of array is different than 1");
    printf("Capacity: %lu\n", array_capacity(test_array));
    array_reserve(test_array, 50);
    printf("After Capacity: %lu\n", array_capacity(test_array));
    array_free(test_array);
    array(int) test_array2 = NULL;
    int n = 1024;
    for (int i = 0; i < n; ++i) {
        array_push(test_array2, i);
    }
    assert(array_length(test_array2) == (size_t)n);
    for (int i = 0; i < 50; ++i) {
        assert(test_array2[i] == i && "I-th element of array is different than i");
    }
    array_free(test_array2);
    assert(test_array == NULL && "Freed array isn't NULL");
    assert(array_length(test_array) == 0 && "Freed array length isn't 0");
}

bool cmp_test(void *a, void *b) { return *(int *)a < *(int *)b; }

void test_sort(void)
{
    array(int) vec = NULL;
    array_push(vec, 1);
    array_push(vec, 13);
    array_push(vec, 3);
    array_push(vec, 18);
    array_push(vec, 177);
    array_push(vec, 600);
    array_push(vec, 189);
    array_sort(vec, cmp_test);
    for (size_t i = 1; i < array_length(vec); i++) {
        assert(vec[i - 1] < vec[i]);
    }
    array_free(vec);

    srand(time(NULL));

    array(int) vec2 = NULL;
    for (int i = 0; i < 128; ++i) {
        array_push(vec2, rand());
    }
    array_sort(vec2, cmp_test);
    for (size_t i = 1; i < array_length(vec2); i++) {
        assert(vec2[i - 1] < vec2[i]);
    }
    array_free(vec2);

    array(int) vec3 = NULL;
    for (int i = 0; i < 4096; ++i) {
        array_push(vec3, rand());
    }
    array_sort(vec3, cmp_test);
    for (size_t i = 1; i < array_length(vec3); i++) {
        assert(vec3[i - 1] < vec3[i]);
    }
    array_free(vec3);
}
#endif // TESTING

#endif // UTIL_IMPL
