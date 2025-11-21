#define _GNU_SOURCE

#include "cliques.h"
#include "algorithms.h"

#include <stdlib.h>
#include <assert.h>
#include <limits.h>

/*
    Greedy clique cover inspired by graph coloring
*/
int greedy_cliques(graph *g, int *Order, int *Clique, int *Clique_size, int *Count)
{
    int nc = 0;

    for (int i = 0; i < g->n; i++)
    {
        int u = Order[i];
        if (!g->A[u])
            continue;

        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];
            Count[Clique[v]]++;
        }

        int best = 0, best_count = 0;

        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];

            int c = Clique[v];
            if (c > 0 && Count[c] == Clique_size[c] && Count[c] > best_count)
            {
                best = c;
                best_count = Count[c];
            }

            Count[c] = 0;
        }

        if (best > 0)
        {
            Clique[u] = best;
            Clique_size[best]++;
        }
        else
        {
            Clique[u] = nc + 1;
            Clique_size[nc + 1] = 1;
            nc++;
        }
    }

    return nc;
}

// Compare clique assignment
int compare_clique(const void *a, const void *b, void *c)
{
    int u = *(int *)a;
    int v = *(int *)b;
    int *C = (int *)c;

    return C[v] - C[u];
}

void update_ub(cliques *c, graph *g)
{
    c->ub = 0;
    for (int i = 0; i < c->n; i++)
    {
        long long max = 0;
        for (int j = 0; j < c->S[i]; j++)
        {
            int u = c->C[i][j];
            if (g->W[u] > max)
                max = g->W[u];
        }
        c->ub += max;
    }
}

void clear_clique_edges(graph *g, cliques *c)
{
    for (int i = 0; i < c->n; i++)
    {
        for (int j = 0; j < c->S[i]; j++)
        {
            int u = c->C[i][j];
            for (int k = j + 1; k < c->S[i]; k++)
            {
                int v = c->C[i][k];
                graph_remove_edge(g, u, v);
            }
        }
    }
}

cliques *cliques_init(graph *g)
{
    cliques *c = malloc(sizeof(cliques));

    int *Order = malloc(sizeof(int) * g->n);
    for (int u = 0; u < g->n; u++)
        Order[u] = u;

    int *Clique = calloc(sizeof(int), g->n);
    int *Clique_size = calloc(sizeof(int), g->n);
    int *Count = calloc(sizeof(int), g->n);

    int nc = greedy_cliques(g, Order, Clique, Clique_size, Count);

    for (int i = 0; i < 10; i++)
    {
        qsort_r(Order, g->n, sizeof(int), compare_clique, Clique);

        for (int u = 0; u < g->n; u++)
        {
            Clique[u] = 0;
            Clique_size[u] = 0;
        }

        nc = greedy_cliques(g, Order, Clique, Clique_size, Count);
    }

    free(Order);

    c->n = nc;

    c->C = malloc(sizeof(int *) * c->n);
    c->S = malloc(sizeof(int) * c->n);
    c->FM = Clique;

    for (int i = 0; i < c->n; i++)
    {
        c->S[i] = Clique_size[i + 1];
        c->C[i] = malloc(sizeof(int) * c->S[i]);
    }

    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;

        c->FM[u]--; // Change from 1-indexed cliques to forward mapping
        int cu = c->FM[u];
        c->C[cu][Count[cu]] = u;
        Count[cu]++;
    }

    update_ub(c, g);

    clear_clique_edges(g, c);

    c->gc = graph_init(1);
    for (int i = 0; i < c->n; i++)
    {
        graph_add_vertex(c->gc, c->S[i] + SIZE_OFFSET, 0, 0);
    }
    for (int i = 0; i < c->n; i++)
    {
        for (int j = 0; j < c->S[i]; j++)
        {
            int u = c->C[i][j];
            for (int k = 0; k < g->D[u]; k++)
            {
                int v = g->V[u][k];
                int dest = c->FM[v];

                assert(dest != i);
                if (dest < i)
                    continue;

                if (!graph_is_neighbor(c->gc, i, dest))
                    graph_add_edge(c->gc, i, dest, 1);
                else
                    graph_increase_edge_weight(c->gc, i, dest, 1);
            }
        }
    }

    free(Clique_size);
    free(Count);

    c->mn = -1;
    c->mnr = -1;
    c->c1 = -1;
    c->c2 = -1;
    c->V = malloc(sizeof(int) * (MAX_ELEMENTS + 1));
    c->R = malloc(sizeof(int) * MAX_ELEMENTS);
    c->L = malloc(sizeof(int) * MAX_ELEMENTS);
    c->X = malloc(sizeof(int) * MAX_ELEMENTS);
    c->Y = malloc(sizeof(int) * MAX_ELEMENTS);
    c->O = malloc(sizeof(int) * MAX_ELEMENTS);
    c->W = malloc(sizeof(long long) * MAX_ELEMENTS);
    c->E = malloc(sizeof(int) * MAX_EDGES);

    return c;
}

