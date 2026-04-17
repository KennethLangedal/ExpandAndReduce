#include "graph.h"
#include "algorithms.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>

#define EXPAND 0
#define DEACTIVATE 1
#define FOLD 2
#define EDGE 3

void graph_increase_size(graph *g)
{
    g->_a *= 2;
    g->V = realloc(g->V, sizeof(int *) * g->_a);
    g->D = realloc(g->D, sizeof(int) * g->_a);
    g->W = realloc(g->W, sizeof(long long) * g->_a);
    g->A = realloc(g->A, sizeof(int) * g->_a);

    g->_A = realloc(g->_A, sizeof(int) * g->_a);

    g->Pl = realloc(g->Pl, sizeof(int) * g->_a);
    g->Pr = realloc(g->Pr, sizeof(int) * g->_a);

    for (int i = g->_a / 2; i < g->_a; i++)
    {
        g->_A[i] = 16;
        g->V[i] = malloc(sizeof(int) * g->_A[i]);
    }
}

void graph_append_endpoint(graph *g, int u, int v)
{
    if (g->_A[u] == g->D[u])
    {
        g->_A[u] *= 2;
        g->V[u] = realloc(g->V[u], sizeof(int) * g->_A[u]);
    }

    g->V[u][g->D[u]] = v;
    g->D[u]++;
}

void graph_construction_add_vertex(graph *g, long long w)
{
    if (g->n == g->_a)
        graph_increase_size(g);

    int u = g->n;
    g->D[u] = 0;
    g->A[u] = 1;
    g->W[u] = w;
    g->n++;
    g->nr++;
}

void graph_construction_add_edge(graph *g, int u, int v)
{
    assert(u < g->n && v < g->n);

    graph_append_endpoint(g, u, v);
    graph_append_endpoint(g, v, u);
}

void graph_construction_sort_edges(graph *g)
{
    int m = 0;
    for (int i = 0; i < g->n; i++)
    {
        qsort(g->V[i], g->D[i], sizeof(int), compare_ids);
        int d = 0;
        for (int j = 0; j < g->D[i]; j++)
        {
            if (j == 0 || g->V[i][j] > g->V[i][d - 1])
                g->V[i][d++] = g->V[i][j];
        }
        g->D[i] = d;
        m += d;
    }
    g->m = m / 2;
}

static inline void parse_id(char *Data, size_t *p, long long *v)
{
    while (Data[*p] < '0' || Data[*p] > '9')
        (*p)++;

    *v = 0;
    while (Data[*p] >= '0' && Data[*p] <= '9')
        *v = (*v) * 10 + Data[(*p)++] - '0';
}

static inline void skip_line(char *Data, size_t *p)
{
    while (Data[*p] != '\n')
        (*p)++;
    (*p)++;
}

