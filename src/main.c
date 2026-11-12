#define _GNU_SOURCE

#include "graph.h"
#include "algorithms.h"
#include "densify.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define MAX_ITERATIONS 128
#define MAX_IMPROVEMENT_LOOPS 8
#define STARTING_CEILING 4
#define INCREMENT_CEILING 4
#define FORCE_LIMIT 1024
#define FORCE_FACTOR 32

volatile sig_atomic_t tle = 0;
int folding = 1;
int _ba = 0;
int *B = NULL;

static int _cap_txy = 0;
static int *T = NULL;
static int *X = NULL;
static int *Y = NULL;

static densify_context *dctx = NULL;
static densify_options dopt;

static inline void ensure_txy(int needed)
{
    if (needed <= _cap_txy)
        return;
    if (_cap_txy == 0)
        _cap_txy = 100000;
    while (_cap_txy < needed)
        _cap_txy *= 2;
    T = realloc(T, sizeof(int) * _cap_txy);
    X = realloc(X, sizeof(int) * _cap_txy);
    Y = realloc(Y, sizeof(int) * _cap_txy);
}

void sig_handler(int signum)
{
    if (signum == SIGINT)
        tle = 1;
}

static inline void mark_included(graph *g, int u, int *I)
{
    if (u < g->org_n)
    {
        I[u] = 1;
    }
    else
    {
        mark_included(g, g->Pl[u], I);
        mark_included(g, g->Pr[u], I);
    }
}

// Restore graph g to time t, but remember any vertices that had reference count zero (removable).
static inline void restart(graph *g, int t, int ref_zero, int *T, int *I)
{
    int needed = g->ref_zero_count - ref_zero + 16;
    ensure_txy(needed);

    int n = 0;
    for (int i = ref_zero; i < g->ref_zero_count; i++)
        T[n++] = g->Ref_zero[i];

    graph_undo_changes(g, t);

    for (int i = 0; i < n; i++)
    {
        int u = T[i];
        I[u] = -1;
        if (g->A[u])
            graph_deactivate_vertex(g, u);
    }

    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u] || g->D[u] > 0)
            continue;

        mark_included(g, u, I);
        graph_reduce(g, u, 1);
    }
}

// A simple check for domination or edge_expansion + domination
static inline int edge_expansion_reduce(graph *g, int u)
{
    if (!g->A[u])
        return 0;

    int reduced = 0;
    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        if (g->W[v] > g->W[u])
            continue;

        int c = -1;
        if (set_is_subset_except_one_pluss_one(g->V[u], g->D[u], g->V[v], g->D[v], v, &c))
        {
            if (c >= 0)
                graph_edge_expansion(g, c, v);
            graph_deactivate_vertex(g, v);
            reduced++;
        }
    }

    return reduced;
}

// Returnes a list of missing edges in the neighborhood of u, or -1 if more than max
static inline int neighborhood_missing_links(graph *g, int u, int max, int *X, int *Y, int *T)
{
    if (!g->A[u])
        return -1;

    int n = 0;
    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        int m = set_minus_except_one(g->V[u], g->D[u], g->V[v], g->D[v], T, v);
        for (int j = 0; j < m; j++)
        {
            if (v > T[j])
                continue;

            X[n] = v;
            Y[n] = T[j];
            n++;
        }

        if (n > max)
            return -1;
    }

    return n;
}

// Expand and reduce the neighborhood of u
static inline int expand_and_reduce(graph *g, int u, int *X, int *Y, int *T, int max, int slack, int force)
{
    if (!g->A[u])
        return 0;

    if (edge_expansion_reduce(g, u))
        return 1;

    int t = g->t;
    int ref_n = g->nr;
    int zero_count = g->ref_zero_count;

    int n = neighborhood_missing_links(g, u, max, X, Y, T);

    // Clique fold
    if (n == 0)
    {
        graph_reduce(g, u, folding);
        return 1;
    }

    while (n > 0)
    {
        // Expand
        for (int i = 0; i < n; i++)
            graph_edge_expansion(g, X[i], Y[i]);

        // Reduce
        for (int i = 0; i < g->D[u]; i++)
        {
            int v = g->V[u][i];
            graph_reduce(g, v, folding);

            if (!g->A[u])
                break;
            if (!g->A[v])
                i--;
        }
        graph_reduce(g, u, folding);

        if (g->nr < ref_n ||                             // Always accept reductions
            (slack && g->ref_zero_count > zero_count) || // If slack, accept changes that makes permanent progress
            (slack && g->nr <= ref_n) ||                 // If slack, accept chenges that does not incease the size of the graph
            (force && !g->A[u]))                         // If force, accept the new graph if the stability number decrease
            return 1;

        if (!g->A[u])
            break;

        n = neighborhood_missing_links(g, u, max, X, Y, T);
    }

    graph_undo_changes(g, t);

    return 0;
}