void cliques_free(cliques *c)
{
    for (int i = 0; i < c->n; i++)
    {
        free(c->C[i]);
    }

    free(c->C);
    free(c->S);
    free(c->FM);

    graph_free(c->gc);

    free(c->V);
    free(c->R);
    free(c->L);
    free(c->X);
    free(c->Y);
    free(c->O);
    free(c->W);
    free(c->E);
}

typedef struct
{
    long long *W;
    int *V;
} merging_data;

int compare_vertices(const void *a, const void *b, void *c)
{
    int u = *(int *)a;
    int v = *(int *)b;
    merging_data *md = (merging_data *)c;

    if (md->W[u] > md->W[v] || (md->W[u] == md->W[v] && md->V[u + 1] - md->V[u] < md->V[v + 1] - md->V[v]))
        return -1;

    if (md->W[u] < md->W[v] || (md->W[u] == md->W[v] && md->V[u + 1] - md->V[u] > md->V[v + 1] - md->V[v]))
        return 1;

    return 0;
}

int cliques_merge_prep(cliques *c, graph *g, int c1, int c2, int stop)
{
    int n = 0, nr = 0, nd = 0, m = 0;

    for (int i = 0; i < c->S[c1]; i++)
        assert(g->A[c->C[c1][i]]);
    for (int i = 0; i < c->S[c2]; i++)
        assert(g->A[c->C[c2][i]]);

    c->mn = -1;
    c->mnr = -1;
    c->mnd = -1;
    c->c1 = -1;
    c->c2 = -1;

    // Copy elements in c1
    for (int i = 0; i < c->S[c1]; i++)
    {
        int u = c->C[c1][i];
        if (n >= MAX_ELEMENTS || n >= stop)
            return 0;

        c->V[n] = m;
        c->R[n] = u;
        c->L[n] = u;
        c->W[n] = g->W[u];
        n++;
        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];
            if (c->FM[v] == c2)
                continue;

            if (m == MAX_EDGES)
                return 0;
            c->E[m++] = v;
        }
    }

    // Copy elements in c2
    for (int i = 0; i < c->S[c2]; i++)
    {
        int u = c->C[c2][i];
        if (n >= MAX_ELEMENTS || n >= stop)
            return 0;

        c->V[n] = m;
        c->R[n] = u;
        c->L[n] = u;
        c->W[n] = g->W[u];
        n++;
        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];
            if (c->FM[v] == c1)
                continue;

            if (m == MAX_EDGES)
                return 0;
            c->E[m++] = v;
        }
    }

    // Construct new vertices
    for (int u = 0; u < c->S[c1]; u++)
    {
        for (int v = c->S[c1]; v < c->S[c1] + c->S[c2]; v++)
        {
            if (graph_is_neighbor(g, c->R[u], c->R[v]))
                continue;

            if (n >= MAX_ELEMENTS || n >= stop)
                return 0;

            c->V[n] = m;
            c->W[n] = c->W[u] + c->W[v];
            c->L[n] = c->L[u]; // Original id for first IS
            c->R[n] = c->R[v]; // Original id for second IS
            n++;

            int i = c->V[u], j = c->V[v];
            while (i < c->V[u + 1] && j < c->V[v + 1])
            {
                if (m == MAX_EDGES)
                    return 0;

                if (c->E[i] < c->E[j])
                {
                    c->E[m++] = c->E[i++];
                }
                else if (c->E[j] < c->E[i])
                {
                    c->E[m++] = c->E[j++];
                }
                else
                {
                    c->E[m++] = c->E[i++];
                    j++;
                }
            }

            while (i < c->V[u + 1])
            {
                if (m == MAX_EDGES)
                    return 0;
                c->E[m++] = c->E[i++];
            }
            while (j < c->V[v + 1])
            {
                if (m == MAX_EDGES)
                    return 0;
                c->E[m++] = c->E[j++];
            }
        }
    }

    c->V[n] = m;

    for (int i = 0; i < n; i++)
        c->O[i] = i;

    merging_data md = {.V = c->V, .W = c->W};
    qsort_r(c->O, n, sizeof(int), compare_vertices, &md);

    for (int i = 0; i < n; i++)
    {
        int u = c->O[i];

        int valid = 1;
        for (int j = 0; j < nr; j++)
        {
            int v = c->X[j];

            if (set_is_subset(c->E + c->V[v], c->V[v + 1] - c->V[v], c->E + c->V[u], c->V[u + 1] - c->V[u]))
            {
                valid = 0;
                break;
            }
        }

        if (valid)
        {
            c->X[nr++] = u;
        }
        else if (u < c->S[c1] + c->S[c2])
        {
            c->Y[nd++] = c->R[u];
        }
    }

    c->mn = n;
    c->mnr = nr;
    c->mnd = nd;
    c->c1 = c1;
    c->c2 = c2;

    return 1;
}

