#pragma once

#include "graph.h"

#define MAX_ELEMENTS 100000000
#define MAX_EDGES (MAX_ELEMENTS * 10)
#define MAX_REDUCED 10000
#define SIZE_OFFSET 10

typedef struct cliques
{
    int n;
    long long ub;
    int **C, *S;

    int *FM;
    graph *gc;

    // Temporary storage for faster merging
    int mn, mnr, mnd, mmd, c1, c2;
    int *V, *R, *L, *X, *Y, *O;
    long long *W;
    int *E;
} cliques;

cliques *cliques_init(graph *g);

void cliques_free(cliques *c);

void cliques_sort(cliques *c, graph *g);

int cliques_merge_prep(cliques *c, graph *g, int c1, int c2, int stop);

void cliques_merge(cliques *c, graph *g);

// void cliques_merge_alt(cliques *c, graph *g, int c1, int c2);