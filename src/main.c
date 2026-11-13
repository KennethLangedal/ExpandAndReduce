#define _GNU_SOURCE

#include "graph.h"
#include "algorithms.h"
#include "densify.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#ifdef _OPENMP
#include <omp.h>
#endif

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

        // Reduce locally around u using two-tier worklist (graph_reduce + densify)
        densify_dominate_reduce_local(dctx, g, u, folding, ref_n, dopt);

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

// Full pass of simple domination (no folding except degree-0) and heavy-vertex inclusion until fixed point
static inline int simple_reduce_loop(graph *g)
{
    int total = 0;
    int changed = 1;
    while (changed && !tle)
    {
        changed = 0;
        for (int u = 0; u < g->n && !tle; u++)
        {
            if (!g->A[u])
                continue;

            if (g->D[u] == 0)
            {
                graph_reduce(g, u, 1);
                changed = 1;
                total++;
                continue;
            }

            long long nbr_weight = 0;
            for (int i = 0; i < g->D[u]; i++)
                nbr_weight += g->W[g->V[u][i]];

            if (g->W[u] >= nbr_weight)
            {
                while (g->D[u] > 0)
                    graph_deactivate_vertex(g, g->V[u][0]);
                graph_reduce(g, u, 1);
                changed = 1;
                total++;
                continue;
            }

            if (graph_reduce(g, u, 0))
            {
                changed = 1;
                total++;
            }
        }
    }
    return total;
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

// Loop over the graph trying expand_and_densify_reduce on each active vertex.
// Uses OpenMP speculative batching (parallel check on thread-local graph copies,
// sequential commit on g) when OpenMP is enabled and the active set is non-trivial.
static inline void densify_reduce_loop(densify_context *dctx, const densify_options *dopt, graph *g, int n_loops, int *X, int *Y, int *T, int sort, int max)
{
    int nthreads = 1;
#ifdef _OPENMP
    nthreads = omp_get_max_threads();
    if (nthreads < 1)
        nthreads = 1;
#endif

    int buf_cap = g->n + max + 10000;
    if (buf_cap < _cap_txy)
        buf_cap = _cap_txy;

    densify_context **t_dctx = malloc(sizeof(densify_context *) * nthreads);
    int **t_X = malloc(sizeof(int *) * nthreads);
    int **t_Y = malloc(sizeof(int *) * nthreads);
    int **t_T = malloc(sizeof(int *) * nthreads);
    graph **g_arr = calloc(nthreads, sizeof(graph *));

    t_dctx[0] = dctx;
    t_X[0] = X;
    t_Y[0] = Y;
    t_T[0] = T;
    for (int t = 1; t < nthreads; t++)
    {
        t_dctx[t] = densify_context_create(g->n);
        t_X[t] = malloc(sizeof(int) * buf_cap);
        t_Y[t] = malloc(sizeof(int) * buf_cap);
        t_T[t] = malloc(sizeof(int) * buf_cap);
    }

    int batch_size = nthreads * 2;
    if (batch_size < 16)
        batch_size = 16;
    int *won = malloc(sizeof(int) * batch_size);

    int count = 0, imp = 1;
    while (imp && count++ < n_loops && g->nr > 0 && !tle)
    {
        ensure_txy(g->n + max + 10000);
        if (_cap_txy > buf_cap)
        {
            buf_cap = _cap_txy;
            t_X[0] = X;
            t_Y[0] = Y;
            t_T[0] = T;
            for (int t = 1; t < nthreads; t++)
            {
                t_X[t] = realloc(t_X[t], sizeof(int) * buf_cap);
                t_Y[t] = realloc(t_Y[t], sizeof(int) * buf_cap);
                t_T[t] = realloc(t_T[t], sizeof(int) * buf_cap);
            }
        }

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

        if (nthreads == 1 || m < 1000)
        {
            for (int i = 0; i < m && g->nr > 0 && !tle; i++)
            {
                printf("\r  [Densify Pass %d] %d/%d | nr=%d, m=%d, off=%lld, zero=%d   ",
                       count, i + 1, m, g->nr, g->m, g->off, g->ref_zero_count);
                fflush(stdout);

                int u = B[i];
                if (!g->A[u])
                    continue;

                int t_start = g->t;
                int nr_start = g->nr;
                int res = expand_and_densify_reduce(dctx, dopt, g, u, X, Y, T, max);
                if (res && g->nr < nr_start)
                    imp = 1;
                else
                    graph_undo_changes(g, t_start);
            }
        }
        else
        {
#ifdef _OPENMP
            #pragma omp parallel for num_threads(nthreads)
            for (int t = 0; t < nthreads; t++)
            {
                if (g_arr[t])
                    graph_free(g_arr[t]);
                g_arr[t] = graph_copy(g);
            }
#endif
            for (int base = 0; base < m && g->nr > 0 && !tle; base += batch_size)
            {
                int end = (base + batch_size < m) ? (base + batch_size) : m;
                int b_len = end - base;

#ifdef _OPENMP
                #pragma omp parallel for schedule(dynamic, 1) num_threads(nthreads)
#endif
                for (int k = 0; k < b_len; k++)
                {
#ifdef _OPENMP
                    int tid = omp_get_thread_num();
#else
                    int tid = 0;
#endif
                    graph *gt = g_arr[tid];
                    int u = B[base + k];
                    if (!gt->A[u])
                    {
                        won[k] = 0;
                        continue;
                    }
                    int t_saved = gt->t;
                    int nr_saved = gt->nr;
                    int res = expand_and_densify_reduce(t_dctx[tid], dopt, gt, u, t_X[tid], t_Y[tid], t_T[tid], max);
                    won[k] = (res && gt->nr < nr_saved);
                    graph_undo_changes(gt, t_saved);
                }

                int batch_changed = 0;
                for (int k = 0; k < b_len && g->nr > 0 && !tle; k++)
                {
                    if (!won[k])
                        continue;
                    int u = B[base + k];
                    if (!g->A[u])
                        continue;
                    int t_start = g->t;
                    int nr_start = g->nr;
                    int res = expand_and_densify_reduce(dctx, dopt, g, u, X, Y, T, max);
                    if (res && g->nr < nr_start)
                    {
                        imp = 1;
                        batch_changed = 1;
                    }
                    else
                    {
                        graph_undo_changes(g, t_start);
                    }
                }

                printf("\r  [Densify Pass %d] %d/%d | nr=%d, m=%d, off=%lld, zero=%d   ",
                       count, end, m, g->nr, g->m, g->off, g->ref_zero_count);
                fflush(stdout);

                if (batch_changed && end < m && g->nr > 0 && !tle)
                {
#ifdef _OPENMP
                    #pragma omp parallel for num_threads(nthreads)
                    for (int t = 0; t < nthreads; t++)
                    {
                        if (g_arr[t])
                            graph_free(g_arr[t]);
                        g_arr[t] = graph_copy(g);
                    }
#endif
                }
            }
        }

        printf("\r  [Densify Pass %d] %d/%d | nr=%d, m=%d, off=%lld, zero=%d   \n",
               count, m, m, g->nr, g->m, g->off, g->ref_zero_count);
        fflush(stdout);
    }

    for (int t = 0; t < nthreads; t++)
    {
        if (g_arr[t])
            graph_free(g_arr[t]);
    }
    for (int t = 1; t < nthreads; t++)
    {
        densify_context_destroy(t_dctx[t]);
        free(t_X[t]);
        free(t_Y[t]);
        free(t_T[t]);
    }
    free(t_dctx);
    free(t_X);
    free(t_Y);
    free(t_T);
    free(g_arr);
    free(won);
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
    // folding = 0;

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
    dopt.max_distance = 3;
    dopt.max_degree = 0;       // No degree limits as requested
    dopt.max_edges_to_add = 0; // Unlimited per-vertex edges added as requested
    char *env_edges = getenv("DENSIFY_MAX_EDGES");
    if (env_edges)
        dopt.max_edges_to_add = atoi(env_edges);

    char *env_dist = getenv("DENSIFY_MAX_DIST");
    if (env_dist)
        dopt.max_distance = atoi(env_dist);

    dctx = densify_context_create(g->n);

    // Comparison Experiment: Reference Expand & Reduce vs Densify-Dominate Expand & Reduce
    int max_links = 32;
    char *env_links = getenv("MAX_MISSING_LINKS");
    if (env_links)
        max_links = atoi(env_links);

    printf("============================================================\n");
    printf("Starting Expand & Reduce + Densify Pipeline\n");
    printf("Graph: vertices=%d, edges=%d, offset=%lld\n", g->nr, g->m, g->off);
    printf("============================================================\n");
    fflush(stdout);

    // Initial expansion-free, folding-free (except deg-0) passes:
    // 1. Simple domination + heavy-vertex inclusion to fixed point
    int init_std = simple_reduce_loop(g);
    printf("[Init Simple]  reduced=%d -> nr=%d, m=%d, off=%lld (%.2fs)\n",
           init_std, g->nr, g->m, g->off, get_wall_time() - t0);
    fflush(stdout);

    // 2. Densify domination with distance 2
    if (g->nr > 0 && !tle)
    {
        dopt.max_distance = 2;
        int d2 = densify_dominate_reduce_graph(dctx, g, &dopt, NULL, NULL, NULL);
        printf("[Init Dist-2]  reduced=%d -> nr=%d, m=%d, off=%lld (%.2fs)\n",
               d2, g->nr, g->m, g->off, get_wall_time() - t0);
        fflush(stdout);
    }

    // Keep initial expansion-free reductions permanently across restarts
    int t_base = g->t;
    int ref_zero_base = g->ref_zero_count;

    // Stage 1: Fast non-densify-expand loop (reduce_loop + expansion-free dist-2 densify)
    int it = 0;
    int consecutive_no_zero = 0;
    while (g->nr > 0 && !tle)
    {
        folding = it % 2;
        int prev_zero = g->ref_zero_count;

        reduce_loop(g, MAX_IMPROVEMENT_LOOPS, X, Y, T, 1, 64, 0, 0);
        if (g->nr == 0 || tle)
            break;

        simple_reduce_loop(g);
        dopt.max_distance = 2;
        densify_dominate_reduce_graph(dctx, g, &dopt, NULL, NULL, NULL);

        printf("[Stage 1 Iter %2d] folding=%d -> nr=%d, m=%d, off=%lld, zero=%d (%.2fs)\n",
               it, folding, g->nr, g->m, g->off, g->ref_zero_count, get_wall_time() - t0);
        fflush(stdout);

        if (g->nr == 0 || tle)
            break;

        if (g->ref_zero_count == prev_zero)
            consecutive_no_zero++;
        else
            consecutive_no_zero = 0;

        if (consecutive_no_zero >= 2)
            break;

        restart(g, t_base, ref_zero_base, T, I);
        it++;
    }

    // Stage 2: Run expand_and_densify_reduce on the reduced Stage 1 kernel
    int it2 = 0;
    if (g->nr > 0 && !tle)
    {
        int t_stage2 = g->t;
        int zero_stage2 = g->ref_zero_count;
        int no_zero_2 = 0;

        while (g->nr > 0 && !tle)
        {
            folding = it2 % 2;
            int prev_zero = g->ref_zero_count;

            if (it2 > 0)
            {
                reduce_loop(g, MAX_IMPROVEMENT_LOOPS, X, Y, T, 1, 64, 0, 0);
                if (g->nr == 0 || tle)
                    break;
            }

            dopt.max_distance = 2;
            densify_reduce_loop(dctx, &dopt, g, MAX_IMPROVEMENT_LOOPS, X, Y, T, 1, max_links);

            printf("[Stage 2 Iter %2d] folding=%d -> nr=%d, m=%d, off=%lld, zero=%d (%.2fs)\n",
                   it2, folding, g->nr, g->m, g->off, g->ref_zero_count, get_wall_time() - t0);
            fflush(stdout);

            if (g->nr == 0 || tle)
                break;

            if (g->ref_zero_count == prev_zero)
                no_zero_2++;
            else
                no_zero_2 = 0;

            if (no_zero_2 >= 2)
                break;

            restart(g, t_stage2, zero_stage2, T, I);
            it2++;
        }
    }

    double t1 = get_wall_time();

    printf("\n============================================================\n");
    printf("Final Summary:\n");
    printf("============================================================\n");
    printf("Iterations:                  Stage 1=%d, Stage 2=%d\n", it, it2);
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
