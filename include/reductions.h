#pragma once

#include "graph.h"

int reduction_neighborhood(graph *g, int u, long long *offset);

int reduction_domination(graph *g, int u);

int reduction_dominates(graph *g, int u);

void edge_expansion(graph *g, int u, int v, int *T);

int reduction_domination_edge(graph *g, int u, int *T);

int reduction_add_edges(graph *g, int u, int *T);

// Assumes domination was called first!
int reduction_weight_transfer(graph *g, int u, long long *offset);

// Randomized attempt at reduction
int reduction_edge_expansion_search(graph *g, int u, int *T);

int reduction_edge_expansion_two_search(graph *g, int u, int *T);

int reduction_edge_expansion_neighborhood(graph *g, int u, int *T1, int *T2, long long *offset);

int reduction_edge_expansion_neighborhood_alt(graph *g, int u, int *T1, int *T2, long long *offset, int strict);