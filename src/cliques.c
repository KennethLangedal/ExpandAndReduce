#define _GNU_SOURCE

#include "cliques.h"
#include "algorithms.h"

#include <stdlib.h>
#include <assert.h>

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
        for (int j = 0; j < c->D[i]; j++)
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
        for (int j = 0; j < c->D[i]; j++)
        {
            int u = c->C[i][j];
            for (int k = j + 1; k < c->D[i]; k++)
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

    for (int i = 0; i < 100; i++)
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
    c->D = malloc(sizeof(int) * c->n);
    c->FM = Clique;
    c->V = malloc(sizeof(int *) * c->n);
    c->EW = malloc(sizeof(int *) * c->n);

    for (int i = 0; i < c->n; i++)
    {
        c->D[i] = Clique_size[i + 1];
        c->C[i] = malloc(sizeof(int) * c->D[i]);
    }

    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;

        Clique[u]--;
        int cu = Clique[u];
        c->C[cu][Count[cu]] = u;
        Count[cu]++;
    }

    int *marks = calloc(c->n, sizeof(int));

    for (int i = 0; i < c->n; i++)
    {
        int count = 0;
        for (int j = 0; j < c->D[i]; j++)
        {
            int u = c->C[i][j];
            for (int k = 0; k < g->D[u]; k++)
            {
                int v = g->V[u][k];
                if (marks[c->FM[v]] == 0)
                    count++;
                marks[c->FM[v]]++;
            }
        }

        c->V[i] = malloc(sizeof(int) * count);
        c->EW[i] = malloc(sizeof(int) * count);

        count = 0;
        for (int j = 0; j < c->D[i]; j++)
        {
            int u = c->C[i][j];
            for (int k = 0; k < g->D[u]; k++)
            {
                int v = g->V[u][k];
                if (marks[c->FM[v]] == 0)
                    continue;

                c->V[i][count] = c->FM[v];
                c->EW[i][count] = marks[c->FM[v]];
                marks[c->FM[v]] = 0;
            }
        }
    }

    update_ub(c, g);
    printf("%d %lld\n", nc, c->ub);

    clear_clique_edges(g, c);

    free(Clique_size);
    free(Count);
    free(marks);

    return c;
}

void cliques_free(cliques *c)
{
    for (int i = 0; i < c->n; i++)
    {
        free(c->C[i]);
        free(c->V[i]);
        free(c->EW[i]);
    }

    free(c->C);
    free(c->D);
    free(c->FM);
    free(c->V);
    free(c->EW);
}

void print_elements(graph *g, int u)
{
    if (g->p1[u] == u)
    {
        printf(" %d", u);
    }
    else
    {
        print_elements(g, g->p1[u]);
        print_elements(g, g->p2[u]);
    }
}

void print(cliques *c, graph *g)
{
    printf("Graph:\n");
    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;
        printf("%d (", u);
        print_elements(g, u);
        printf(") %lld:", g->W[u]);

        for (int i = 0; i < g->D[u]; i++)
        {
            printf(" %d", g->V[u][i]);
        }
        printf("\n");
    }

    printf("Cliques:\n");
    for (int i = 0; i < c->n; i++)
    {
        printf("%d: ", i);
        for (int j = 0; j < c->D[i]; j++)
        {
            printf(" %d", c->C[i][j]);
        }
        printf("\n");
    }
}

