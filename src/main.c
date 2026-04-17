#define _GNU_SOURCE

#include "graph.h"
#include "algorithms.h"

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

void sig_handler(int signum)
{
    if (signum == SIGINT)
        tle = 1;
}

// Restore graph g to time t, but remember any vertices that had reference count zero (removable).
static inline void restart(graph *g, int t, int ref_zero, int *T, int *I)
{
    int n = 0;
    for (int i = ref_zero; i < g->ref_zero_count; i++)
        T[n++] = g->Ref_zero[i];

    graph_undo_changes(g, t);

    for (int i = 0; i < n; i++)
    {
        int u = T[i];
        I[u] = -1;
        graph_deactivate_vertex(g, u);
    }

    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u] || g->D[u] > 0)
            continue;

        I[u] = 1;
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

// Loop over the graph while trying to reduce
static inline void reduce_loop(graph *g, int n, int *O, int *X, int *Y, int *T, int sort, int max, int slack, int force)
{
    int count = 0, imp = 1;
    while (imp && count++ < n && !tle)
    {
        int m = 0;
        for (int u = 0; u < g->n; u++)
            if (g->A[u])
                O[m++] = u;

        if (sort)
            qsort_r(O, m, sizeof(int), compare_degrees, g->D);
        else
            shuffle_list(O, m);

        imp = 0;
        for (int i = 0; i < m; i++)
            imp |= expand_and_reduce(g, O[i], X, Y, T, max, slack, force);
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

    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigaction(SIGINT, &sa, NULL);

    FILE *f = fopen(argv[1], "r");
    graph *g = graph_parse(f);
    fclose(f);

    int verbose = 0;

    printf("%s,%d,%d,", argv[1], g->n, g->m);

    int *T = malloc(sizeof(int) * g->m * 2);
    int *X = malloc(sizeof(int) * g->m * 2);
    int *Y = malloc(sizeof(int) * g->m * 2);

    int *I = malloc(sizeof(int) * g->org_n);
    for (int i = 0; i < g->org_n; i++)
        I[i] = 0;

    int *O = malloc(sizeof(int) * g->n * 8);

    for (int it = 0; it < MAX_ITERATIONS && !tle && g->nr > 0; it++)
    {
        int t = g->t;
        int ref_zero = g->ref_zero_count;

        reduce_loop(g, MAX_IMPROVEMENT_LOOPS / 2, O, X, Y, T, (it % 2) == 0, STARTING_CEILING + it * INCREMENT_CEILING, 0, 0);
        if (verbose)
            printf("%d %d %lld\n", g->nr, g->m, g->off);

        reduce_loop(g, MAX_IMPROVEMENT_LOOPS / 2, O, X, Y, T, (it % 2) == 0, STARTING_CEILING + it * INCREMENT_CEILING, 1, 0);
        if (verbose)
            printf("%d %d %lld\n", g->nr, g->m, g->off);

        if (g->nr < FORCE_LIMIT)
            reduce_loop(g, MAX_IMPROVEMENT_LOOPS, O, X, Y, T, it, STARTING_CEILING + it * FORCE_FACTOR, 1, 1);

        restart(g, t, ref_zero, T, I);
    }

    folding = 0;

    if (g->nr > 0)
    {
        reduce_loop(g, MAX_IMPROVEMENT_LOOPS / 2, O, X, Y, T, 1, STARTING_CEILING + 32 * INCREMENT_CEILING, 0, 0);
        reduce_loop(g, MAX_IMPROVEMENT_LOOPS / 2, O, X, Y, T, 1, STARTING_CEILING + 32 * INCREMENT_CEILING, 1, 0);
    }

    printf("%d,%d,%lld\n", g->nr, g->m, g->off);

    // Store reduced graph
    int *RM = malloc(sizeof(int) * g->n);
    int id = 0;
    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;
        RM[u] = id++;
    }

    f = fopen(argv[2], "w");
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

    int inc = 0, exc = 0;
    for (int u = 0; u < g->org_n; u++)
    {
        if (I[u] < 0)
            exc++;
        else if (I[u] > 0)
            inc++;
    }

    // Storing the meta information
    f = fopen(argv[3], "w");
    // for (int u = 0; u < g->org_n; u++)
    // {
    //     if (I[u] <= 0)
    //         continue;
    //     fprintf(f, "%d\n", u + 1);
    // }
    fprintf(f, "%d %d %d\n", inc, exc, g->nr);
    for (int u = 0; u < g->org_n; u++)
    {
        if (I[u] <= 0)
            continue;
        fprintf(f, "%d ", u + 1);
    }
    fprintf(f, "\n");
    for (int u = 0; u < g->org_n; u++)
    {
        if (I[u] >= 0)
            continue;
        fprintf(f, "%d ", u + 1);
    }
    fprintf(f, "\n");
    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;
        recursive_print(f, g, u);
        fprintf(f, "\n");
    }
    fclose(f);

    free(T);
    free(X);
    free(Y);
    free(I);
    free(O);

    graph_free(g);

    return 0;
}