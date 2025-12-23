#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct
{
    int a, b;
    int n;
    long long wa, wb;
    long long w;
} element;

typedef struct
{
    uint64_t n;
    uint64_t _a;

    element *H;
} heap;

static inline heap *heap_init(uint64_t capacity)
{
    heap *h = malloc(sizeof(heap));

    h->n = 0;
    h->_a = capacity;

    h->H = malloc(sizeof(element) * h->_a);

    return h;
}

static inline void heap_free(heap *h)
{
    free(h->H);

    free(h);
}

static inline void heap_pop(heap *h)
{
    if (h->n == 0)
        return;

    h->H[0] = h->H[h->n - 1];
    h->n--;

    uint64_t i = 0;

    while (1)
    {
        uint64_t left = 2 * i + 1;
        uint64_t right = 2 * i + 2;
        uint64_t largest = i;

        if (left < h->n && (h->H[left].w > h->H[largest].w ||
                            (h->H[left].w == h->H[largest].w && h->H[left].n < h->H[largest].n)))
            largest = left;

        if (right < h->n && (h->H[right].w > h->H[largest].w ||
                             (h->H[right].w == h->H[largest].w && h->H[right].n < h->H[largest].n)))
            largest = right;

        if (largest == i)
            break;

        element t = h->H[i];
        h->H[i] = h->H[largest];
        h->H[largest] = t;

        i = largest;
    }
}

static inline void heap_insert(heap *h, element e)
{
    if (h->n == h->_a)
    {
        h->_a *= 2;
        h->H = realloc(h->H, sizeof(element) * h->_a);
    }

    uint64_t i = h->n;
    h->H[i] = e;
    h->n++;

    while (1)
    {
        uint64_t parent = (i - 1) / 2;

        if (i == 0 || (h->H[parent].w > e.w ||
                       (h->H[parent].w == e.w && h->H[parent].n <= e.n)))
            break;

        h->H[i] = h->H[parent];
        h->H[parent] = e;

        i = parent;
    }
}