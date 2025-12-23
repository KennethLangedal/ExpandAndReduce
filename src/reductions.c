#include "reductions.h"
#include "algorithms.h"

#include <assert.h>
#include <stdlib.h>

int reduction_neighborhood(graph *g, int u, long long *offset)
{
    if (!g->A[u])
        return 0;

    long long nw = 0;

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        nw += g->W[v];

        if (g->W[u] < nw)
            break;
    }

    if (nw <= g->W[u])
    {
        while (g->D[u] > 0)
        {
            int v = g->V[u][g->D[u] - 1];
            graph_deactivate_vertex(g, v);
        }
        *offset += g->W[u];
        graph_deactivate_vertex(g, u);

        return 1;
    }

    return 0;
}

int reduction_domination(graph *g, int u)
{
    if (!g->A[u] || g->D[u] > 100)
        return 0;

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];

        if (g->W[u] > g->W[v])
            continue;

        if (set_is_subset_except_one(g->V[v], g->D[v], g->V[u], g->D[u], u))
        {
            graph_deactivate_vertex(g, u);

            return 1;
        }
    }

    return 0;
}

int reduction_dominates(graph *g, int u)
{
    if (!g->A[u] || g->D[u] > 100)
        return 0;

    int removed = 0;

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];

        if (g->W[u] < g->W[v])
            continue;

        if (set_is_subset_except_one(g->V[u], g->D[u], g->V[v], g->D[v], v))
        {
            graph_deactivate_vertex(g, v);
            removed++;
            i--;
        }
    }

    return removed;
}

void edge_expansion(graph *g, int u, int v, int *T)
{
    if (graph_is_neighbor(g, u, v))
        return;

    int d = set_union_pluss_two(g->V[u], g->D[u], g->V[v], g->D[v], u, v, T);

    graph_add_vertex_full(g, g->W[u] + g->W[v], u, v, T, d);
    graph_add_edge(g, u, v);
}

int reduction_domination_edge(graph *g, int u, int *T)
{
    if (!g->A[u])
        return 0;

    int w = -1;

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];

        if (g->W[u] > g->W[v])
            continue;

        if (!set_is_subset_except_one_pluss_one(g->V[v], g->D[v], g->V[u], g->D[u], u, &w))
            continue;

        if (w >= 0)
        {
            assert(!graph_is_neighbor(g, u, w));
            edge_expansion(g, u, w, T);
        }

        graph_deactivate_vertex(g, u);

        return 1;
    }

    return 0;
}

int reduction_add_edges(graph *g, int u, int *T)
{
    if (!g->A[u])
        return 0;

    int res = 0;
    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];

        if (g->W[v] < g->W[u])
            continue;

        int n = set_minus_except_one(g->V[v], g->D[v], g->V[u], g->D[u], T, u);

        if (n == 0)
            continue;

        // Smallest degree vertex in N(v) \ N[u]
        int cand = -1;
        for (int j = 0; j < n; j++)
        {
            int w = T[j];
            if (cand < 0 || g->D[w] < g->D[cand])
                cand = w;
        }

        for (int j = 0; j < g->D[cand]; j++)
        {
            int w = g->V[cand][j];

            if (!graph_is_neighbor(g, u, w) && !graph_is_neighbor(g, v, w) &&
                set_is_subset(T, n, g->V[w], g->D[w]))
            {
                graph_add_edge(g, u, w);
                res++;
            }
        }
    }

    return res;
}

int reduction_weight_transfer(graph *g, int u, long long *offset)
{
    if (!g->A[u] || g->D[u] > 100)
        return 0;

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];

        if (!set_is_subset_except_one(g->V[u], g->D[u], g->V[v], g->D[v], v))
            return 0;
    }

    *offset += g->W[u];

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];

        if (g->W[v] <= g->W[u])
        {
            graph_deactivate_vertex(g, v);
            i--;
        }
        else
        {
            graph_change_vertex_weight(g, v, g->W[v] - g->W[u]);
        }
    }

    graph_deactivate_vertex(g, u);

    return 1;
}

int reduction_edge_expansion_search(graph *g, int u, int *T)
{
    if (!g->A[u] || g->D[u] <= 1)
        return 0;

    int t = g->t;

    int v1 = g->V[u][rand() % g->D[u]];
    int v2 = g->V[u][rand() % g->D[u]];

    if (v1 == v2 || graph_is_neighbor(g, v1, v2))
        return 0;

    edge_expansion(g, v1, v2, T);

    int red = 0;

    red += reduction_dominates(g, g->n - 1);
    red += reduction_domination(g, g->n - 1);

    red += reduction_dominates(g, v1);
    red += reduction_domination(g, v1);

    red += reduction_dominates(g, v2);
    red += reduction_domination(g, v2);

    if (red == 0)
    {
        graph_undo_changes(g, t);
    }

    return red;
}

