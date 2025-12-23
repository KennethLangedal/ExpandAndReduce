#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct
{
    uint64_t n;  // Number of elements
    uint64_t _a; // Allocated space

    uint64_t *K; // Keys
    uint8_t *V;  // Used
} hashset64;

static inline uint64_t hash64(uint64_t x)
{
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static inline hashset64 *hashset64_init(uint64_t capacity)
{
    hashset64 *hs = malloc(sizeof(hashset64));

    hs->n = 0;
    hs->_a = capacity;

    hs->K = calloc(hs->_a, sizeof(uint64_t));
    hs->V = calloc(hs->_a, sizeof(uint8_t));

    return hs;
}

static inline void hashset64_free(hashset64 *hs)
{
    free(hs->K);
    free(hs->V);

    free(hs);
}

static inline void hashset64_resize(hashset64 *hs)
{
    hashset64 *hs_new = hashset64_init(hs->_a * 2);

    for (uint64_t i = 0; i < hs->_a; i++)
    {
        if (!hs->V[i])
            continue;

        uint64_t key = hs->K[i];
        uint64_t idx = hash64(key) % hs_new->_a;

        while (hs_new->V[idx])
            idx = (idx + 1) % hs_new->_a;

        hs_new->V[idx] = 1;
        hs_new->K[idx] = key;
        hs_new->n++;
    }

    hashset64 t = *hs;
    *hs = *hs_new;
    *hs_new = t;

    hashset64_free(hs_new);
}

static inline void hashset64_insert(hashset64 *hs, uint64_t key)
{
    if (hs->n * 2 >= hs->_a)
        hashset64_resize(hs); // load factor 0.5

    uint64_t idx = hash64(key) % hs->_a;

    while (hs->V[idx])
    {
        if (hs->K[idx] == key)
            return;               // already present
        idx = (idx + 1) % hs->_a; // linear probe
    }

    hs->V[idx] = 1;
    hs->K[idx] = key;
    hs->n++;
}

static inline int hashset64_contains(hashset64 *hs, uint64_t key)
{
    uint64_t idx = hash64(key) % hs->_a;

    while (hs->V[idx])
    {
        if (hs->K[idx] == key)
            return 1;
        idx = (idx + 1) % hs->_a;
    }

    return 0;
}