// Expand and densify-reduce the neighborhood of u
static inline int expand_and_densify_reduce(densify_context *dctx, const densify_options *dopt, graph *g, int u, int *X, int *Y, int *T, int max)
{
    if (!g->A[u])
        return 0;

    int ref_n = g->nr;
    int n = neighborhood_missing_links(g, u, max, X, Y, T);

    // Clique fold
    if (n == 0)
    {
        graph_reduce(g, u, folding);
        return g->nr < ref_n;
    }

    while (n > 0 && !tle)
    {
        // Expand missing links
        for (int i = 0; i < n; i++)
            graph_edge_expansion(g, X[i], Y[i]);

        // Reduce using densify_dominate_reduce_graph
        densify_dominate_reduce_graph(dctx, g, dopt, NULL, NULL, NULL);

        if (g->nr < ref_n)
            return 1;

        if (!g->A[u])
            break;

        n = neighborhood_missing_links(g, u, max, X, Y, T);
        if (n == 0)
        {
            graph_reduce(g, u, folding);
            if (g->nr < ref_n)
                return 1;
            break;
        }
    }

    return g->nr < ref_n;
}

// Loop over the graph while trying to reduce
static inline void reduce_loop(graph *g, int n, int *X, int *Y, int *T, int sort, int max, int slack, int force)
{
    int count = 0, imp = 1;
    while (imp && count++ < n && !tle)
    {
        ensure_txy(g->n + max + 10000);

        if (g->nr > _ba)
        {
            _ba *= 2;
            B = realloc(B, sizeof(int) * _ba);
        }

        int m = 0;
        for (int u = 0; u < g->n; u++)
            if (g->A[u])
                B[m++] = u;

        if (sort)
            qsort_r(B, m, sizeof(int), compare_degrees, g->D);
        else
            shuffle_list(B, m);

        imp = 0;
        for (int i = 0; i < m && !tle; i++)
            imp |= expand_and_reduce(g, B[i], X, Y, T, max, slack, force);
    }
}

