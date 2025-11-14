#pragma once

#include <time.h>

static inline double get_wtime()
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    return (double)tp.tv_sec + ((double)tp.tv_nsec / 1e9);
}

static inline int compare_ids(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

// Returns the position of the first element >= to x
static inline int lower_bound(const int *A, int n, int x)
{
    const int *s = A;
    while (n > 1)
    {
        int h = n / 2;
        s += (s[h - 1] < x) * h;
        n -= h;
    }
    s += (n == 1 && s[0] < x);
    return s - A;
}

// Test if A and B are equal
static inline int set_is_equal(const int *A, int a, const int *B, int b)
{
    if (b != a)
        return 0;

    for (int i = 0; i < a; i++)
    {
        if (A[i] != B[i])
            return 0;
    }

    return 1;
}

// Test if A is a subset of B
static inline int set_is_subset(const int *A, int a, const int *B, int b)
{
    if (b < a)
        return 0;

    int i = 0, j = 0;
    while (i < a && j < b && (a - i <= b - j))
    {
        if (A[i] == B[j])
        {
            i++;
            j++;
        }
        else if (A[i] > B[j])
        {
            j++;
        }
        else
        {
            return 0;
        }
    }

    return i == a;
}

// Test if A is a subset of B, ignoring x from A
static inline int set_is_subset_except_one(const int *A, int a, const int *B, int b, int x)
{
    if (b < a - 1)
        return 0;

    int i = 0, j = 0;
    while (i < a && j < b)
    {
        if (A[i] == B[j])
        {
            i++;
            j++;
        }
        else if (A[i] > B[j])
        {
            j++;
        }
        else if (A[i] == x)
        {
            i++;
        }
        else
        {
            return 0;
        }
    }

    if (i < a && A[i] == x)
        i++;

    return i == a;
}

// Test if A is a subset of B, ignoring x and y from A
static inline int set_is_subset_except_two(const int *A, int a, const int *B, int b, int x, int y)
{
    if (b < a - 2)
        return 0;

    int i = 0, j = 0;
    while (i < a && j < b)
    {
        if (A[i] == B[j])
        {
            i++;
            j++;
        }
        else if (A[i] > B[j])
        {
            j++;
        }
        else if (A[i] == x || A[i] == y)
        {
            i++;
        }
        else
        {
            return 0;
        }
    }

    while (i < a && (A[i] == x || A[i] == y))
        i++;

    return i == a;
}

// Computes A \ (B U {x}) and stores the result in C, returns the size of C
static inline int set_minus_except_one(const int *A, int a, const int *B, int b, int *C, int x)
{
    int n = 0;
    int i = 0, j = 0;
    while (i < a && j < b)
    {
        if (A[i] == B[j])
        {
            i++;
            j++;
        }
        else if (A[i] > B[j])
        {
            j++;
        }
        else
        {
            if (A[i] != x)
                C[n++] = A[i];
            i++;
        }
    }

    while (i < a)
    {
        if (A[i] != x)
            C[n++] = A[i];
        i++;
    }

    return n;
}