void cliques_merge(cliques *c, graph *g, int c1, int c2)
{
    // printf("Merging %d and %d\n", c1, c2);
    // print(c, g);

    int *marks = calloc(g->n, sizeof(int));
    long long c1_max = 0, c2_max = 0;
    for (int i = 0; i < c->D[c1]; i++)
    {
        int u = c->C[c1][i];
        marks[u] = 1;
        if (g->W[u] > c1_max)
            c1_max = g->W[u];
    }
    for (int i = 0; i < c->D[c2]; i++)
    {
        int u = c->C[c2][i];
        marks[u] = 1;
        if (g->W[u] > c2_max)
            c2_max = g->W[u];
    }
    c->ub -= c1_max;
    c->ub -= c2_max;

    // printf("Adding new vertices\n");

    int s = g->n;
    long long new_max = c2_max > c1_max ? c2_max : c1_max;
    for (int i = 0; i < c->D[c1]; i++)
    {
        int u = c->C[c1][i];
        for (int j = 0; j < c->D[c2]; j++)
        {
            int v = c->C[c2][j];
            assert(v < g->n);
            assert(u != v);
            if (graph_is_neighbor(g, u, v))
                continue;

            int w = g->n;
            graph_add_vertex(g, g->W[u] + g->W[v], u, v);

            if (g->W[u] + g->W[v] > new_max)
            {
                new_max = g->W[u] + g->W[v];
            }

            for (int k = 0; k < g->D[u]; k++)
            {
                int x = g->V[u][k];
                if (marks[x])
                    continue;

                graph_add_edge(g, w, x);
            }
            for (int k = 0; k < g->D[v]; k++)
            {
                int x = g->V[v][k];
                if (marks[x])
                    continue;

                if (graph_is_neighbor(g, w, x))
                    continue;

                graph_add_edge(g, w, x);
            }
        }
    }

    for (int i = 0; i < c->D[c1]; i++)
    {
        int u = c->C[c1][i];
        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];
            if (!marks[v])
                continue;

            graph_remove_edge(g, u, v);
            j--;
        }
    }
    for (int i = 0; i < c->D[c2]; i++)
    {
        int u = c->C[c2][i];
        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];
            if (!marks[v])
                continue;

            graph_remove_edge(g, u, v);
            j--;
        }
    }

    // printf("Constructing clique\n");

    int n = c->D[c1] + c->D[c2] + (g->n - s);
    int *tmp = malloc(sizeof(int) * n);
    for (int i = 0; i < c->D[c1]; i++)
        tmp[i] = c->C[c1][i];
    for (int i = 0; i < c->D[c2]; i++)
        tmp[c->D[c1] + i] = c->C[c2][i];
    for (int i = 0; i < g->n - s; i++)
        tmp[c->D[c1] + c->D[c2] + i] = s + i;

    // printf("Removing edges\n");

    // for (int i = 0; i < n; i++)
    // {
    //     int u = tmp[i];
    //     if (g->W[u] > new_max)
    //         new_max = g->W[u];
    //     for (int j = i + 1; j < n; j++)
    //     {
    //         int v = tmp[j];
    //         if (graph_is_neighbor(g, u, v))
    //             graph_remove_edge(g, u, v);
    //     }
    // }

    c->ub += new_max;

    free(c->C[c1]);
    c->C[c1] = tmp;
    c->D[c1] = n;

    // printf("Checking domination, before %d\n", n);

    for (int i = 0; i < n; i++)
    {
        int u = tmp[i];

        for (int j = 0; j < n; j++)
        {
            int v = tmp[j];
            if (u == v)
                continue;

            if (g->W[u] > g->W[v] || g->D[v] > g->D[u])
                continue;

            if (set_is_subset_except_one(g->V[v], g->D[v], g->V[u], g->D[u], u))
            {
                graph_deactivate_vertex(g, u);
                tmp[i] = tmp[n - 1];
                n--;
                i--;
                break;
            }
        }
    }

    c->D[c1] = n;

    // if (n < 10)
    // {
    //     for (int i = 0; i < n; i++)
    //     {
    //         int u = tmp[i];
    //         printf("%d (%lld): ", u, g->W[u]);
    //         for (int j = 0; j < g->D[u]; j++)
    //         {
    //             int v = g->V[u][j];
    //             printf("%d ", v);
    //         }
    //         printf("\n");
    //     }
    // }

    // printf("After %d\n", n);

    c->D[c2] = c->D[c->n - 1];
    c->C[c2] = c->C[c->n - 1];
    c->n--;

    free(marks);

    // print(c, g);

    // static int t = 3;
    // t--;

    // if (t == 0)
    //     exit(0);
}
