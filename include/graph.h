#pragma once

#include <stdio.h>

typedef struct log_action
{
    int V[2];
    long long w;
} log_action;

typedef struct graph
{
    int n;        // Number of vertices
    int m;        // Number of edges
    int nr;       // Number remaining vertices
    int **V;      // Adjacency lists
    int *D;       // Degrees
    long long *W; // Vertex weights
    int *A;       // Active flags

    int _a;  // Number of vertices allocated memory for
    int *_A; // Allocated memory per neighborhood

    int *Pl, *Pr; // Parent pointers

    int t;            // Log counter
    int _al;          // Log allocated size
    char *_Log_op;    // Log operation
    log_action *_Log; // Log data
} graph;

graph *graph_init();

graph *graph_parse(FILE *f);

graph *graph_copy(graph *g);

void graph_free(graph *g);

void graph_add_vertex(graph *g, long long w, int pl, int pr);

void graph_add_vertex_full(graph *g, long long w, int pl, int pr, int *N, int d);

void graph_add_edge(graph *g, int u, int v);

void graph_remove_edge(graph *g, int u, int v);

void graph_deactivate_vertex(graph *g, int u);

void graph_change_vertex_weight(graph *g, int u, long long w);

void graph_undo_changes(graph *g, int t);

// Helper functions

int graph_is_neighbor(graph *g, int u, int v);