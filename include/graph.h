#pragma once

#include <stdio.h>

typedef struct
{
    int n, m, nr; // Number of vertices, edges, and remaining vertices
    int **V, *D;  // Adjacency lists and degrees
    long long *W; // Vertex weights
    int *A;       // Active flags

    int _a;  // Number of vertices allocated memory for
    int *_A; // Allocated memory per neighborhood

    int *p1, *p2;
} graph;

graph *graph_parse(FILE *f);

void graph_free(graph *g);

graph *graph_copy(graph *g);

void graph_add_vertex(graph *g, long long w, int p1, int p2);

void graph_add_edge(graph *g, int u, int v);

void graph_remove_edge(graph *g, int u, int v);

void graph_deactivate_vertex(graph *g, int u);

void graph_change_vertex_weight(graph *g, int u, long long w);

// Helper functions

int graph_is_neighbor(graph *g, int u, int v);