graph *graph_parse(FILE *f)
{
    graph *g = malloc(sizeof(graph));
    *g = (graph){.n = 0, .m = 0, .nr = 0, .off = 0, ._a = 16, .t = 0, .org_n = 0, ._al = 16};

    g->V = malloc(sizeof(int *) * g->_a);
    g->D = malloc(sizeof(int) * g->_a);
    g->W = malloc(sizeof(long long) * g->_a);

    g->A = malloc(sizeof(int) * g->_a);

    g->_A = malloc(sizeof(int) * g->_a);

    g->Pl = malloc(sizeof(int) * g->_a);
    g->Pr = malloc(sizeof(int) * g->_a);

    for (int i = 0; i < g->_a; i++)
    {
        g->_A[i] = 16;
        g->V[i] = malloc(sizeof(int) * g->_A[i]);
    }

    g->_Log_op = malloc(sizeof(char) * g->_al);
    g->_Log = malloc(sizeof(log_action) * g->_al);

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *Data = mmap(0, size, PROT_READ, MAP_PRIVATE, fileno_unlocked(f), 0);
    size_t p = 0;

    while (Data[p] == '%')
        skip_line(Data, &p);

    long long n, m, w, t;
    parse_id(Data, &p, &n);
    parse_id(Data, &p, &m);
    parse_id(Data, &p, &t);

    skip_line(Data, &p);

    int vertex_weights = t >= 10,
        edge_weights = (t == 1 || t == 11);

    g->org_n = n;
    g->Ref_count = malloc(sizeof(int) * n);

    g->ref_zero_count = 0;
    g->Ref_zero = malloc(sizeof(int) * n);

    for (int u = 0; u < n; u++)
    {
        graph_construction_add_vertex(g, 1);
        g->Pl[u] = u;
        g->Pr[u] = u;
        g->Ref_count[u] = 1;
    }

    long long ei = 0;
    for (int u = 0; u < n; u++)
    {
        while (Data[p] == '%')
            skip_line(Data, &p);

        g->W[u] = 1;
        if (vertex_weights)
        {
            parse_id(Data, &p, &w);
            g->W[u] = w;
        }

        while (ei < m * 2)
        {
            while (Data[p] == ' ')
                p++;
            if (Data[p] == '\n')
                break;

            long long e;
            parse_id(Data, &p, &e);
            if (e - 1 > u)
                graph_construction_add_edge(g, u, e - 1);

            if (edge_weights)
                parse_id(Data, &p, &w);

            ei++;
        }
        p++;
    }

    munmap(Data, size);

    graph_construction_sort_edges(g);

    return g;
}

graph *graph_copy(graph *g)
{
    graph *gc = malloc(sizeof(graph));

    gc->n = g->n;
    gc->m = g->m;
    gc->nr = g->nr;
    gc->off = g->off;

    gc->_a = g->_a;

    gc->V = malloc(sizeof(int *) * gc->_a);
    gc->D = malloc(sizeof(int) * gc->_a);
    gc->W = malloc(sizeof(long long) * gc->_a);
    gc->A = malloc(sizeof(int) * gc->_a);
    gc->_A = malloc(sizeof(int) * gc->_a);

    gc->Pl = malloc(sizeof(int) * gc->_a);
    gc->Pr = malloc(sizeof(int) * gc->_a);

    gc->org_n = g->org_n;
    gc->Ref_count = malloc(sizeof(int) * gc->org_n);

    gc->ref_zero_count = g->ref_zero_count;
    gc->Ref_zero = malloc(sizeof(int) * gc->org_n);

    for (int i = 0; i < gc->org_n; i++)
        gc->Ref_count[i] = g->Ref_count[i];

    for (int i = 0; i < gc->ref_zero_count; i++)
        gc->Ref_zero[i] = g->Ref_zero[i];

    for (int i = 0; i < gc->_a; i++)
    {
        gc->_A[i] = g->_A[i];
        gc->V[i] = malloc(sizeof(int) * gc->_A[i]);
    }

    for (int u = 0; u < g->n; u++)
    {
        gc->D[u] = g->D[u];
        gc->A[u] = g->A[u];
        gc->W[u] = g->W[u];

        gc->Pl[u] = g->Pl[u];
        gc->Pr[u] = g->Pr[u];

        for (int i = 0; i < g->D[u]; i++)
            gc->V[u][i] = g->V[u][i];
    }

    gc->t = g->t;
    gc->_al = g->_al;

    gc->_Log_op = malloc(sizeof(char) * gc->_al);
    gc->_Log = malloc(sizeof(log_action) * gc->_al);

    for (int i = 0; i < g->t; i++)
    {
        gc->_Log_op[i] = g->_Log_op[i];
        gc->_Log[i] = g->_Log[i];
    }

    return gc;
}

void graph_free(graph *g)
{
    for (int i = 0; i < g->_a; i++)
        free(g->V[i]);

    free(g->V);
    free(g->D);
    free(g->W);
    free(g->A);
    free(g->_A);

    free(g->Pl);
    free(g->Pr);

    free(g->Ref_count);

    free(g->Ref_zero);

    free(g->_Log_op);
    free(g->_Log);

    free(g);
}

// Everything below this point assumes sorted neighborhoods

