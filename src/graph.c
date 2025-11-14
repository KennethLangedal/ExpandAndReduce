#include "graph.h"
#include "algorithms.h"

#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>

void graph_increase_size(graph *g)
{
    g->_a *= 2;
    g->V = realloc(g->V, sizeof(int *) * g->_a);
    g->D = realloc(g->D, sizeof(int) * g->_a);
    g->A = realloc(g->A, sizeof(int) * g->_a);
    g->_A = realloc(g->_A, sizeof(long long) * g->_a);
    g->W = realloc(g->W, sizeof(long long) * g->_a);

    g->pl = realloc(g->pl, sizeof(int) * g->_a);
    g->pr = realloc(g->pr, sizeof(int) * g->_a);

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
    g->V[u][g->D[u]++] = v;
}

graph *graph_init()
{
    graph *g = malloc(sizeof(graph));
    *g = (graph){.n = 0, .m = 0, .nr = 0, ._a = 16};

    g->V = malloc(sizeof(int *) * g->_a);
    g->D = malloc(sizeof(int) * g->_a);
    g->A = malloc(sizeof(int) * g->_a);
    g->_A = malloc(sizeof(long long) * g->_a);
    g->W = malloc(sizeof(long long) * g->_a);

    g->pl = malloc(sizeof(int) * g->_a);
    g->pr = malloc(sizeof(int) * g->_a);

    for (int i = 0; i < g->_a; i++)
    {
        g->_A[i] = 16;
        g->V[i] = malloc(sizeof(int) * g->_A[i]);
    }

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
    long long m = 0;
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
    g->m = m;
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
    graph *g = graph_init();

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

    int vertex_weights = t >= 10,
        edge_weights = (t == 1 || t == 11);

    for (int u = 0; u < n; u++)
    {
        graph_construction_add_vertex(g, 1);
        g->pl[u] = u;
        g->pr[u] = u;
    }

    long long ei = 0;
    for (int u = 0; u < n; u++)
    {
        while (Data[p] == '%')
            skip_line(Data, &p);

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

    free(g->pl);
    free(g->pr);

    free(g);
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
    gc->A = malloc(sizeof(int) * gc->_a);
    gc->_A = malloc(sizeof(long long) * gc->_a);
    gc->W = malloc(sizeof(long long) * gc->_a);

    gc->pl = malloc(sizeof(int) * gc->_a);
    gc->pr = malloc(sizeof(int) * gc->_a);

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

        gc->pl[u] = g->pl[u];
        gc->pr[u] = g->pr[u];

        for (int i = 0; i < g->D[u]; i++)
        {
            gc->V[u][i] = g->V[u][i];
        }
    }

    return gc;
}

// Everything below this point assumes sorted neighborhoods

void graph_add_vertex(graph *g, long long w)
{
    graph_construction_add_vertex(g, w);
}

int graph_insert_endpoint(graph *g, int u, int v)
{
    int p = lower_bound(g->V[u], g->D[u], v);

    if (p < g->D[u] && g->V[u][p] == v)
        return 0;

    graph_append_endpoint(g, u, v);
    for (int i = g->D[u] - 1; i > p; i--)
    {
        g->V[u][i] = g->V[u][i - 1];
    }
    g->V[u][p] = v;
    g->m++;

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
}

int graph_remove_endpoint(graph *g, int u, int v)
{
    int p = lower_bound(g->V[u], g->D[u], v);

    if (p >= g->D[u] || g->V[u][p] != v)
        return 0;

    for (int i = p + 1; i < g->D[u]; i++)
    {
        g->V[u][i - 1] = g->V[u][i];
    }
    g->D[u]--;
    g->m--;

    return 1;
}

void graph_remove_edge(graph *g, int u, int v)
{
    assert(g->A[u] && g->A[v]);

    int e0 = graph_remove_endpoint(g, u, v);
    int e1 = graph_remove_endpoint(g, v, u);

    assert(e0 == e1 && e0 == 1);
}

void graph_deactivate_vertex(graph *g, int u)
{
    assert(g->A[u]);

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        graph_remove_endpoint(g, v, u);
    }
    g->A[u] = 0;
    g->m -= g->D[u];
    g->nr--;
}

void graph_change_vertex_weight(graph *g, int u, long long w)
{
    g->W[u] = w;
}

int graph_is_neighbor(graph *g, int u, int v)
{
    int pu = lower_bound(g->V[u], g->D[u], v);
    int uv = pu < g->D[u] && g->V[u][pu] == v;

    int pv = lower_bound(g->V[v], g->D[v], u);
    int vu = pv < g->D[v] && g->V[v][pv] == u;

    assert(uv == vu);

    return uv;
}