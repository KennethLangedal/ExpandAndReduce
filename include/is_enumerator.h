#pragma once

#include "hashset.h"
#include "heap.h"
#include "graph.h"

typedef struct is_enumerator
{
    int n;
    int _a;

    element *E;

    hashset64 *hs;
    heap *h;

    int *L_list, ln;
    struct is_enumerator *L_req;

    int *R_list, rn;
    struct is_enumerator *R_req;
} is_enumerator;

is_enumerator *is_enumerator_init(int *L_list, int ln, is_enumerator *L_req, int *R_list, int rn, is_enumerator *R_req);

void is_enumerator_free(is_enumerator *ie);

void is_enumerator_get_next(is_enumerator *ie, graph *g, uint8_t *V, int verbose);

void is_enumerator_print_solution(is_enumerator *ie, graph *g, int p);