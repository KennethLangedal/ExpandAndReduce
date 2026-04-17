#pragma once

#include <stdio.h>
#include "signal.h"

typedef struct log_action_struct
{
    int u, v;
} log_action;

typedef struct graph_struct
{
    int n;         // Number of vertices
    int m;         // Number of edges
    int nr;        // Number remaining vertices
    long long off; // Solution offset
    int **V;       // Adjacency lists
    int *D;        // Degrees
    long long *W;  // Vertex weights
    int *A;        // Active flags

    int _a;  // Number of vertices allocated memory for
    int *_A; // Allocated memory per neighborhood

    int *Pl, *Pr; // Parent pointers

    int org_n;      // Original number of vertices
    int *Ref_count; // Number of active vertices for each original vertex

    int ref_zero_count; // Number of vertices with ref-count zero
    int *Ref_zero;      // List of vertices with ref-count zero;

    int t;            // Log counter
    int _al;          // Log allocated size
    char *_Log_op;    // Log operation
    log_action *_Log; // Log data
} graph;

graph *graph_parse(FILE *f);

graph *graph_copy(graph *g);

void graph_free(graph *g);

void graph_add_edge(graph *g, int u, int v);

void graph_edge_expansion(graph *g, int u, int v);

void graph_deactivate_vertex(graph *g, int u);

void graph_undo_changes(graph *g, int t);

// Helper functions

int graph_is_neighbor(graph *g, int u, int v);

int graph_reduce(graph *g, int u, int fold);