void graph_log_action(graph *g, char op, log_action a)
{
    if (g->_al == g->t)
    {
        g->_al *= 2;
        g->_Log_op = realloc(g->_Log_op, sizeof(char) * g->_al);
        g->_Log = realloc(g->_Log, sizeof(log_action) * g->_al);
    }

    g->_Log_op[g->t] = op;
    g->_Log[g->t] = a;
    g->t++;
}

void increase_ref_count(graph *g, int u)
{
    if (u < g->org_n)
    {
        if (g->Ref_count[u] == 0)
        {
            assert(g->Ref_zero[g->ref_zero_count - 1] == u);
            g->ref_zero_count--;
        }
        g->Ref_count[u]++;
    }
    else
    {
        increase_ref_count(g, g->Pr[u]);
        increase_ref_count(g, g->Pl[u]);
    }
}

void decrease_ref_count(graph *g, int u)
{
    if (u < g->org_n)
    {
        if (g->Ref_count[u] == 1)
            g->Ref_zero[g->ref_zero_count++] = u;
        g->Ref_count[u]--;
    }
    else
    {
        decrease_ref_count(g, g->Pl[u]);
        decrease_ref_count(g, g->Pr[u]);
    }
}

int graph_insert_endpoint(graph *g, int u, int v)
{
    int p = lower_bound(g->V[u], g->D[u], v);

    if (p < g->D[u] && g->V[u][p] == v)
        return 0;

    graph_append_endpoint(g, u, 0);

    memmove(g->V[u] + p + 1, g->V[u] + p, sizeof(int) * (g->D[u] - p - 1));

    g->V[u][p] = v;

    return 1;
}

int graph_remove_endpoint(graph *g, int u, int v)
{
    int p = lower_bound(g->V[u], g->D[u], v);

    if (p >= g->D[u] || g->V[u][p] != v)
        return 0;

    memmove(g->V[u] + p, g->V[u] + p + 1, sizeof(int) * (g->D[u] - p - 1));

    g->D[u]--;

    return 1;
}

void graph_add_edge(graph *g, int u, int v)
{
    assert(g->A[u] && g->A[v] && !graph_is_neighbor(g, u, v));

    int uv = graph_insert_endpoint(g, u, v);
    int vu = graph_insert_endpoint(g, v, u);
    assert(uv && vu);

    g->m += 1;

    graph_log_action(g, EDGE, (log_action){.u = u, .v = v});
}

void graph_add_edge_undo(graph *g, log_action a)
{
    int u = a.u, v = a.v;

    assert(g->A[u] && g->A[v] && graph_is_neighbor(g, u, v));

    int uv = graph_remove_endpoint(g, u, v);
    int vu = graph_remove_endpoint(g, v, u);
    assert(uv && vu);

    g->m -= 1;
}

void graph_edge_expansion(graph *g, int u, int v)
{
    assert(g->A[u] && g->A[v] && !graph_is_neighbor(g, u, v));

    // Constructing the new vertex

    graph_construction_add_vertex(g, g->W[u] + g->W[v]);

    int w = g->n - 1;

    g->Pl[w] = u;
    g->Pr[w] = v;

    increase_ref_count(g, w);

    while (g->_A[w] < g->D[u] + g->D[v] + 2)
        g->_A[w] *= 2;

    g->V[w] = realloc(g->V[w], sizeof(int) * g->_A[w]);

    g->D[w] = set_union_pluss_two(g->V[u], g->D[u], g->V[v], g->D[v], u, v, g->V[w]);

    for (int i = 0; i < g->D[w]; i++)
    {
        int x = g->V[w][i];
        graph_append_endpoint(g, x, w);
    }

    // Insert edge between u and v

    int uv = graph_insert_endpoint(g, u, v);
    int vu = graph_insert_endpoint(g, v, u);
    assert(uv && vu);

    g->m += g->D[w] + 1;

    graph_log_action(g, EXPAND, (log_action){.u = u, .v = v});
}

