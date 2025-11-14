#pragma once

#include "graph.h"

typedef struct
{
    long long n, ub;
    int **C, *D;
} cliques;

cliques *cliques_init(graph *g);

void cliques_free(cliques *c);

void cliques_merge(cliques *c, graph *g, int c1, int c2);
