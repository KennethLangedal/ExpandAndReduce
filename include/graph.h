#pragma once

#include <stdio.h>

typedef struct
{
    int n;          // Number of vertices
    int m;          // Number of edges
    int nr;         // Number remaining vertices
    int **V;        // Adjacency lists
    int *D;         // Degrees
    long long *W;   // Vertex weights
    long long **EW; // Edge weights
    int *A;         // Active flags

    int _a;  // Number of vertices allocated memory for
    int *_A; // Allocated memory per neighborhood

    int *Pl, *Pr; // Parent pointers
} graph;

graph *graph_init(int edge_weights);

graph *graph_parse(FILE *f);

graph *graph_copy(graph *g);

void graph_free(graph *g);

void graph_enable_edge_weights(graph *g);

void graph_disable_edge_weights(graph *g);

void graph_add_vertex(graph *g, long long w, int pl, int pr);

void graph_add_edge(graph *g, int u, int v, long long w);

void graph_remove_edge(graph *g, int u, int v);

void graph_deactivate_vertex(graph *g, int u);

void graph_change_vertex_weight(graph *g, int u, long long w);

void graph_increase_edge_weight(graph *g, int u, int v, long long w);

// Helper functions

int graph_is_neighbor(graph *g, int u, int v);