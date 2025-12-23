#include "graph.h"
#include "algorithms.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>

#define ADD_VERTEX 0
#define REMOVE_VERTEX 1
#define ADD_EDGE 2
#define REMOVE_EDGE 3
#define CHANGE_WEIGHT 4
#define ADD_VERTEX_FULL 5

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

graph *graph_init()
{
    graph *g = malloc(sizeof(graph));
    *g = (graph){.n = 0, .m = 0, .nr = 0, ._a = 16, .t = 0, ._al = 16};

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

    return g;
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
    graph *g = graph_init(0);

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

    for (int u = 0; u < n; u++)
    {
        graph_construction_add_vertex(g, 1);
        g->Pl[u] = u;
        g->Pr[u] = u;
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

    gc->_a = g->_a;

    gc->V = malloc(sizeof(int *) * gc->_a);
    gc->D = malloc(sizeof(int) * gc->_a);
    gc->W = malloc(sizeof(long long) * gc->_a);
    gc->A = malloc(sizeof(int) * gc->_a);
    gc->_A = malloc(sizeof(int) * gc->_a);

    gc->Pl = malloc(sizeof(int) * gc->_a);
    gc->Pr = malloc(sizeof(int) * gc->_a);

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
        {
            gc->V[u][i] = g->V[u][i];
        }
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
    {
        free(g->V[i]);
    }
    free(g->V);
    free(g->D);
    free(g->W);
    free(g->A);
    free(g->_A);

    free(g->Pl);
    free(g->Pr);

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

void graph_add_vertex(graph *g, long long w, int pl, int pr)
{
    graph_construction_add_vertex(g, w);

    int u = g->n - 1;

    g->Pl[u] = pl;
    g->Pr[u] = pr;

    graph_log_action(g, ADD_VERTEX, (log_action){.V = {u, 0}, .w = 0});
}

void graph_add_vertex_undo(graph *g, log_action a)
{
    g->n--;
    g->nr--;
}

void graph_add_vertex_full(graph *g, long long w, int pl, int pr, int *N, int d)
{
    graph_construction_add_vertex(g, w);

    int u = g->n - 1;

    g->Pl[u] = pl;
    g->Pr[u] = pr;

    for (int i = 0; i < d; i++)
    {
        int v = N[i];
        // Safe since u is the largest ID in the graph
        graph_append_endpoint(g, u, v);
        graph_append_endpoint(g, v, u);
    }

    g->m += d;

    graph_log_action(g, ADD_VERTEX_FULL, (log_action){.V = {u, 0}, .w = 0});
}

void graph_add_vertex_full_undo(graph *g, log_action a)
{
    int u = g->n - 1;

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        // Safe since u is the largest ID in the graph
        g->D[v]--;
    }

    g->n--;
    g->nr--;

    g->m -= g->D[u];
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

void graph_add_edge(graph *g, int u, int v)
{
    assert(g->A[u] && g->A[v]);

    if (u == v)
        return;

    int e0 = graph_insert_endpoint(g, u, v);
    int e1 = graph_insert_endpoint(g, v, u);

    assert(e0 == e1);

    if (e0 && e1)
    {
        g->m++;
        graph_log_action(g, ADD_EDGE, (log_action){.V = {u, v}, .w = 0});
    }
}

void graph_add_edge_undo(graph *g, log_action a)
{
    graph_remove_edge(g, a.V[0], a.V[1]);
    g->t--;
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

void graph_remove_edge(graph *g, int u, int v)
{
    assert(g->A[u] && g->A[v]);

    int e0 = graph_remove_endpoint(g, u, v);
    int e1 = graph_remove_endpoint(g, v, u);

    assert(e0 == e1);

    if (e0 && e1)
    {
        g->m--;
        graph_log_action(g, REMOVE_EDGE, (log_action){.V = {u, v}, .w = 0});
    }
}

void graph_remove_edge_undo(graph *g, log_action a)
{
    graph_add_edge(g, a.V[0], a.V[1]);
    g->t--;
}

void graph_deactivate_vertex(graph *g, int u)
{
    assert(g->A[u]);

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        int found = graph_remove_endpoint(g, v, u);
        assert(found);
    }
    g->A[u] = 0;
    g->m -= g->D[u];
    g->nr--;

    graph_log_action(g, REMOVE_VERTEX, (log_action){.V = {u, 0}, .w = 0});
}

void graph_deactivate_vertex_undo(graph *g, log_action a)
{
    int u = a.V[0];
    assert(!g->A[u]);

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

void graph_change_vertex_weight(graph *g, int u, long long w)
{
    graph_log_action(g, CHANGE_WEIGHT, (log_action){.V = {u, 0}, .w = g->W[u]});

    g->W[u] = w;
}

void graph_change_vertex_weight_undo(graph *g, log_action a)
{
    int u = a.V[0];
    int w = a.w;

    g->W[u] = w;
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
        case ADD_VERTEX:
            graph_add_vertex_undo(g, a);
            break;
        case REMOVE_VERTEX:
            graph_deactivate_vertex_undo(g, a);
            break;
        case ADD_EDGE:
            graph_add_edge_undo(g, a);
            break;
        case REMOVE_EDGE:
            graph_remove_edge_undo(g, a);
            break;
        case CHANGE_WEIGHT:
            graph_change_vertex_weight_undo(g, a);
            break;
        case ADD_VERTEX_FULL:
            graph_add_vertex_full_undo(g, a);
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