void recursive_print(FILE *f, graph *g, int u)
{
    if (u < g->org_n)
    {
        fprintf(f, "%d ", u + 1);
    }
    else
    {
        recursive_print(f, g, g->Pl[u]);
        recursive_print(f, g, g->Pr[u]);
    }
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        printf("Usage: %s [input graph] [reduced graph] [meta file]\n", argv[0]);
        printf("All inputs are file paths\n");
        return 0;
    }

    // struct sigaction sa = {};
    // sa.sa_handler = sig_handler;
    // sigaction(SIGINT, &sa, NULL);

    FILE *f = fopen(argv[1], "r");
    graph *g = graph_parse(f);
    fclose(f);

    int verbose = 1;

    printf("%s,%d,%d\n", argv[1], g->n, g->m);

    double t0 = get_wall_time();

    ensure_txy(g->m + g->n + 100000);

    int *I = malloc(sizeof(int) * g->org_n);
    for (int i = 0; i < g->org_n; i++)
        I[i] = 0;

    _ba = g->n;
    B = malloc(sizeof(int) * _ba);

    dopt = densify_default_options();
    dopt.verbose = 0;
    dopt.max_degree = 0; // No degree limits as requested
    dopt.max_edges_to_add = 0; // Unlimited per-vertex edges added as requested
    char *env_edges = getenv("DENSIFY_MAX_EDGES");
    if (env_edges)
        dopt.max_edges_to_add = atoi(env_edges);

    char *env_dist = getenv("DENSIFY_MAX_DIST");
    if (env_dist)
        dopt.max_distance = atoi(env_dist);

    dctx = densify_context_create(g->n);

    // Comparison Experiment: Reference Expand & Reduce vs Densify-Dominate Expand & Reduce
    int max_links = 8;
    char *env_links = getenv("MAX_MISSING_LINKS");
    if (env_links)
        max_links = atoi(env_links);

    printf("============================================================\n");
    printf("Starting Expand & Reduce Comparison Experiment (Single Pass)\n");
    printf("Graph: vertices=%d, edges=%d, offset=%lld, max_links=%d\n", g->nr, g->m, g->off, max_links);
    printf("Comparing Reference (expand_and_reduce) vs Densify (expand_and_densify_reduce)\n");
    printf("============================================================\n");
    fflush(stdout);

    int total_evaluated = 0;
    int both_yes = 0;
    int densify_only_yes = 0;
    int ref_only_yes = 0;
    int both_no = 0;

    reduce_loop(g, MAX_IMPROVEMENT_LOOPS, X, Y, T, 1, 32, 0, 0);

    while (1)
    {
    for (int u = 0; u < g->n && g->nr > 0 && !tle; u++)
    {
        if (!g->A[u])
            continue;

        total_evaluated++;
        int t_start = g->t;
        int nr_start = g->nr;

        // 1. Evaluate Reference Method
        int ref_res = expand_and_reduce(g, u, X, Y, T, max_links, 0, 0);
        int ref_nr = g->nr;
        int ref_yes = (ref_res && ref_nr < nr_start);

        // Undo reference changes
        graph_undo_changes(g, t_start);

        // 2. Evaluate Densify Method
        int densify_res = expand_and_densify_reduce(dctx, &dopt, g, u, X, Y, T, max_links);
        int densify_nr = g->nr;
        int densify_yes = (densify_res && densify_nr < nr_start);

        // 3. Keep decision:
        // When both succeed, keep whichever achieves the smaller nr (tied: prefer densify).
        const char *kept = "None";
        if (densify_yes && ref_yes)
        {
            both_yes++;
            if (densify_nr < ref_nr)
            {
                // Densify achieved smaller nr: keep Densify (already in Densify's state)
                kept = "Densify";
            }
            else if (ref_nr < densify_nr)
            {
                // Reference achieved smaller nr: undo Densify, re-run Reference
                graph_undo_changes(g, t_start);
                expand_and_reduce(g, u, X, Y, T, max_links, 0, 0);
                kept = "Ref";
            }
            else
            {
                // Tied on nr: keep Densify
                kept = "Densify";
            }
        }
        else if (densify_yes)
        {
            densify_only_yes++;
            kept = "Densify";
        }
        else if (ref_yes)
        {
            ref_only_yes++;
            graph_undo_changes(g, t_start);
            expand_and_reduce(g, u, X, Y, T, max_links, 0, 0);
            kept = "Ref";
        }
        else
        {
            both_no++;
            graph_undo_changes(g, t_start);
            kept = "None";
        }

        printf("[Vertex %4d] Ref: %-3s (nr: %4d) | Densify: %-3s (nr: %4d) -> Kept: %-7s | Graph: nr=%d, m=%d, off=%lld, zero=%d\n",
               u + 1,
               ref_yes ? "YES" : "NO", ref_nr,
               densify_yes ? "YES" : "NO", densify_nr,
               kept,
               g->nr, g->m, g->off, g->ref_zero_count);
        fflush(stdout);
    }   
    }

    double t1 = get_wall_time();

    printf("\n============================================================\n");
    printf("Comparison Experiment Summary (Single Pass over Vertices):\n");
    printf("============================================================\n");
    printf("Total vertices evaluated:    %d\n", total_evaluated);
    printf("Both YES (both reduced):     %d\n", both_yes);
    printf("Densify ONLY YES (Ref NO):   %d  <-- New reductions unlocked by densify!\n", densify_only_yes);
    printf("Ref ONLY YES (Densify NO):   %d\n", ref_only_yes);
    printf("Both NO (both failed):       %d\n", both_no);
    printf("------------------------------------------------------------\n");
    printf("Total kept reductions:       %d (Densify kept: %d, Ref kept: %d)\n",
           densify_only_yes + both_yes + ref_only_yes, densify_only_yes + both_yes, ref_only_yes);
    printf("Final graph state:           vertices=%d, edges=%d, offset=%lld\n", g->nr, g->m, g->off);
    printf("Total elapsed time:          %6.2fs\n", t1 - t0);
    printf("============================================================\n");
    fflush(stdout);

    // Store reduced graph if output files provided
    if (argc >= 4)
    {
        int *RM = malloc(sizeof(int) * g->n);
        int id = 0;
        for (int u = 0; u < g->n; u++)
        {
            if (!g->A[u])
                continue;
            RM[u] = id++;
        }

        f = fopen(argv[2], "w");
        if (f)
        {
            fprintf(f, "%d %d 10\n", id, g->m);
            for (int u = 0; u < g->n; u++)
            {
                if (!g->A[u])
                    continue;
                fprintf(f, "%lld ", g->W[u]);
                for (int i = 0; i < g->D[u]; i++)
                    fprintf(f, "%d ", RM[g->V[u][i]] + 1);
                fprintf(f, "\n");
            }
            fclose(f);
        }

        f = fopen(argv[3], "w");
        if (f)
        {
            fprintf(f, "%% Reduced Graph Metadata\n");
            fprintf(f, "included: 0\nexcluded: %d\nremaining: %d\n\n", g->org_n - g->nr, g->nr);
            fclose(f);
        }

        free(RM);
    }

    free(T);
    free(X);
    free(Y);
    free(I);
    free(B);

    if (dctx)
        densify_context_destroy(dctx);

    graph_free(g);

    return 0;
}
