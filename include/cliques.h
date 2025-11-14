#pragma once

#include "graph.h"

typedef struct
{
    int n;
    long long ub;
    int **C, *D;

    int *FM;
    int **V, **EW;
} cliques;

cliques *cliques_init(graph *g);

void cliques_free(cliques *c);

void cliques_merge(cliques *c, graph *g, int c1, int c2);