int reduction_edge_expansion_two_search(graph *g, int u, int *T)
{
    if (!g->A[u] || g->D[u] == 0)
        return 0;

    int t = g->t;
    int ref_n = g->nr, ref_m = g->m;

    int v = g->V[u][rand() % g->D[u]];
    int w1 = g->V[v][rand() % g->D[v]];

    if (w1 == u || graph_is_neighbor(g, u, w1))
        return 0;

    v = g->V[u][rand() % g->D[u]];
    int w2 = g->V[v][rand() % g->D[v]];

    if (w2 == u || w2 == w1 || graph_is_neighbor(g, u, w2))
        return 0;

    edge_expansion(g, u, w1, T);
    edge_expansion(g, u, w2, T);

    int red = 0;

    red += reduction_dominates(g, g->n - 1);
    red += reduction_domination(g, g->n - 1);

    red += reduction_dominates(g, g->n - 2);
    red += reduction_domination(g, g->n - 2);

    red += reduction_dominates(g, u);
    red += reduction_domination(g, u);

    red += reduction_dominates(g, w1);
    red += reduction_domination(g, w1);

    red += reduction_dominates(g, w2);
    red += reduction_domination(g, w2);

    if (red == 0 || g->nr > ref_n)
    {
        graph_undo_changes(g, t);
    }

    return red;
}

int reduction_edge_expansion_neighborhood(graph *g, int u, int *T1, int *T2, long long *offset)
{
    if (!g->A[u] || g->D[u] <= 1)
        return 0;

    int t = g->t;
    int ref_n = g->nr, ref_m = g->m;

    long long ref_offset = *offset;

    int d = g->D[u];
    for (int i = 0; i < d; i++)
        T2[i] = g->V[u][i];

    if (d > 50)
        return 0;

    for (int i = 0; i < d; i++)
    {
        int v1 = T2[i];
        for (int j = i + 1; j < d; j++)
        {
            int v2 = T2[j];

            if (!graph_is_neighbor(g, v1, v2))
                edge_expansion(g, v1, v2, T1);
        }
    }

    reduction_weight_transfer(g, u, offset);

    int imp = 1;
    while (imp)
    {
        imp = 0;
        for (int i = 0; i < g->D[u]; i++)
        {
            int v = g->V[u][i];

            imp += reduction_domination(g, v);
            imp += reduction_weight_transfer(g, v, offset);
        }
    }

    if (g->nr > ref_n)
    {
        graph_undo_changes(g, t);
        *offset = ref_offset;

        return 0;
    }

    return 1;
}

int reduction_edge_expansion_neighborhood_alt(graph *g, int u, int *T1, int *T2, long long *offset, int strict)
{
    if (!g->A[u] || g->D[u] <= 1)
        return 0;

    int t = g->t;
    int ref_n = g->nr, ref_m = g->m;

    long long ref_offset = *offset;

    int n = (rand() % 512) + 1;

    for (int i = 0; i < n; i++)
    {
        int v1 = g->V[u][rand() % g->D[u]];
        int v2 = g->V[u][rand() % g->D[u]];

        if (v1 == v2 || graph_is_neighbor(g, v1, v2))
            continue;

        edge_expansion(g, v1, v2, T1);
    }

    // n = (rand() % 8) + 1;

    // for (int i = 0; i < n; i++)
    // {
    //     int v1 = g->V[u][rand() % g->D[u]];
    //     int v2 = g->V[v1][rand() % g->D[v1]];

    //     if (u == v2 || graph_is_neighbor(g, u, v2))
    //         continue;

    //     edge_expansion(g, u, v2, T1);
    // }

    reduction_domination(g, u);
    reduction_weight_transfer(g, u, offset);

    int imp = 1;
    while (imp)
    {
        imp = 0;
        for (int i = 0; i < g->D[u]; i++)
        {
            int v = g->V[u][i];

            imp += reduction_domination(g, v);
            imp += reduction_dominates(g, v);
            imp += reduction_weight_transfer(g, v, offset);
        }
    }

    // for (int i = 0; i < g->D[u]; i++)
    // {
    //     int v = g->V[u][i];
    //     if (g->A[v] && g->Pl[v] != v && g->A[g->Pl[v]] && g->A[g->Pr[v]])
    //     {
    //         // We can undo the edge expansion here
    //         graph_deactivate_vertex(g, v);
    //         graph_remove_edge(g, g->Pl[v], g->Pr[v]);
    //     }
    // }

    int undo = 0;
    if (strict == 0)
    {
        undo = g->nr >= ref_n || g->m >= ref_m;
    }
    else if (strict == 1)
    {
        undo = g->nr >= ref_n;
    }
    else if (strict == 2)
    {
        undo = g->nr > ref_n;
    }
    else
    {
        undo = *offset <= ref_offset || g->nr > ref_n + (strict - 2);
    }

    if (undo)
    {
        graph_undo_changes(g, t);
        *offset = ref_offset;

        return 0;
    }

    return 1;
}