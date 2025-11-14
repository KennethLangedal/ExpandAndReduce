#include "graph.h"
#include "cliques.h"
#include "algorithms.h"

#include <stdio.h>
#include <stdlib.h>

void rec_mark(graph *g, int *M, int u)
{
    if (g->p1[u] == u)
    {
        M[u] = 1;
    }
    else
    {
        rec_mark(g, M, g->p1[u]);
        rec_mark(g, M, g->p2[u]);
    }
}

void clique_reduce(graph *g)
{
    graph *gc = graph_copy(g);
    cliques *c = cliques_init(gc);

    exit(0);

    int limit = 2;
    int last = 0;

    int *marks = calloc(gc->n * 100, sizeof(int));
    int *late_valid = calloc(gc->n, sizeof(int));
    for (int i = 0; i < gc->n; i++)
    {
        late_valid[i] = 0;
    }

    while (c->n > 1)
    {
        int c1 = rand() % c->n;
        int c2 = rand() % c->n;

        last++;

        if (last > (c->n * c->n) / 2)
        {
            limit++;
            last = 0;

            if (limit > 40)
                break;
        }

        if (c1 == c2 || c->D[c1] > limit || c->D[c2] > limit)
            continue;

        int connections = 0;
        for (int i = 0; i < c->D[c1]; i++)
        {
            int u = c->C[c1][i];
            marks[u] = 1;
        }
        for (int i = 0; i < c->D[c2]; i++)
        {
            int u = c->C[c2][i];
            for (int j = 0; j < gc->D[u]; j++)
            {
                int v = gc->V[u][j];
                connections += marks[v];
            }
        }
        for (int i = 0; i < c->D[c1]; i++)
        {
            int u = c->C[c1][i];
            marks[u] = 0;
        }

        if (((c->D[c1] * c->D[c2]) - connections) > limit * limit)
            continue;

        // int min = c->D[c1] < c->D[c2] ? c->D[c1] : c->D[c2];
        // if (connections < min || connections == 0)
        if (connections == 0)
            continue;

        printf("Merging %d %d with %d connections (%d)\n", c->D[c1], c->D[c2], connections, limit);

        last = 0;

        cliques_merge(c, gc, c1, c2);
        printf("%d %lld %d %d\n", c->n, c->ub, gc->nr, gc->m / 2);
    }
    printf("-----------------------------------\n");

    // for (int u = 0; u < gc->n; u++)
    // {
    //     if (!gc->A[u])
    //         continue;
    //     printf("%d (%lld):", u, gc->W[u]);
    //     for (int i = 0; i < gc->D[u]; i++)
    //     {
    //         int v = gc->V[u][i];
    //         printf(" %d", v);
    //     }
    //     printf("\n");
    // }

    for (int u = 0; u < gc->n; u++)
    {
        if (!gc->A[u])
            continue;

        rec_mark(gc, late_valid, u);
    }

    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;

        if (late_valid[u])
            continue;

        graph_deactivate_vertex(g, u);
    }

    graph_free(gc);
    cliques_free(c);

    free(marks);
    free(late_valid);
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    graph *g = graph_parse(f);
    fclose(f);

    int org_n = g->n;

    printf("%d %d\n", g->nr, g->m / 2);

    for (int t = 0; t < 3; t++)
    {
        for (int u = 0; u < g->n; u++)
        {
            if (!g->A[u])
                continue;
            for (int i = 0; i < g->D[u]; i++)
            {
                int v = g->V[u][i];
                if (g->W[u] > g->W[v] || g->D[v] > g->D[u])
                    continue;

                // Higher or equal weight, smaller or equal degree -> Could be dominated

                if (set_is_subset_except_one(g->V[v], g->D[v], g->V[u], g->D[u], u))
                {
                    graph_deactivate_vertex(g, u);
                    break;
                }
            }
        }
    }

    printf("%d %d\n", g->nr, g->m / 2);

    for (int i = 0; i < 1; i++)
    {
        clique_reduce(g);

        printf("%d %d\n", g->nr, g->m / 2);
    }

    int *FM = malloc(sizeof(int) * g->n);
    int n = 0, m = 0;
    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;
        FM[u] = n++;
        m += g->D[u];
    }

    f = fopen("temp.graph", "w");
    fprintf(f, "%d %d %d\n", n, m / 2, 10);
    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;

        fprintf(f, "%lld ", g->W[u]);
        for (int i = 0; i < g->D[u]; i++)
        {
            fprintf(f, "%d ", FM[g->V[u][i]]);
        }
        fprintf(f, "\n");
    }
    fclose(f);
    free(FM);

    // cliques *c = cliques_init(g);

    // printf("%lld %lld\n", g->nr, g->m / 2);

    // int limit = 5;
    // int last = 0;

    // int *marks = calloc(g->n * 30, sizeof(int));
    // int *late_valid = calloc(g->n, sizeof(int));
    // for (int i = 0; i < g->n; i++)
    // {
    //     late_valid[i] = 0;
    // }

    // while (c->n > 1)
    // {
    //     int c1 = rand() % c->n;
    //     int c2 = rand() % c->n;

    //     last++;

    //     if (last > 100000)
    //     {
    //         limit *= 2;
    //         last = 0;

    //         if (limit > 100)
    //             break;
    //     }

    //     if (c1 == c2 || c->D[c1] > limit || c->D[c2] > limit)
    //         continue;

    //     int connections = 0;
    //     for (int i = 0; i < c->D[c1]; i++)
    //     {
    //         int u = c->C[c1][i];
    //         marks[u] = 1;
    //     }
    //     for (int i = 0; i < c->D[c2]; i++)
    //     {
    //         int u = c->C[c2][i];
    //         for (int j = 0; j < g->D[u]; j++)
    //         {
    //             int v = g->V[u][j];
    //             connections += marks[v];
    //         }
    //     }
    //     for (int i = 0; i < c->D[c1]; i++)
    //     {
    //         int u = c->C[c1][i];
    //         marks[u] = 0;
    //     }

    //     int min = c->D[c1] < c->D[c2] ? c->D[c1] : c->D[c2];
    //     if (connections < (min * min) / 2)
    //         continue;

    //     // int c1_max = c->C[c1][0], c2_max = c->C[c2][0];

    //     // for (int i = 0; i < c->D[c1]; i++)
    //     // {
    //     //     int u = c->C[c1][i];
    //     //     if (g->W[u] > g->W[c1_max])
    //     //         c1_max = u;
    //     // }
    //     // for (int i = 0; i < c->D[c2]; i++)
    //     // {
    //     //     int u = c->C[c2][i];
    //     //     if (g->W[u] > g->W[c2_max])
    //     //         c2_max = u;
    //     // }

    //     // if (!graph_is_neighbor(g, c1_max, c2_max))
    //     //     continue;

    //     last = 0;

    //     // printf("Merging\n");
    //     cliques_merge(c, g, c1, c2);
    //     printf("%lld %lld %lld %lld\n", c->n, c->ub, g->nr, g->m / 2);
    // }

    // for (int u = 0; u < g->n; u++)
    // {
    //     if (!g->A[u])
    //         continue;

    //     rec_mark(g, late_valid, u);
    // }

    // int new_n = 0;
    // for (int u = 0; u < org_n; u++)
    // {
    //     if (!late_valid[u])
    //         continue;

    //     new_n++;
    // }

    // printf("%d %d\n", org_n, new_n);

    graph_free(g);
    // cliques_free(c);

    return 0;
}