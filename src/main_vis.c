#define _GNU_SOURCE

#include "graph.h"
#include "barnes_hut.h"
#include "screen.h"
#include "algorithms.h"
#include "cliques.h"

#include "is_enumerator.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <omp.h>
#include <assert.h>
#include <math.h>

#define WIDTH 1500
#define HEIGHT 900

/*
-doc2
-alabama2
-MT-D-01

-fandisk
-greenland3
*/

uint32_t hash_color(uint32_t id)
{
    uint32_t h = id * 2654435761u; // Knuth's multiplicative hash
    uint8_t r = (h >> 16) & 0xFF;
    uint8_t g = (h >> 8) & 0xFF;
    uint8_t b = h & 0xFF;
    return (r << 16) | (g << 8) | b;
}

void mark_rec(graph *g, int *marks, int u)
{
    if (g->Pl[u] == u)
    {
        marks[u] = 1;
        return;
    }

    mark_rec(g, marks, g->Pl[u]);
    mark_rec(g, marks, g->Pr[u]);
}

void reduce_graph(graph *g, long long *offset)
{
    for (int u = 0; u < g->n; u++)
    {
        if (!g->A[u])
            continue;

        if (g->D[u] == 0)
        {
            *offset += g->W[u];
            graph_deactivate_vertex(g, u);
            continue;
        }

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

void reduce_clique_graph(graph *g, cliques *c, long long *offset)
{
    for (int i = 0; i < c->n; i++)
    {
        if (c->S[i] == 0)
            continue;

        assert(c->gc->A[i]);

        if (c->S[i] == 1 && g->D[c->C[i][0]] == 0)
        {
            int u = c->C[i][0];
            *offset += g->W[u];
            c->ub -= g->W[u];
            graph_deactivate_vertex(g, u);
            free(c->C[i]);
            c->C[i] = NULL;
            c->S[i] = 0;
            graph_deactivate_vertex(c->gc, i);
            continue;
        }

        for (int j = 0; j < c->S[i]; j++)
        {
            int u = c->C[i][j];
            for (int k = 0; k < c->S[i]; k++)
            {
                int v = c->C[i][k];
                if (u == v || !g->A[v])
                    continue;

                if (g->W[u] > g->W[v] || g->D[v] > g->D[u])
                    continue;

                if (set_is_subset(g->V[v], g->D[v], g->V[u], g->D[u]))
                {
                    graph_deactivate_vertex(g, u);
                    break;
                }
            }
        }
        int new_s = 0;
        for (int j = 0; j < c->S[i]; j++)
        {
            int u = c->C[i][j];
            if (!g->A[u])
            {
                c->gc->W[i]--;
                for (int k = 0; k < g->D[u]; k++)
                {
                    int id = c->FM[g->V[u][k]];
                    graph_increase_edge_weight(c->gc, i, id, -1);
                }
            }
            else
            {
                c->C[i][new_s++] = u;
            }
        }
        c->S[i] = new_s;
    }
}

void clique_graph_search_for_include(graph *g, cliques *c, long long *offset)
{
    long long *NW = calloc(c->n, sizeof(long long));
    for (int i = 0; i < c->n; i++)
    {
        long long max_w = 0, second_max_w = 0;
        for (int j = 0; j < c->S[i]; j++)
        {
            int u = c->C[i][j];
            if (g->W[u] >= max_w)
            {
                second_max_w = max_w;
                max_w = g->W[u];
            }
        }

        if (second_max_w == max_w)
            continue;

        for (int j = 0; j < c->S[i]; j++)
        {
            int u = c->C[i][j];
            if (g->W[u] < max_w)
                continue;

            for (int k = 0; k < g->D[u]; k++)
            {
                int v = g->V[u][k];
                if (g->W[v] > NW[c->FM[v]])
                    NW[c->FM[v]] = g->W[v];
            }

            long long total = second_max_w;
            for (int k = 0; k < g->D[u]; k++)
            {
                int v = g->V[u][k];
                total += NW[c->FM[v]];
                NW[c->FM[v]] = 0;
            }

            if (total <= g->W[u])
                printf("Can include %d from clique %d\n", u, c->FM[u]);
        }
    }
    free(NW);
}

void clique_graph_local_search(graph *g, cliques *c, long long offset)
{
    int *S = malloc(sizeof(int) * c->n);
    long long size_best = offset;
    long long size = offset;

    for (int i = 0; i < c->n; i++)
    {
        S[i] = -1;
    }

    for (int t = 0; t < 100; t++)
    {
        long long max_it = 10000000;
        for (int it = 0; it < max_it; it++)
        {
            double t = 1.0 - ((double)it / (double)max_it);

            int u = rand() % g->n;
            while (!g->A[u])
                u = rand() % g->n;

            long long cost = 0;
            int uc = c->FM[u];

            if (S[uc] == u)
                continue;

            if (S[uc] >= 0)
                cost += g->W[S[uc]];

            for (int i = 0; i < g->D[u]; i++)
            {
                int v = g->V[u][i];
                int vc = c->FM[v];

                if (S[vc] == v)
                    cost += g->W[v];
            }

            long long diff = g->W[u] - cost;
            double sa_diff = (double)diff / 100.0;
            if (diff >= 0 || ((double)rand() / (double)RAND_MAX) < exp(sa_diff / t))
            {
                size += diff;

                S[uc] = u;
                for (int i = 0; i < g->D[u]; i++)
                {
                    int v = g->V[u][i];
                    int vc = c->FM[v];

                    if (S[vc] == v)
                        S[vc] = -1;
                }

                if (size >= size_best)
                {
                    size_best = size;
                    printf("\r%.8lf %10lld %10lld", t, size, size_best);
                    fflush(stdout);
                }
            }

            if ((it % 1000) == 0)
            {
                printf("\r%.8lf %10lld %10lld", t, size, size_best);
                fflush(stdout);
            }
        }
    }

    printf("\n");

    free(S);
}

void print_update(graph *g, cliques *c, long long offset, int original_n)
{
    printf("NR: %d, UB (IS): %lld, GNR: %d, GMR: %d, LB (VC) %lld\n", c->gc->nr, c->ub + offset, g->nr, g->m, original_n - (c->ub + offset));
}

static inline int compare_weights(const void *a, const void *b, void *c)
{
    int u = *(int *)a;
    int v = *(int *)b;
    graph *g = (graph *)c;

    if (g->W[u] > g->W[v])
        return -1;
    if (g->W[u] < g->W[v])
        return 1;
    return 0;
}

void merge_cliqe_solutions(is_enumerator **Solver, int *n_solvers, int *Map, uint32_t *Colors, int c1, int c2, cliques *c, graph *g)
{
    if (Map[c1] < 0 && Map[c2] < 0)
    {
        Map[c1] = *n_solvers;
        Map[c2] = *n_solvers;

        Colors[c1] = hash_color(*n_solvers + 1);
        Colors[c2] = hash_color(*n_solvers + 1);

        qsort_r(c->C[c1], c->S[c1], sizeof(int), compare_weights, g);
        qsort_r(c->C[c2], c->S[c2], sizeof(int), compare_weights, g);

        printf("%d %d\n", c1, c2);

        Solver[*n_solvers] = is_enumerator_init(c->C[c1], c->S[c1], NULL, c->C[c2], c->S[c2], NULL);

        *n_solvers += 1;

        uint8_t *V = calloc(g->n, sizeof(uint8_t));
        is_enumerator_get_next(Solver[*n_solvers - 1], g, V, 1);
        printf("\n");
        free(V);

        element best = Solver[*n_solvers - 1]->E[Solver[*n_solvers - 1]->n - 1];
        printf("%d %d %lld\n", best.a, best.b, best.w);
    }
    else if (Map[c1] >= 0 && Map[c2] >= 0)
    {
        int ma = Map[c1], mb = Map[c2];

        is_enumerator *a = Solver[ma];
        is_enumerator *b = Solver[mb];

        printf("%d %d\n", c1, c2);

        for (int i = 0; i < c->gc->n; i++)
        {
            if (Map[i] == ma || Map[i] == mb)
            {
                Map[i] = *n_solvers;
                Colors[i] = hash_color(*n_solvers + 1);
            }
        }

        Solver[*n_solvers] = is_enumerator_init(NULL, 0, a, NULL, 0, b);
        *n_solvers += 1;

        uint8_t *V = calloc(g->n, sizeof(uint8_t));
        is_enumerator_get_next(Solver[*n_solvers - 1], g, V, 1);
        printf("\n");
        free(V);

        element best = Solver[*n_solvers - 1]->E[Solver[*n_solvers - 1]->n - 1];
        printf("%d %d %lld\n", best.a, best.b, best.w);
    }

    is_enumerator_print_solution(Solver[*n_solvers - 1], g, 0);
    printf("\n");
}

long long upper_bound(cliques *c, graph *g, is_enumerator **Solvers, int *Map, long long offset)
{
    long long ub = offset;
    int *V = calloc(c->n, sizeof(int));
    for (int i = 0; i < c->n; i++)
    {
        if (!c->gc->A[i])
            continue;

        if (Map[i] >= 0)
        {
            if (V[Map[i]])
                continue;
            V[Map[i]] = 1;
            ub += Solvers[Map[i]]->E[0].w;
        }
        else
        {
            long long max = 0;
            for (int j = 0; j < c->S[i]; j++)
            {
                int v = c->C[i][j];
                if (g->W[v] > max)
                    max = g->W[v];
            }
            ub += max;
        }
    }
    free(V);

    return ub;
}

// TODO, Run reductions outside the active clique (could work)
// TODO, Implement lookahead to only contract "good" pairs

int main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow(argv[1],
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888,
                                         SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

    int running = 1;
    SDL_Event e;

    FILE *f = fopen(argv[1], "r");
    graph *g = graph_parse(f);
    fclose(f);

    // for (int i = 0; i < g->n; i++)
    //     g->W[i] = 1;

    int original_n = g->n;

    graph *copy = graph_copy(g);

    printf("%d %d\n", g->n, g->m);

    long long offset = 0;

    for (int t = 0; t < 3; t++)
    {
        reduce_graph(g, &offset);
    }

    printf("%d %d\n", g->nr, g->m);

    cliques *c = cliques_init(g);

    cliques_sort(c, g);

    printf("%d %d\n", c->gc->n, c->gc->m);

    print_update(g, c, offset, original_n);

    clique_graph_local_search(g, c, offset);
    clique_graph_search_for_include(g, c, &offset);

    screen *s = screen_init(HEIGHT, WIDTH);
    barnes_hut *bh = barnes_hut_init(c->gc);

    uint32_t *Colors = malloc(sizeof(uint32_t) * c->gc->n);

    for (int u = 0; u < c->gc->n; u++)
    {
        // Colors[u] = 0x2c2c2c;
        Colors[u] = 0x2c2c2c;
    }

    int mbd = 0, selected = -1;
    int draw_edges = 0;

    int v1 = -1;

    int c1 = -1;

    is_enumerator **Solver = malloc(sizeof(is_enumerator *) * 100);
    int n_solver = 0;
    int *Map = malloc(sizeof(int) * c->gc->n);
    for (int i = 0; i < c->gc->n; i++)
        Map[i] = -1;

    // merge_cliqe_solutions(Solver, &n_solver, Map, Colors, 2, 3, c, g);
    // merge_cliqe_solutions(Solver, &n_solver, Map, Colors, 48, 13, c, g);

    // merge_cliqe_solutions(Solver, &n_solver, Map, Colors, 2, 48, c, g);

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == 1)
            {
                screen_mouse_down(s, e.motion.x, e.motion.y);
                mbd = 1;

                float x = -s->root_x + ((float)e.motion.x / s->zoom);
                float y = -s->root_y + ((float)e.motion.y / s->zoom);

                float min_d = 1e6;
                int v = -1;

                for (int u = 0; u < c->gc->n; u++)
                {
                    if (!c->gc->A[u])
                        continue;
                    float dx = bh->X[u] - x, dy = bh->Y[u] - y;
                    float d = sqrtf(dx * dx + dy * dy) * s->zoom;

                    if (d < min_d)
                    {
                        min_d = d;
                        v = u;
                    }
                }

                if (v >= 0)
                {
                    double rx = sqrt((double)c->gc->W[v] / M_PI);
                    if (min_d <= rx * s->zoom + 2)
                    {
                        // Combine version starts here
                        // if (c1 < 0)
                        // {
                        //     c1 = v;
                        //     Colors[c1] = 0x00ff00;
                        // }
                        // else if (c1 >= 0 && v != c1)
                        // {
                        //     merge_cliqe_solutions(Solver, &n_solver, Map, Colors, c1, v, c, g);
                        //     c1 = -1;

                        //     long long ub = upper_bound(c, g, Solver, Map, offset);

                        //     printf("NR: %d, UB (IS): %lld, GNR: %d, GMR: %d, LB (VC) %lld\n", c->gc->nr, ub, g->nr, g->m, original_n - ub);
                        // }

                        // Until here

                        selected = v;
                        bh->Tabu[v] = 1;
                        printf("%d\n", c->S[v]);
                        // break;
                    }
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == 1)
            {
                screen_mouse_up(s);
                mbd = 0;

                if (selected >= 0)
                {
                    bh->Tabu[selected] = 0;
                    if (v1 < 0)
                    {
                        v1 = selected;
                        Colors[v1] = 0x00ff00;
                    }
                    else if (selected != v1)
                    {
                        cliques_merge_prep(c, g, v1, selected, 99999999);
                        cliques_merge(c, g);
                        Colors[v1] = 0x2c2c2c;
                        v1 = -1;
                        print_update(g, c, offset, original_n);
                    }
                    selected = -1;
                }
            }
            else if (e.type == SDL_MOUSEMOTION && s->drag > 0)
            {
                screen_mouse_move(s, e.motion.x, e.motion.y);
                bh->rest_l = s->sliders[0];
                bh->k_spring = (s->sliders[1] * 4.0f) / 200.0f;
                bh->k_repel = (s->sliders[2] * 4.0f) / 200.0f;
                bh->k_gravity = (s->sliders[3] * 4.0f) / 200.0f;
                bh->theta = (s->sliders[4] * 4.0f) / 200.0f;
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r)
            {
                if (v1 >= 0)
                    Colors[v1] = 0x2c2c2c;
                v1 = -1;
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_s)
            {
                int max = (1 << 10);
                while (c->gc->nr > 0)
                {
                    int c1 = -1, c2 = -1;
                    int delta_n = 99999, delta_m = 99999;

                    if (c->gc->nr > 100)
                    {
                        for (int u = 0; u < c->n; u++)
                        {
                            if (!c->gc->A[u])
                                continue;

                            for (int i = 0; i < c->gc->D[u]; i++)
                            {
                                int v = c->gc->V[u][i];
                                if (u >= v)
                                    continue;

                                int val = cliques_merge_prep(c, g, u, v, max);
                                int lc = c->S[u] > c->S[v] ? c->S[u] : c->S[v];
                                int d_n = c->mnr - lc;
                                int d_m = c->mmd;

                                int ub_red = 1;
                                if (c->S[u] <= 1 || g->W[c->C[u][0]] == g->W[c->C[u][1]])
                                    ub_red = 0;
                                else if (c->S[v] <= 1 || g->W[c->C[v][0]] == g->W[c->C[v][1]])
                                    ub_red = 0;
                                else if (!graph_is_neighbor(g, c->C[u][0], c->C[v][0]))
                                    ub_red = 0;

                                if (ub_red && val && d_n < delta_n)
                                {
                                    delta_n = d_n;
                                    c1 = u;
                                    c2 = v;
                                }
                            }
                        }
                    }
                    else
                    {
                        for (int u = 0; u < c->n; u++)
                        {
                            if (!c->gc->A[u])
                                continue;

                            for (int v = u + 1; v < c->n; v++)
                            {
                                if (!c->gc->A[v])
                                    continue;

                                int val = cliques_merge_prep(c, g, u, v, max);
                                int lc = c->S[u] > c->S[v] ? c->S[u] : c->S[v];
                                int d_n = c->mnr - lc;
                                int d_m = c->mmd;
                                if (val && d_n + d_m < delta_m)
                                {
                                    delta_m = d_n + d_m;
                                    c1 = u;
                                    c2 = v;
                                }
                            }
                        }
                    }

                    if (c1 >= 0)
                    {
                        cliques_merge_prep(c, g, c1, c2, max);
                        cliques_merge(c, g);
                        print_update(g, c, offset, original_n);
                        break;
                    }
                    else
                    {
                        max *= 2;
                    }
                }
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_a)
            {
                int max = (1 << 8);
                while (c->gc->nr > 0)
                {
                    int c1 = -1, c2 = -1;
                    int delta_n = 99999, delta_m = 99999;

                    for (int i = 0; i < max; i++)
                    {
                        int u = rand() % c->n;
                        int it = 0;
                        while (it++ < max && (!c->gc->A[u] || c->gc->D[u] == 0))
                            u = rand() % c->n;

                        if (it >= max)
                            continue;

                        assert(c->gc->A[u] && c->S[u] > 0);

                        int v = c->gc->V[u][rand() % c->gc->D[u]];
                        if ((rand() % 10) == 0)
                        {
                            int d2 = c->gc->V[v][rand() % c->gc->D[v]];
                            if (d2 != u)
                                v = d2;
                        }

                        int val = cliques_merge_prep(c, g, u, v, max);
                        int lc = c->S[u] > c->S[v] ? c->S[u] : c->S[v];
                        int d_n = c->mnr - lc;
                        int d_m = c->mmd;
                        if (val && d_n + d_m < delta_m)
                        {
                            delta_m = d_n + d_m;
                            c1 = u;
                            c2 = v;
                        }
                    }

                    if (c1 >= 0)
                    {
                        cliques_merge_prep(c, g, c1, c2, max);
                        cliques_merge(c, g);
                        print_update(g, c, offset, original_n);
                        // break;
                    }
                    else
                    {
                        break;
                        // max *= 2;
                    }
                }
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_k)
            {
                reduce_clique_graph(g, c, &offset);
                print_update(g, c, offset, original_n);
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_i)
            {
                f = fopen("clique_ilp.lp", "w");
                fprintf(f, "Maximize obj: %lld + ", offset);
                int first = 1;
                for (int u = 0; u < g->n; u++)
                {
                    if (!g->A[u])
                        continue;

                    if (!first)
                        fprintf(f, " + %lldx%d", g->W[u], u);
                    else
                        fprintf(f, "%lldx%d", g->W[u], u);

                    first = 0;
                }
                fprintf(f, "\nSubject To\n");
                long long count = 0;
                for (int i = 0; i < c->n; i++)
                {
                    if (c->S[i] == 0)
                        continue;
                    fprintf(f, "c%lld: ", count++);

                    first = 1;
                    for (int j = 0; j < c->S[i]; j++)
                    {
                        if (!first)
                            fprintf(f, " + x%d", c->C[i][j]);
                        else
                            fprintf(f, "x%d", c->C[i][j]);

                        first = 0;
                    }

                    fprintf(f, " <= 1\n");
                }

                for (int u = 0; u < g->n; u++)
                {
                    if (!g->A[u])
                        continue;

                    for (int i = 0; i < g->D[u]; i++)
                    {
                        int v = g->V[u][i];
                        if (u >= v)
                            continue;

                        fprintf(f, "c%lld: x%d + x%d <= 1\n", count++, u, v);
                    }
                }

                fprintf(f, "Binaries");
                for (int u = 0; u < g->n; u++)
                {
                    if (!g->A[u])
                        continue;

                    fprintf(f, " x%d", u);
                }

                fprintf(f, "\nEnd\n");

                fclose(f);
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_l)
            {
                f = fopen("clique.lp", "w");
                fprintf(f, "Maximize obj: %lld + ", offset);
                int first = 1;
                for (int u = 0; u < g->n; u++)
                {
                    if (!g->A[u])
                        continue;

                    if (!first)
                        fprintf(f, " + %lldx%d", g->W[u], u);
                    else
                        fprintf(f, "%lldx%d", g->W[u], u);

                    first = 0;
                }
                fprintf(f, "\nSubject To\n");
                long long count = 0;
                for (int i = 0; i < c->n; i++)
                {
                    if (c->S[i] == 0)
                        continue;
                    fprintf(f, "c%lld: ", count++);

                    first = 1;
                    for (int j = 0; j < c->S[i]; j++)
                    {
                        if (!first)
                            fprintf(f, " + x%d", c->C[i][j]);
                        else
                            fprintf(f, "x%d", c->C[i][j]);

                        first = 0;
                    }

                    fprintf(f, " <= 1\n");
                }

                for (int u = 0; u < g->n; u++)
                {
                    if (!g->A[u])
                        continue;

                    for (int i = 0; i < g->D[u]; i++)
                    {
                        int v = g->V[u][i];
                        if (u >= v)
                            continue;

                        fprintf(f, "c%lld: x%d + x%d <= 1\n", count++, u, v);
                    }
                }

                fprintf(f, "End\n");

                fclose(f);
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_b)
            {
                int *marks = calloc(original_n, sizeof(int));
                for (int u = 0; u < g->n; u++)
                {
                    if (!g->A[u])
                        continue;

                    mark_rec(g, marks, u);
                }

                graph_free(g);
                cliques_free(c);
                barnes_hut_free(bh);

                g = graph_copy(copy);
                for (int u = 0; u < original_n; u++)
                {
                    if (marks[u])
                        continue;

                    graph_deactivate_vertex(g, u);
                }

                c = cliques_init(g);
                bh = barnes_hut_init(c->gc);

                free(marks);
            }
            else if (e.type == SDL_MOUSEMOTION && mbd)
            {
                if (selected >= 0)
                {
                    bh->X[selected] += (float)e.motion.xrel / s->zoom;
                    bh->Y[selected] += (float)e.motion.yrel / s->zoom;
                }
                else
                {
                    s->root_x += (float)e.motion.xrel / s->zoom;
                    s->root_y += (float)e.motion.yrel / s->zoom;
                }
            }
            else if (e.type == SDL_MOUSEWHEEL)
            {
                if (e.wheel.y > 0 && s->zoom < 10.0f)
                    s->zoom *= 1.1;
                else if (e.wheel.y < 0 && s->zoom > 0.01f)
                    s->zoom *= 0.9;
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_e)
            {
                draw_edges = !draw_edges;
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q)
            {
                running = 0;
            }
            else if (e.type == SDL_QUIT)
            {
                running = 0;
            }
        }

        double t0 = omp_get_wtime();
        barnes_hut_step(bh, c->gc);
        double t1 = omp_get_wtime();
        screen_render_frame(s, c->gc, c, g, Colors, bh->X, bh->Y, draw_edges);
        double t2 = omp_get_wtime();

        // printf("\r%5.3lf %5.3lf", t1 - t0, t2 - t1);
        // fflush(stdout);

        SDL_UpdateTexture(tex, NULL, s->Pixels, WIDTH * sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    printf("\n");

    graph_free(g);
    barnes_hut_free(bh);
    screen_free(s);

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}