#include "graph.h"
#include "barnes_hut.h"
#include "screen.h"
#include "algorithms.h"
#include "cliques.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <omp.h>
#include <assert.h>

#define WIDTH 1500
#define HEIGHT 900

uint32_t hash_color(uint32_t id)
{
    uint32_t h = id * 2654435761u; // Knuth's multiplicative hash
    uint8_t r = (h >> 16) & 0xFF;
    uint8_t g = (h >> 8) & 0xFF;
    uint8_t b = h & 0xFF;
    return (r << 16) | (g << 8) | b;
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

// TODO, Run reductions outside the active clique (could work)
// TODO, Implement lookahead to only contract "good" pairs

int main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("Graph Vis",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888,
                                         SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

    int running = 1;
    SDL_Event e;

    FILE *f = fopen(argv[1], "r");
    graph *g = graph_parse(f);
    fclose(f);

    printf("%d %d\n", g->n, g->m);

    long long offset = 0;

    for (int t = 0; t < 3; t++)
    {
        reduce_graph(g, &offset);
    }

    printf("%d %d\n", g->nr, g->m);

    cliques *c = cliques_init(g);

    printf("%d %d\n", c->gc->n, c->gc->m);
    printf("%lld\n", offset + c->ub);

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
                        selected = v;
                        bh->Tabu[v] = 1;
                        printf("%d %d\n", c->S[v], v);
                        break;
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
                        printf("%lld\n", offset + c->ub);
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
                int c1 = -1, c2 = -1;
                int delta = 99999;
                int max = 1000;
                while (1)
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
                            int d = c->mnr - (c->gc->D[u] + c->gc->D[v]);
                            if (val && d < delta)
                            {
                                delta = d;
                                c1 = u;
                                c2 = v;
                            }
                        }
                    }

                    if (c1 >= 0)
                    {
                        cliques_merge_prep(c, g, c1, c2, max);
                        cliques_merge(c, g);
                        break;
                    }
                    else
                    {
                        max *= 2;
                    }
                }
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
        screen_render_frame(s, c->gc, Colors, bh->X, bh->Y, draw_edges);
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