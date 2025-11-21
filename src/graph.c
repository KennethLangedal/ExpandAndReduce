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
    g->W = realloc(g->W, sizeof(long long) * g->_a);
    if (g->EW != NULL)
        g->EW = realloc(g->EW, sizeof(long long *) * g->_a);
    g->A = realloc(g->A, sizeof(int) * g->_a);

    g->_A = realloc(g->_A, sizeof(int) * g->_a);

    g->Pl = realloc(g->Pl, sizeof(int) * g->_a);
    g->Pr = realloc(g->Pr, sizeof(int) * g->_a);

    for (int i = g->_a / 2; i < g->_a; i++)
    {
        g->_A[i] = 16;
        g->V[i] = malloc(sizeof(int) * g->_A[i]);
        if (g->EW != NULL)
            g->EW[i] = malloc(sizeof(long long) * g->_A[i]);
    }
}

void graph_append_endpoint(graph *g, int u, int v, long long w)
{
    if (g->_A[u] == g->D[u])
    {
        g->_A[u] *= 2;
        g->V[u] = realloc(g->V[u], sizeof(int) * g->_A[u]);
        if (g->EW != NULL)
            g->EW[u] = realloc(g->EW[u], sizeof(long long) * g->_A[u]);
    }

    g->V[u][g->D[u]] = v;
    if (g->EW != NULL)
        g->EW[u][g->D[u]] = w;

    g->D[u]++;
}

graph *graph_init(int edge_weights)
{
    graph *g = malloc(sizeof(graph));
    *g = (graph){.n = 0, .m = 0, .nr = 0, ._a = 16};

    g->V = malloc(sizeof(int *) * g->_a);
    g->D = malloc(sizeof(int) * g->_a);
    g->W = malloc(sizeof(long long) * g->_a);
    g->EW = NULL;
    if (edge_weights)
        g->EW = malloc(sizeof(long long *) * g->_a);

    g->A = malloc(sizeof(int) * g->_a);

    g->_A = malloc(sizeof(int) * g->_a);

    g->Pl = malloc(sizeof(int) * g->_a);
    g->Pr = malloc(sizeof(int) * g->_a);

    for (int i = 0; i < g->_a; i++)
    {
        g->_A[i] = 16;
        g->V[i] = malloc(sizeof(int) * g->_A[i]);
        if (g->EW != NULL)
            g->EW[i] = malloc(sizeof(long long) * g->_A[i]);
    }

    return g;
}

// Assumes no edge weights
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

// Assumes no edge weights
void graph_construction_add_edge(graph *g, int u, int v)
{
    assert(u < g->n && v < g->n);

    graph_append_endpoint(g, u, v, 0);
    graph_append_endpoint(g, v, u, 0);
}

// Assumes no edge weights
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
    if (g->EW != NULL)
        gc->EW = malloc(sizeof(long long *) * gc->_a);
    gc->A = malloc(sizeof(int) * gc->_a);
    gc->_A = malloc(sizeof(int) * gc->_a);

    gc->Pl = malloc(sizeof(int) * gc->_a);
    gc->Pr = malloc(sizeof(int) * gc->_a);

    for (int i = 0; i < gc->_a; i++)
    {
        gc->_A[i] = g->_A[i];
        gc->V[i] = malloc(sizeof(int) * gc->_A[i]);
        if (gc->EW != NULL)
            gc->EW[i] = malloc(sizeof(long long) * gc->_A[i]);
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

            if (gc->EW != NULL)
                gc->EW[u][i] = g->EW[u][i];
        }
    }

    return gc;
}

void graph_free(graph *g)
{
    for (int i = 0; i < g->_a; i++)
    {
        free(g->V[i]);
        if (g->EW != NULL)
            free(g->EW[i]);
    }
    free(g->V);
    free(g->D);
    free(g->W);
    free(g->EW);
    free(g->A);
    free(g->_A);

    free(g->Pl);
    free(g->Pr);

    free(g);
}

// Everything below this point assumes sorted neighborhoods

void graph_enable_edge_weights(graph *g)
{
    if (g->EW != NULL)
        return;

    g->EW = malloc(sizeof(long long *) * g->_a);
    for (int i = 0; i < g->_a; i++)
    {
        g->EW[i] = malloc(sizeof(long long) * g->_A[i]);
        for (int j = 0; j < g->D[i]; j++)
            g->EW[i][j] = 1;
    }
}

void graph_disable_edge_weights(graph *g)
{
    if (g->EW == NULL)
        return;

    for (int i = 0; i < g->_a; i++)
    {
        free(g->EW[i]);
    }
    free(g->EW);
    g->EW = NULL;
}

void graph_add_vertex(graph *g, long long w, int Pl, int Pr)
{
    graph_construction_add_vertex(g, w);
    g->Pl[g->n - 1] = Pl;
    g->Pr[g->n - 1] = Pr;
}

int graph_insert_endpoint(graph *g, int u, int v, long long w)
{
    int p = lower_bound(g->V[u], g->D[u], v);

    if (p < g->D[u] && g->V[u][p] == v)
        return 0;

    graph_append_endpoint(g, u, v, w);
    for (int i = g->D[u] - 1; i > p; i--)
    {
        g->V[u][i] = g->V[u][i - 1];
        if (g->EW != NULL)
            g->EW[u][i] = g->EW[u][i - 1];
    }
    g->V[u][p] = v;
    if (g->EW != NULL)
        g->EW[u][p] = w;

    return 1;
}

void graph_add_edge(graph *g, int u, int v, long long w)
{
    assert(g->A[u] && g->A[v]);

    if (u == v)
        return;

    int e0 = graph_insert_endpoint(g, u, v, w);
    int e1 = graph_insert_endpoint(g, v, u, w);

    assert(e0 == e1);

    if (e0 && e1)
        g->m++;
}

int graph_remove_endpoint(graph *g, int u, int v)
{
    int p = lower_bound(g->V[u], g->D[u], v);

    if (p >= g->D[u] || g->V[u][p] != v)
        return 0;

    for (int i = p + 1; i < g->D[u]; i++)
    {
        g->V[u][i - 1] = g->V[u][i];
        if (g->EW != NULL)
            g->EW[u][i - 1] = g->EW[u][i];
    }
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
        g->m--;
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
}

void graph_change_vertex_weight(graph *g, int u, long long w)
{
    g->W[u] = w;
}

void graph_increase_edge_weight(graph *g, int u, int v, long long w)
{
    int pu = lower_bound(g->V[u], g->D[u], v);
    int uv = pu < g->D[u] && g->V[u][pu] == v;

    int pv = lower_bound(g->V[v], g->D[v], u);
    int vu = pv < g->D[v] && g->V[v][pv] == u;

    assert(uv == vu && g->EW != NULL);

    g->EW[u][pu] += w;
    g->EW[v][pv] += w;
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