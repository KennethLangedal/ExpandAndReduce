#include "graph.h"
#include "reductions.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void print_neighborhood(graph *g, int u)
{
    printf("%d (%lld):", u, g->W[u]);
    for (int i = 0; i < g->D[u]; i++)
    {
        printf(" %d", g->V[u][i]);
    }
    printf("\n");
}

void print_graph(graph *g, long long offset)
{
    printf("----- %d %d %lld -----\n", g->nr, g->m, offset);
    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;
        print_neighborhood(g, u);
    }
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    graph *g = graph_parse(f);
    fclose(f);

    graph *gc1 = graph_copy(g);
    graph *gc2 = graph_copy(g);

    // printf("%s %d %d\n", argv[1], g->nr, g->m);

    long long offset = 0;

    int *T1 = malloc(sizeof(int) * g->n);
    int *T2 = malloc(sizeof(int) * g->n);

    int imp = 1;
    while (imp)
    {
        imp = 0;

        for (int u = 0; u < g->n; u++)
        {
            imp += reduction_neighborhood(g, u, &offset);
            imp += reduction_dominates(g, u);
            imp += reduction_domination(g, u);
            imp += reduction_weight_transfer(g, u, &offset);
            // imp += reduction_add_edges(g, u, T1);
            // imp += reduction_domination_edge(g, u, T1);
        }

        imp = 0;

        printf("%d %d %lld\n", g->nr, g->m, offset);
    }

    int it = 0;
    int level = 1;

    int last_n = g->nr, last_m = g->m;
    while (g->nr > 0)
    {
        if (it++ >= 100000)
        {
            it = 0;
            for (int u = 0; u < g->n; u++)
            {
                reduction_neighborhood(g, u, &offset);
                reduction_domination(g, u);
                reduction_weight_transfer(g, u, &offset);
                // reduction_add_edges(g, u, T1);
                // reduction_domination_edge(g, u, T1);
            }

            if (last_n == g->nr && last_m == g->m)
                level++;

            // if (level > 20)
            //     level = 20;
            // break;

            last_n = g->nr, last_m = g->m;
        }
        // int res = reduction_edge_expansion_search(g, rand() % g->n, T);
        int res = reduction_edge_expansion_neighborhood_alt(g, rand() % g->n, T1, T2, &offset, level);
        if (res)
        {
            printf("\r%10d %10d %10d %10lld %5d", it, g->nr, g->m, offset, level);
            fflush(stdout);
        }
    }

    printf("\r%10d %10d %10d %10lld %5d\n", it, g->nr, g->m, offset, level);

    // printf("%s %d %d %lld\n", argv[1], g->nr, g->m, offset);

    // printf("\n");

    // printf("%d %d\n", g->nr, g->m);

    // imp = 1;
}
