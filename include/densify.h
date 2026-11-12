#pragma once

#include "graph.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct densify_options_struct
{
    int max_distance;      // Max distance to explore (2 or 3, default: 3)
    int max_degree;        // Ignore vertices with degree > max_degree (0 = unlimited)
    int max_edges_to_add;  // Max edges to add per vertex attempt (0 = unlimited)
    int max_passes;        // Max passes over vertices in densify_reduce_graph (default: 1)
    int keep_edges;        // 1 = keep all edges found (default), 0 = undo edges if vertex not dominated
    int use_heuristic;     // 1 = use fast greedy + 1-swap heuristic (default), 0 = HiGHS ILP
    int num_restarts;      // Number of random restarts for heuristic (default: 4)
    double time_limit_sec; // HiGHS time limit per ILP solve in seconds (default: 0.1)
    int verbose;           // Logging verbosity (0 = quiet, 1 = informative)
} densify_options;

densify_options densify_default_options(void);

// Opaque context holding reusable buffers and the HiGHS instance for zero-allocation calls
typedef struct densify_context_struct densify_context;

densify_context *densify_context_create(int initial_n);
void densify_context_destroy(densify_context *ctx);

// Fast greedy + 1-swap heuristic with random restarts to test if pair (u, v) can be densified
int densify_test_pair_heuristic(densify_context *ctx, graph *g, int u, int v, const densify_options *opt);

// Context-aware functions with zero dynamic allocations during processing
int densify_test_pair_ctx(densify_context *ctx, graph *g, int u, int v, const densify_options *opt);
int densify_try_add_edge_ctx(densify_context *ctx, graph *g, int u, int v, const densify_options *opt);
int densify_reduce_vertex_ctx(densify_context *ctx, graph *g, int u, const densify_options *opt);

// Checks if vertex u is dominated by any neighbor (w(v) >= w(u) and N(v) \ {u} \subseteq N(u) \ {v}).
// If so, deactivates u via graph_deactivate_vertex(g, u) and returns 1. Otherwise returns 0.
int densify_is_dominated(graph *g, int u);

// Convenience wrapper functions
int densify_test_pair(void *highs, graph *g, int u, int v, const densify_options *opt);
int densify_try_add_edge(void *highs, graph *g, int u, int v, const densify_options *opt);
int densify_reduce_vertex(void *highs, graph *g, int u, const densify_options *opt);
int densify_reduce_graph(graph *g, const densify_options *opt);
int densify_graph(graph *g, const densify_options *opt);

// Runs a randomized densify and reduce loop until fixed point.
// In each round, vertices in B are shuffled, tested for distance 2/3 densification edges (which are kept),
// and eagerly/fully reduced until no more edges can be added and no more vertices can be reduced.
int densify_and_reduce_loop(densify_context *ctx, graph *g, int *B, int folding, const densify_options *opt);

// Runs alternating expand (densify without eager domination, randomized vertex order)
// and reduce (domination/folding by vertex ID until fixed point) loops until a pass where nothing changes.
int densify_alternating_phase(densify_context *ctx, graph *g, int folding, const densify_options *opt);

// Extended pair densification test where vertex u has temporary neighborhood N_prime (count: N_prime_count)
int densify_test_pair_extended(densify_context *ctx, graph *g, int u, const int *N_prime, int N_prime_count, int v, const densify_options *opt);

// Tests and reduces vertex u using the temporary one-sided densification and domination framework.
// Returns 1 if u was dominated and deactivated, 0 otherwise.
// If dominated, out_dom_type is set to 1 (single-vertex domination) or 2 (independent-set domination).
int densify_dominate_reduce_vertex(densify_context *ctx, graph *g, int u, const densify_options *opt, int *out_dom_type);

// Runs the fixed-point outer loop over active vertices (0..n-1) until convergence.
int densify_dominate_reduce_graph(densify_context *ctx, graph *g, const densify_options *opt, int *out_single_dom, int *out_set_dom, int *out_heavy_dom);

#ifdef __cplusplus
}
#endif