void graph_edge_expansion_undo(graph *g, log_action a)
{
    int u = a.u, v = a.v;
    int w = g->n - 1;

    decrease_ref_count(g, w);

    assert(g->A[u] && g->A[v] && g->A[w]);

    // Undo the new vertex

    for (int i = 0; i < g->D[w]; i++)
    {
        int x = g->V[w][i];
        g->D[x]--;
    }

    g->n--;
    g->nr--;

    // Remove the edge between u and v

    int uv = graph_remove_endpoint(g, u, v);
    int vu = graph_remove_endpoint(g, v, u);
    assert(uv && vu);

    g->m -= g->D[w] + 1;
}

void graph_clique_fold(graph *g, int u)
{
    assert(g->A[u]);

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        assert(g->W[v] > g->W[u]);
        g->W[v] -= g->W[u];
    }

    g->off += g->W[u];
    graph_log_action(g, FOLD, (log_action){.u = u, .v = 0});

    increase_ref_count(g, u); // To not count u as removed yet
    graph_deactivate_vertex(g, u);
}

void graph_clique_fold_undo(graph *g, log_action a)
{
    int u = a.u;
    assert(g->A[u]);

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        g->W[v] += g->W[u];
        assert(g->W[v] > g->W[u]);
    }

    g->off -= g->W[u];
    decrease_ref_count(g, u);
}

void graph_deactivate_vertex(graph *g, int u)
{
    assert(g->A[u]);

    decrease_ref_count(g, u);

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        int found = graph_remove_endpoint(g, v, u);
        assert(found);
    }
    g->A[u] = 0;
    g->m -= g->D[u];
    g->nr--;

    graph_log_action(g, DEACTIVATE, (log_action){.u = u, .v = 0});
}

void graph_deactivate_vertex_undo(graph *g, log_action a)
{
    int u = a.u;
    assert(!g->A[u]);

    increase_ref_count(g, u);

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        int found = graph_insert_endpoint(g, v, u);
        assert(found);
    }

    g->A[u] = 1;
    g->m += g->D[u];
    g->nr++;
}

void graph_undo_changes(graph *g, int t)
{
    while (g->t > t)
    {
        g->t--;
        log_action a = g->_Log[g->t];
        char op = g->_Log_op[g->t];

        switch (op)
        {
        case EXPAND:
            graph_edge_expansion_undo(g, a);
            break;
        case DEACTIVATE:
            graph_deactivate_vertex_undo(g, a);
            break;
        case FOLD:
            graph_clique_fold_undo(g, a);
            break;
        case EDGE:
            graph_add_edge_undo(g, a);
            break;

        default:
            break;
        }
    }
}

int graph_is_neighbor(graph *g, int u, int v)
{
    assert(g->A[u] && g->A[v]);

    int pu = lower_bound(g->V[u], g->D[u], v);
    int uv = pu < g->D[u] && g->V[u][pu] == v;

    int pv = lower_bound(g->V[v], g->D[v], u);
    int vu = pv < g->D[v] && g->V[v][pv] == u;

    assert(uv == vu);

    return uv;
}

int graph_reduce(graph *g, int u, int fold)
{
    if (!g->A[u])
        return 0;

    int reduced = 0;
    int foldable = fold;

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];

        if (g->W[u] < g->W[v])
        {
            // u is dominated by v (v is heavier and N(v) is a subset of N(u))
            if (set_is_subset_except_one(g->V[v], g->D[v], g->V[u], g->D[u], u))
            {
                graph_deactivate_vertex(g, u);
                return 1;
            }
            // if v is dominated by u, but the weights are the wrong way around, we could fold u (only if it applies for all)
            else if (foldable && !set_is_subset_except_one(g->V[u], g->D[u], g->V[v], g->D[v], v))
            {
                foldable = 0;
            }
        }
        else
        {
            // u is dominating v (u is heavier and N(u) is a subset of N(v))
            if (set_is_subset_except_one(g->V[u], g->D[u], g->V[v], g->D[v], v))
            {
                graph_deactivate_vertex(g, v);
                reduced++;
                i--;
            }
            else
            {
                foldable = 0;
            }
        }
    }

    if (foldable)
    {
        graph_clique_fold(g, u);
        return 1;
    }

    return reduced > 0;
}