void cliques_merge(cliques *c, graph *g)
{
    int c1 = c->c1, c2 = c->c2;
    if (c->c1 < 0)
        return;

    int max_c1 = 0, max_c2 = 0;
    for (int i = 0; i < c->S[c1]; i++)
        max_c1 = c->W[i] > max_c1 ? c->W[i] : max_c1;

    for (int i = c->S[c1]; i < c->S[c2] + c->S[c1]; i++)
        max_c2 = c->W[i] > max_c2 ? c->W[i] : max_c2;

    int max_new = 0;
    for (int i = 0; i < c->mnr; i++)
        max_new = c->W[c->X[i]] > max_new ? c->W[c->X[i]] : max_new;

    c->ub += max_new - (max_c1 + max_c2);

    for (int i = 0; i < c->mnd; i++)
        graph_deactivate_vertex(g, c->Y[i]);

    int s = g->n;
    int t = c->S[c1] + c->S[c2];
    c->C[c1] = realloc(c->C[c1], sizeof(int) * c->mnr);
    c->S[c1] = 0;

    for (int i = 0; i < c->mnr; i++)
    {
        int u = c->X[i];

        // Existing vertex
        if (u < t)
        {
            int id = c->R[u];
            c->C[c1][c->S[c1]++] = id;
            c->FM[id] = c1;
            for (int i = 0; i < g->D[id]; i++)
            {
                int v = g->V[id][i];
                // Remove edges between c1 and c2
                if (c->FM[v] == c1 || c->FM[v] == c2)
                {
                    graph_remove_edge(g, id, v);
                    i--;
                }
            }
        }
        // New vertex
        else
        {
            c->C[c1][c->S[c1]++] = g->n;
            graph_add_vertex(g, c->W[u], c->L[u], c->R[u]);
            for (int i = c->V[u]; i < c->V[u + 1]; i++)
            {
                int v = c->E[i];
                graph_add_edge(g, g->n - 1, v, 1);
            }
        }
    }

    c->FM = realloc(c->FM, sizeof(int) * g->n);
    for (int i = s; i < g->n; i++)
        c->FM[i] = c1;

    graph_deactivate_vertex(c->gc, c2);
    while (c->gc->D[c1] > 0)
        graph_remove_edge(c->gc, c1, c->gc->V[c1][0]);

    c->S[c2] = 0;
    free(c->C[c2]);
    c->C[c2] = NULL;

    graph_change_vertex_weight(c->gc, c1, c->mnr + SIZE_OFFSET);

    // Fix the clique graph
    for (int i = 0; i < c->mnr; i++)
    {
        int u = c->X[i];
        for (int j = c->V[u]; j < c->V[u + 1]; j++)
        {
            int v = c->E[j];
            if (!g->A[v] || c->FM[v] == c1 || c->FM[v] == c2)
                continue;

            if (graph_is_neighbor(c->gc, c1, c->FM[v]))
                graph_increase_edge_weight(c->gc, c1, c->FM[v], 1);
            else
                graph_add_edge(c->gc, c1, c->FM[v], 1);
        }
    }
}
