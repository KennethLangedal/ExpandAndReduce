#include "densify.h"
#include "algorithms.h"

#include <interfaces/highs_c_api.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>

extern volatile sig_atomic_t tle __attribute__((weak));

static inline int is_tle(void)
{
    return (&tle != NULL && tle != 0);
}

densify_options densify_default_options(void)
{
    densify_options opt;
    opt.max_distance = 3;
    opt.max_degree = 0;
    opt.max_edges_to_add = 16;
    opt.max_passes = 1;
    opt.keep_edges = 1;
    opt.use_heuristic = 1;
    opt.num_restarts = 4;
    opt.time_limit_sec = 0.1;
    opt.verbose = 0;
    return opt;
}

struct densify_context_struct
{
    void *highs;
    double inf;

    // Graph vertex sized buffers
    int capacity_n;
    int *stamp;
    int cur_tag;
    int *queue;
    int *dist;
    int *candidates;
    int *C;
    int *Cu;
    int *Cv;
    int *conflicts;
    int *in_set;
    int *order;
    int *col_tag;
    int *col_map;

    // HiGHS batch column buffers
    int col_capacity;
    double *col_cost;
    double *col_lower;
    double *col_upper;
    HighsInt *col_integrality;

    // HiGHS batch row buffers
    int row_capacity;
    int nz_capacity;
    double *row_lower;
    double *row_upper;
    HighsInt *row_starts;
    HighsInt *row_indices;
    double *row_values;
};

densify_context *densify_context_create(int initial_n)
{
    densify_context *ctx = malloc(sizeof(densify_context));
    if (!ctx)
        return NULL;

    ctx->highs = Highs_create();
    Highs_setBoolOptionValue(ctx->highs, "output_flag", 0);
    ctx->inf = Highs_getInfinity(ctx->highs);

    ctx->capacity_n = initial_n > 128 ? initial_n : 128;
    ctx->cur_tag = 0;
    ctx->stamp = calloc(ctx->capacity_n, sizeof(int));
    ctx->queue = malloc(sizeof(int) * ctx->capacity_n);
    ctx->dist = malloc(sizeof(int) * ctx->capacity_n);
    ctx->candidates = malloc(sizeof(int) * ctx->capacity_n);
    ctx->C = malloc(sizeof(int) * ctx->capacity_n);
    ctx->Cu = malloc(sizeof(int) * ctx->capacity_n);
    ctx->Cv = malloc(sizeof(int) * ctx->capacity_n);
    ctx->conflicts = calloc(ctx->capacity_n, sizeof(int));
    ctx->in_set = calloc(ctx->capacity_n, sizeof(int));
    ctx->order = malloc(sizeof(int) * ctx->capacity_n);
    ctx->col_tag = calloc(ctx->capacity_n, sizeof(int));
    ctx->col_map = malloc(sizeof(int) * ctx->capacity_n);

    ctx->col_capacity = 256;
    ctx->col_cost = malloc(sizeof(double) * ctx->col_capacity);
    ctx->col_lower = malloc(sizeof(double) * ctx->col_capacity);
    ctx->col_upper = malloc(sizeof(double) * ctx->col_capacity);
    ctx->col_integrality = malloc(sizeof(HighsInt) * ctx->col_capacity);

    ctx->row_capacity = 2048;
    ctx->nz_capacity = 4096;
    ctx->row_lower = malloc(sizeof(double) * ctx->row_capacity);
    ctx->row_upper = malloc(sizeof(double) * ctx->row_capacity);
    ctx->row_starts = malloc(sizeof(HighsInt) * ctx->row_capacity);
    ctx->row_indices = malloc(sizeof(HighsInt) * ctx->nz_capacity);
    ctx->row_values = malloc(sizeof(double) * ctx->nz_capacity);

    return ctx;
}

static void densify_context_ensure_n(densify_context *ctx, int n)
{
    if (n <= ctx->capacity_n)
        return;

    int old_cap = ctx->capacity_n;
    while (ctx->capacity_n < n)
        ctx->capacity_n *= 2;

    ctx->stamp = realloc(ctx->stamp, sizeof(int) * ctx->capacity_n);
    ctx->queue = realloc(ctx->queue, sizeof(int) * ctx->capacity_n);
    ctx->dist = realloc(ctx->dist, sizeof(int) * ctx->capacity_n);
    ctx->candidates = realloc(ctx->candidates, sizeof(int) * ctx->capacity_n);
    ctx->C = realloc(ctx->C, sizeof(int) * ctx->capacity_n);
    ctx->Cu = realloc(ctx->Cu, sizeof(int) * ctx->capacity_n);
    ctx->Cv = realloc(ctx->Cv, sizeof(int) * ctx->capacity_n);
    ctx->conflicts = realloc(ctx->conflicts, sizeof(int) * ctx->capacity_n);
    ctx->in_set = realloc(ctx->in_set, sizeof(int) * ctx->capacity_n);
    ctx->order = realloc(ctx->order, sizeof(int) * ctx->capacity_n);
    ctx->col_tag = realloc(ctx->col_tag, sizeof(int) * ctx->capacity_n);
    ctx->col_map = realloc(ctx->col_map, sizeof(int) * ctx->capacity_n);

    memset(ctx->stamp + old_cap, 0, sizeof(int) * (ctx->capacity_n - old_cap));
    memset(ctx->conflicts + old_cap, 0, sizeof(int) * (ctx->capacity_n - old_cap));
    memset(ctx->in_set + old_cap, 0, sizeof(int) * (ctx->capacity_n - old_cap));
    memset(ctx->col_tag + old_cap, 0, sizeof(int) * (ctx->capacity_n - old_cap));
}

static void densify_context_ensure_cols(densify_context *ctx, int needed_cols)
{
    if (needed_cols <= ctx->col_capacity)
        return;

    while (ctx->col_capacity < needed_cols)
        ctx->col_capacity *= 2;

    ctx->col_cost = realloc(ctx->col_cost, sizeof(double) * ctx->col_capacity);
    ctx->col_lower = realloc(ctx->col_lower, sizeof(double) * ctx->col_capacity);
    ctx->col_upper = realloc(ctx->col_upper, sizeof(double) * ctx->col_capacity);
    ctx->col_integrality = realloc(ctx->col_integrality, sizeof(HighsInt) * ctx->col_capacity);
}

static void densify_context_ensure_rows(densify_context *ctx, int needed_rows, int needed_nz)
{
    if (needed_rows > ctx->row_capacity)
    {
        while (ctx->row_capacity < needed_rows)
            ctx->row_capacity *= 2;

        ctx->row_lower = realloc(ctx->row_lower, sizeof(double) * ctx->row_capacity);
        ctx->row_upper = realloc(ctx->row_upper, sizeof(double) * ctx->row_capacity);
        ctx->row_starts = realloc(ctx->row_starts, sizeof(HighsInt) * ctx->row_capacity);
    }

    if (needed_nz > ctx->nz_capacity)
    {
        while (ctx->nz_capacity < needed_nz)
            ctx->nz_capacity *= 2;

        ctx->row_indices = realloc(ctx->row_indices, sizeof(HighsInt) * ctx->nz_capacity);
        ctx->row_values = realloc(ctx->row_values, sizeof(double) * ctx->nz_capacity);
    }
}

void densify_context_destroy(densify_context *ctx)
{
    if (!ctx)
        return;

    if (ctx->highs)
        Highs_destroy(ctx->highs);

    free(ctx->stamp);
    free(ctx->queue);
    free(ctx->dist);
    free(ctx->candidates);
    free(ctx->C);
    free(ctx->Cu);
    free(ctx->Cv);
    free(ctx->conflicts);
    free(ctx->in_set);
    free(ctx->order);
    free(ctx->col_tag);
    free(ctx->col_map);

    free(ctx->col_cost);
    free(ctx->col_lower);
    free(ctx->col_upper);
    free(ctx->col_integrality);

    free(ctx->row_lower);
    free(ctx->row_upper);
    free(ctx->row_starts);
    free(ctx->row_indices);
    free(ctx->row_values);

    free(ctx);
}

int densify_is_dominated(graph *g, int u)
{
    if (!g->A[u])
        return 0;

    for (int i = 0; i < g->D[u]; i++)
    {
        int v = g->V[u][i];
        if (!g->A[v])
            continue;

        // Condition for v to dominate u:
        // 1. W[v] > W[u] OR (W[v] == W[u] && v < u)
        // 2. N(v) \ {u} \subseteq N(u) \ {v}
        if (g->W[v] > g->W[u] || (g->W[v] == g->W[u] && v < u))
        {
            if (set_is_subset_except_one(g->V[v], g->D[v], g->V[u], g->D[u], u))
            {
                graph_deactivate_vertex(g, u);
                return 1;
            }
        }
    }
    return 0;
}

// Fast greedy + 1-swap MWIS check on candidate subset S of C
static inline int mwis_check(graph *g, int *S, int S_size, long long target,
                            int *conflicts, int *in_set, int *order, int base_tag, int num_restarts)
{
    for (int r = 0; r < num_restarts; r++)
    {
        int cur_tag = base_tag + r * 10 + 1;
        long long cur_weight = 0;

        for (int i = 0; i < S_size; i++)
        {
            conflicts[S[i]] = 0;
            order[i] = S[i];
        }

        if (r > 0)
        {
            // Fisher-Yates random shuffle
            for (int i = 0; i < S_size - 1; i++)
            {
                int j = i + (rand() % (S_size - i));
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
        else
        {
            // Insertion sort by weight descending
            for (int i = 0; i < S_size; i++)
            {
                for (int j = i + 1; j < S_size; j++)
                {
                    if (g->W[order[j]] > g->W[order[i]])
                    {
                        int tmp = order[i];
                        order[i] = order[j];
                        order[j] = tmp;
                    }
                }
            }
        }

        // 1. Greedy independent set construction
        for (int i = 0; i < S_size; i++)
        {
            int w = order[i];
            if (conflicts[w] == 0)
            {
                in_set[w] = cur_tag;
                cur_weight += g->W[w];
                if (cur_weight >= target)
                    return 1;

                for (int k = 0; k < g->D[w]; k++)
                    conflicts[g->V[w][k]]++;
            }
        }

        // 2. 1-swap improvement
        for (int i = 0; i < S_size; i++)
        {
            int y = order[i];
            if (in_set[y] == cur_tag)
                continue;

            // If y has exactly 1 chosen neighbor z
            if (conflicts[y] == 1)
            {
                int z = -1;
                for (int k = 0; k < g->D[y]; k++)
                {
                    int nbr = g->V[y][k];
                    if (in_set[nbr] == cur_tag)
                    {
                        z = nbr;
                        break;
                    }
                }

                if (z >= 0 && g->W[y] > g->W[z])
                {
                    // Swap z out, y in
                    in_set[z] = 0;
                    cur_weight -= g->W[z];
                    for (int k = 0; k < g->D[z]; k++)
                        conflicts[g->V[z][k]]--;

                    in_set[y] = cur_tag;
                    cur_weight += g->W[y];
                    for (int k = 0; k < g->D[y]; k++)
                        conflicts[g->V[y][k]]++;

                    if (cur_weight >= target)
                        return 1;

                    // Greedily admit any newly conflict-free vertices
                    for (int j = 0; j < S_size; j++)
                    {
                        int cand = order[j];
                        if (in_set[cand] != cur_tag && conflicts[cand] == 0)
                        {
                            in_set[cand] = cur_tag;
                            cur_weight += g->W[cand];
                            for (int k = 0; k < g->D[cand]; k++)
                                conflicts[g->V[cand][k]]++;

                            if (cur_weight >= target)
                                return 1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

int densify_test_pair_heuristic(densify_context *ctx, graph *g, int u, int v, const densify_options *opt)
{
    assert(g != NULL);
    assert(ctx != NULL);

    if (u < 0 || u >= g->n || v < 0 || v >= g->n || u == v)
        return 0;

    if (!g->A[u] || !g->A[v] || graph_is_neighbor(g, u, v))
        return 0;

    if (opt && opt->max_degree > 0 && (g->D[u] > opt->max_degree || g->D[v] > opt->max_degree))
        return 0;

    densify_context_ensure_n(ctx, g->n);

    if (ctx->cur_tag > 1000000000)
    {
        memset(ctx->stamp, 0, sizeof(int) * ctx->capacity_n);
        memset(ctx->in_set, 0, sizeof(int) * ctx->capacity_n);
        ctx->cur_tag = 0;
    }
    int tag_union = ++ctx->cur_tag;

    ctx->stamp[u] = tag_union;
    ctx->stamp[v] = tag_union;
    for (int i = 0; i < g->D[u]; i++)
        ctx->stamp[g->V[u][i]] = tag_union;
    for (int i = 0; i < g->D[v]; i++)
        ctx->stamp[g->V[v][i]] = tag_union;

    int C_size = 0, Cu_size = 0, Cv_size = 0;
    long long w_C = 0, w_Cu = 0, w_Cv = 0;

    // Scan N(u)
    for (int i = 0; i < g->D[u]; i++)
    {
        int r = g->V[u][i];
        if (!g->A[r])
            continue;

        int ok = 1;
        for (int j = 0; j < g->D[r]; j++)
        {
            if (ctx->stamp[g->V[r][j]] != tag_union)
            {
                ok = 0;
                break;
            }
        }
        if (ok)
        {
            ctx->C[C_size++] = r;
            w_C += g->W[r];
            if (!graph_is_neighbor(g, v, r))
            {
                ctx->Cu[Cu_size++] = r;
                w_Cu += g->W[r];
            }
        }
    }

    // Scan N(v)
    for (int i = 0; i < g->D[v]; i++)
    {
        int r = g->V[v][i];
        if (!g->A[r] || graph_is_neighbor(g, u, r))
            continue;

        int ok = 1;
        for (int j = 0; j < g->D[r]; j++)
        {
            if (ctx->stamp[g->V[r][j]] != tag_union)
            {
                ok = 0;
                break;
            }
        }
        if (ok)
        {
            ctx->C[C_size++] = r;
            w_C += g->W[r];
            ctx->Cv[Cv_size++] = r;
            w_Cv += g->W[r];
        }
    }

    if (C_size == 0)
        return 0;

    int restarts = (opt && opt->num_restarts > 0) ? opt->num_restarts : 4;
    int tag_mwis = ctx->cur_tag += 50;

    // Case 1: replace u only (target: W[u])
    if (w_Cu >= g->W[u])
    {
        if (mwis_check(g, ctx->Cu, Cu_size, g->W[u], ctx->conflicts, ctx->in_set, ctx->order, tag_mwis, restarts))
            return 1;
    }

    // Case 2: replace v only (target: W[v])
    if (w_Cv >= g->W[v])
    {
        if (mwis_check(g, ctx->Cv, Cv_size, g->W[v], ctx->conflicts, ctx->in_set, ctx->order, tag_mwis + 15, restarts))
            return 1;
    }

    // Case 3: replace both u and v (target: W[u] + W[v])
    if (w_C >= g->W[u] + g->W[v])
    {
        if (mwis_check(g, ctx->C, C_size, g->W[u] + g->W[v], ctx->conflicts, ctx->in_set, ctx->order, tag_mwis + 30, restarts))
            return 1;
    }

    return 0;
}

int densify_test_pair_ctx(densify_context *ctx, graph *g, int u, int v, const densify_options *opt)
{
    if (opt && opt->use_heuristic)
    {
        return densify_test_pair_heuristic(ctx, g, u, v, opt);
    }

    assert(g != NULL);
    assert(ctx != NULL);

    if (u < 0 || u >= g->n || v < 0 || v >= g->n || u == v)
        return 0;

    if (!g->A[u] || !g->A[v])
        return 0;

    if (graph_is_neighbor(g, u, v))
        return 0;

    if (opt && opt->max_degree > 0 && (g->D[u] > opt->max_degree || g->D[v] > opt->max_degree))
        return 0;

    densify_context_ensure_n(ctx, g->n);

    // 1. Stamp {u, v} U N(u) U N(v)
    if (ctx->cur_tag > 1000000000)
    {
        memset(ctx->stamp, 0, sizeof(int) * ctx->capacity_n);
        memset(ctx->col_tag, 0, sizeof(int) * ctx->capacity_n);
        ctx->cur_tag = 0;
    }
    int tag_union = ++ctx->cur_tag;

    ctx->stamp[u] = tag_union;
    ctx->stamp[v] = tag_union;
    for (int i = 0; i < g->D[u]; i++)
        ctx->stamp[g->V[u][i]] = tag_union;
    for (int i = 0; i < g->D[v]; i++)
        ctx->stamp[g->V[v][i]] = tag_union;

    // 2. Collect C = {r in N(u) U N(v) | N(r) \subseteq {u, v} U N(u) U N(v)}
    int C_size = 0;
    long long total_c_weight = 0;

    // Scan N(u)
    for (int i = 0; i < g->D[u]; i++)
    {
        int r = g->V[u][i];
        if (!g->A[r])
            continue;

        int ok = 1;
        for (int j = 0; j < g->D[r]; j++)
        {
            if (ctx->stamp[g->V[r][j]] != tag_union)
            {
                ok = 0;
                break; // Reject early on 1st outside neighbor!
            }
        }
        if (ok)
        {
            ctx->C[C_size++] = r;
            total_c_weight += g->W[r];
        }
    }

    // Scan N(v)
    for (int i = 0; i < g->D[v]; i++)
    {
        int r = g->V[v][i];
        if (!g->A[r])
            continue;

        if (graph_is_neighbor(g, u, r))
            continue;

        int ok = 1;
        for (int j = 0; j < g->D[r]; j++)
        {
            if (ctx->stamp[g->V[r][j]] != tag_union)
            {
                ok = 0;
                break;
            }
        }
        if (ok)
        {
            ctx->C[C_size++] = r;
            total_c_weight += g->W[r];
        }
    }

    // 3. Early prune on weight
    long long min_uv_weight = g->W[u] < g->W[v] ? g->W[u] : g->W[v];
    if (C_size == 0 || total_c_weight < min_uv_weight)
        return 0;

    // 4. Formulate ILP
    int num_cols = C_size + 2;
    int col_u = C_size;
    int col_v = C_size + 1;

    densify_context_ensure_cols(ctx, num_cols);

    for (int i = 0; i < C_size; i++)
    {
        ctx->col_cost[i] = (double)g->W[ctx->C[i]];
        ctx->col_lower[i] = 0.0;
        ctx->col_upper[i] = 1.0;
        ctx->col_integrality[i] = kHighsVarTypeInteger;
    }
    ctx->col_cost[col_u] = -(double)g->W[u];
    ctx->col_lower[col_u] = 0.0;
    ctx->col_upper[col_u] = 1.0;
    ctx->col_integrality[col_u] = kHighsVarTypeInteger;

    ctx->col_cost[col_v] = -(double)g->W[v];
    ctx->col_lower[col_v] = 0.0;
    ctx->col_upper[col_v] = 1.0;
    ctx->col_integrality[col_v] = kHighsVarTypeInteger;

    // Fast column index mapping for C vertices
    int tag_col = ++ctx->cur_tag;
    for (int i = 0; i < C_size; i++)
    {
        int w = ctx->C[i];
        ctx->col_tag[w] = tag_col;
        ctx->col_map[w] = i;
    }

    // Upper bound on rows: 1 + 2*C_size + sum(D[w] for w in C)
    int max_est_rows = 1 + 2 * C_size;
    for (int i = 0; i < C_size; i++)
        max_est_rows += g->D[ctx->C[i]];
    densify_context_ensure_rows(ctx, max_est_rows, max_est_rows * 2);

    int num_rows = 0;
    int num_nz = 0;

    // Row 0: x_u + x_v >= 1
    ctx->row_lower[num_rows] = 1.0;
    ctx->row_upper[num_rows] = 2.0;
    ctx->row_starts[num_rows] = num_nz;
    ctx->row_indices[num_nz] = col_u;
    ctx->row_values[num_nz++] = 1.0;
    ctx->row_indices[num_nz] = col_v;
    ctx->row_values[num_nz++] = 1.0;
    num_rows++;

    // Row: x_w - x_u <= 0 for w in N(u) \cap C
    for (int i = 0; i < C_size; i++)
    {
        int w = ctx->C[i];
        if (graph_is_neighbor(g, u, w))
        {
            ctx->row_lower[num_rows] = -ctx->inf;
            ctx->row_upper[num_rows] = 0.0;
            ctx->row_starts[num_rows] = num_nz;
            ctx->row_indices[num_nz] = i;
            ctx->row_values[num_nz++] = 1.0;
            ctx->row_indices[num_nz] = col_u;
            ctx->row_values[num_nz++] = -1.0;
            num_rows++;
        }
    }

    // Row: x_w - x_v <= 0 for w in N(v) \cap C
    for (int i = 0; i < C_size; i++)
    {
        int w = ctx->C[i];
        if (graph_is_neighbor(g, v, w))
        {
            ctx->row_lower[num_rows] = -ctx->inf;
            ctx->row_upper[num_rows] = 0.0;
            ctx->row_starts[num_rows] = num_nz;
            ctx->row_indices[num_nz] = i;
            ctx->row_values[num_nz++] = 1.0;
            ctx->row_indices[num_nz] = col_v;
            ctx->row_values[num_nz++] = -1.0;
            num_rows++;
        }
    }

    // Conflict rows: x_w1 + x_w2 <= 1 for all edges in G[C]
    for (int i = 0; i < C_size; i++)
    {
        int w1 = ctx->C[i];
        for (int k = 0; k < g->D[w1]; k++)
        {
            int w2 = g->V[w1][k];
            if (w2 > w1 && ctx->col_tag[w2] == tag_col)
            {
                int j = ctx->col_map[w2];
                ctx->row_lower[num_rows] = -ctx->inf;
                ctx->row_upper[num_rows] = 1.0;
                ctx->row_starts[num_rows] = num_nz;
                ctx->row_indices[num_nz] = i;
                ctx->row_values[num_nz++] = 1.0;
                ctx->row_indices[num_nz] = j;
                ctx->row_values[num_nz++] = 1.0;
                num_rows++;
            }
        }
    }

    double time_limit = (opt && opt->time_limit_sec > 0.0) ? opt->time_limit_sec : 0.1;
    Highs_setDoubleOptionValue(ctx->highs, "time_limit", time_limit);
    Highs_changeObjectiveSense(ctx->highs, kHighsObjSenseMaximize);

    Highs_addCols(ctx->highs, num_cols, ctx->col_cost, ctx->col_lower, ctx->col_upper, 0, NULL, NULL, NULL);
    Highs_changeColsIntegralityByRange(ctx->highs, 0, num_cols - 1, ctx->col_integrality);
    Highs_addRows(ctx->highs, num_rows, ctx->row_lower, ctx->row_upper, num_nz, ctx->row_starts, ctx->row_indices, ctx->row_values);

    Highs_run(ctx->highs);

    int can_densify = 0;
    HighsInt status = Highs_getModelStatus(ctx->highs);
    HighsInt primal_status = kHighsSolutionStatusNone;
    Highs_getIntInfoValue(ctx->highs, "primal_solution_status", &primal_status);

    if (status == kHighsModelStatusOptimal || status == kHighsModelStatusObjectiveTarget || primal_status == kHighsSolutionStatusFeasible)
    {
        double obj = Highs_getObjectiveValue(ctx->highs);
        if (obj >= -1e-5)
        {
            can_densify = 1;
        }
    }

    Highs_clearModel(ctx->highs);

    return can_densify;
}

int densify_try_add_edge_ctx(densify_context *ctx, graph *g, int u, int v, const densify_options *opt)
{
    if (densify_test_pair_ctx(ctx, g, u, v, opt))
    {
        graph_add_edge(g, u, v);
        return 1;
    }
    return 0;
}

int densify_reduce_vertex_ctx(densify_context *ctx, graph *g, int u, const densify_options *opt)
{
    if (!g->A[u])
        return 0;

    // 1. Test domination for the vertex initially
    if (densify_is_dominated(g, u))
    {
        if (opt && opt->verbose)
            printf("Densify: vertex %d dominated initially!\n", u + 1);
        return 1;
    }

    densify_context_ensure_n(ctx, g->n);

    int t = g->t;
    int edges_added_total = 0;
    int max_dist = (opt && opt->max_distance > 0) ? opt->max_distance : 3;

    int max_passes = 8;
    for (int pass = 0; pass < max_passes && g->A[u] && !is_tle(); pass++)
    {
        // 2. BFS candidate collection from u up to max_dist
        if (ctx->cur_tag > 1000000000)
        {
            memset(ctx->stamp, 0, sizeof(int) * ctx->capacity_n);
            memset(ctx->col_tag, 0, sizeof(int) * ctx->capacity_n);
            ctx->cur_tag = 0;
        }
        int tag = ++ctx->cur_tag;

        ctx->stamp[u] = tag;
        ctx->dist[u] = 0;
        int head = 0, tail = 0;
        ctx->queue[tail++] = u;

        // Depth 1: direct neighbors (forbidden)
        for (int i = 0; i < g->D[u]; i++)
        {
            int c = g->V[u][i];
            if (g->A[c] && ctx->stamp[c] != tag)
            {
                ctx->stamp[c] = tag;
                ctx->dist[c] = 1;
                ctx->queue[tail++] = c;
            }
        }

        int num_candidates = 0;
        while (head < tail)
        {
            int curr = ctx->queue[head++];
            int d = ctx->dist[curr];
            if (d >= max_dist)
                continue;

            for (int i = 0; i < g->D[curr]; i++)
            {
                int v = g->V[curr][i];
                if (g->A[v] && ctx->stamp[v] != tag)
                {
                    ctx->stamp[v] = tag;
                    ctx->dist[v] = d + 1;
                    ctx->queue[tail++] = v;

                    // d + 1 is 2 or 3
                    ctx->candidates[num_candidates++] = v;
                }
            }
        }

        if (num_candidates == 0)
            break;

        int edges_added_pass = 0;
        for (int i = 0; i < num_candidates && g->A[u] && !is_tle(); i++)
        {
            int v = ctx->candidates[i];
            if (!g->A[u] || !g->A[v] || graph_is_neighbor(g, u, v))
                continue;

            if (opt && opt->max_degree > 0 && (g->D[u] > opt->max_degree || g->D[v] > opt->max_degree))
                continue;

            if (densify_try_add_edge_ctx(ctx, g, u, v, opt))
            {
                edges_added_pass++;
                edges_added_total++;
                if (opt && opt->verbose)
                {
                    printf("  [u=%d] Added edge (%d, %d), total edges added to u: %d\n",
                           u + 1, u + 1, v + 1, edges_added_total);
                }

                // 3. Eagerly test for domination as we gain more neighbors
                if (densify_is_dominated(g, u))
                {
                    if (opt && opt->verbose)
                    {
                        printf(">>> SUCCESS: Vertex %d is DOMINATED and DELETED (added %d edges)!\n",
                               u + 1, edges_added_total);
                    }
                    return 1;
                }

                if (opt && opt->max_edges_to_add > 0 && edges_added_total >= opt->max_edges_to_add)
                    break;
            }
        }

        if (edges_added_pass == 0)
            break;

        if (opt && opt->max_edges_to_add > 0 && edges_added_total >= opt->max_edges_to_add)
            break;
    }

    if (!g->A[u])
        return 1;

    // If we are not able to delete it, undo the edges we added unless keep_edges is requested
    if (!opt || !opt->keep_edges)
    {
        if (edges_added_total > 0)
        {
            if (opt && opt->verbose)
            {
                printf("  [u=%d] Failed to dominate, undoing %d edges.\n", u + 1, edges_added_total);
            }
            graph_undo_changes(g, t);
        }
    }

    return 0;
}

int densify_reduce_graph(graph *g, const densify_options *opt)
{
    if (!g || g->nr <= 1)
        return 0;

    densify_options default_opt = densify_default_options();
    const densify_options *actual_opt = opt ? opt : &default_opt;

    densify_context *ctx = densify_context_create(g->n);
    if (!ctx)
        return 0;

    int total_deleted = 0;
    int imp = 1;
    int pass = 0;

    while (imp && g->nr > 0 && !is_tle())
    {
        imp = 0;
        pass++;
        int round_deleted = 0;

        for (int u = 0; u < g->n && g->nr > 0 && !is_tle(); u++)
        {
            if (!g->A[u])
                continue;

            if (actual_opt->max_degree > 0 && g->D[u] > actual_opt->max_degree)
                continue;

            if (densify_reduce_vertex_ctx(ctx, g, u, actual_opt))
            {
                round_deleted++;
                imp = 1;
            }
        }

        total_deleted += round_deleted;
        if (actual_opt->verbose)
        {
            printf("Densify-Reduce pass %d finished: deleted %d vertices (remaining: nr=%d, m=%d)\n",
                   pass, round_deleted, g->nr, g->m);
        }

        if (actual_opt->max_passes > 0 && pass >= actual_opt->max_passes)
            break;
    }

    densify_context_destroy(ctx);
    return total_deleted;
}

// Convenience wrapper functions
int densify_test_pair(void *highs, graph *g, int u, int v, const densify_options *opt)
{
    densify_context *ctx = densify_context_create(g->n);
    if (!ctx)
        return 0;
    int res = densify_test_pair_ctx(ctx, g, u, v, opt);
    densify_context_destroy(ctx);
    return res;
}

int densify_try_add_edge(void *highs, graph *g, int u, int v, const densify_options *opt)
{
    densify_context *ctx = densify_context_create(g->n);
    if (!ctx)
        return 0;
    int res = densify_try_add_edge_ctx(ctx, g, u, v, opt);
    densify_context_destroy(ctx);
    return res;
}

int densify_reduce_vertex(void *highs, graph *g, int u, const densify_options *opt)
{
    densify_context *ctx = densify_context_create(g->n);
    if (!ctx)
        return 0;
    int res = densify_reduce_vertex_ctx(ctx, g, u, opt);
    densify_context_destroy(ctx);
    return res;
}

int densify_graph(graph *g, const densify_options *opt)
{
    if (!g || g->nr <= 1)
        return 0;

    densify_options default_opt = densify_default_options();
    const densify_options *actual_opt = opt ? opt : &default_opt;

    densify_context *ctx = densify_context_create(g->n);
    if (!ctx)
        return 0;

    int edges_added = 0;
    int max_dist = actual_opt->max_distance > 0 ? actual_opt->max_distance : 3;

    for (int u = 0; u < g->n && !is_tle(); u++)
    {
        if (!g->A[u])
            continue;

        if (actual_opt->max_degree > 0 && g->D[u] > actual_opt->max_degree)
            continue;

        densify_context_ensure_n(ctx, g->n);

        if (ctx->cur_tag > 1000000000)
        {
            memset(ctx->stamp, 0, sizeof(int) * ctx->capacity_n);
            memset(ctx->col_tag, 0, sizeof(int) * ctx->capacity_n);
            ctx->cur_tag = 0;
        }
        int tag = ++ctx->cur_tag;

        ctx->stamp[u] = tag;
        ctx->dist[u] = 0;
        int head = 0, tail = 0;
        ctx->queue[tail++] = u;

        for (int i = 0; i < g->D[u]; i++)
        {
            int c = g->V[u][i];
            if (g->A[c] && ctx->stamp[c] != tag)
            {
                ctx->stamp[c] = tag;
                ctx->dist[c] = 1;
                ctx->queue[tail++] = c;
            }
        }

        int num_candidates = 0;
        while (head < tail)
        {
            int curr = ctx->queue[head++];
            int d = ctx->dist[curr];
            if (d >= max_dist)
                continue;

            for (int i = 0; i < g->D[curr]; i++)
            {
                int v = g->V[curr][i];
                if (g->A[v] && ctx->stamp[v] != tag)
                {
                    ctx->stamp[v] = tag;
                    ctx->dist[v] = d + 1;
                    ctx->queue[tail++] = v;
                    if (v > u)
                        ctx->candidates[num_candidates++] = v;
                }
            }
        }

        for (int i = 0; i < num_candidates && !is_tle(); i++)
        {
            int v = ctx->candidates[i];
            if (!g->A[u] || !g->A[v] || graph_is_neighbor(g, u, v))
                continue;

            if (actual_opt->max_degree > 0 && (g->D[u] > actual_opt->max_degree || g->D[v] > actual_opt->max_degree))
                continue;

            if (densify_try_add_edge_ctx(ctx, g, u, v, actual_opt))
            {
                edges_added++;
                if (actual_opt->verbose)
                    printf("Densify: added edge (%d, %d), total added: %d\n", u + 1, v + 1, edges_added);

                if (actual_opt->max_edges_to_add > 0 && edges_added >= actual_opt->max_edges_to_add)
                    break;
            }
        }

        if (actual_opt->max_edges_to_add > 0 && edges_added >= actual_opt->max_edges_to_add)
            break;
    }

    densify_context_destroy(ctx);
    return edges_added;
}

int densify_and_reduce_loop(densify_context *ctx, graph *g, int *B, int folding, const densify_options *opt)
{
    if (!g || g->nr <= 1)
        return 0;

    densify_options default_opt = densify_default_options();
    const densify_options *actual_opt = opt ? opt : &default_opt;

    densify_context_ensure_n(ctx, g->n);

    int total_edges_added = 0;
    int total_reduced = 0;
    int imp = 1;
    int round = 0;
    int max_rounds = (actual_opt->max_passes > 0) ? actual_opt->max_passes : 2;

    while (imp && round < max_rounds && g->nr > 0 && !is_tle())
    {
        imp = 0;
        round++;

        // 1. Collect all currently active vertices into B
        int m = 0;
        for (int u = 0; u < g->n; u++)
        {
            if (g->A[u])
                B[m++] = u;
        }

        if (m <= 1)
            break;

        // 2. Randomize order of vertices
        shuffle_list(B, m);

        int round_edges = 0;
        int round_reduced = 0;

        // 3. Densification pass over B (keeping all edges)
        for (int i = 0; i < m && g->nr > 0 && !is_tle(); i++)
        {
            int u = B[i];
            if (!g->A[u])
                continue;

            if (actual_opt->max_degree > 0 && g->D[u] > actual_opt->max_degree)
                continue;

            densify_context_ensure_n(ctx, g->n);

            if (ctx->cur_tag > 1000000000)
            {
                memset(ctx->stamp, 0, sizeof(int) * ctx->capacity_n);
                memset(ctx->col_tag, 0, sizeof(int) * ctx->capacity_n);
                ctx->cur_tag = 0;
            }
            int tag = ++ctx->cur_tag;

            ctx->stamp[u] = tag;
            ctx->dist[u] = 0;
            int head = 0, tail = 0;
            ctx->queue[tail++] = u;

            // Direct neighbors (forbidden)
            for (int j = 0; j < g->D[u]; j++)
            {
                int c = g->V[u][j];
                if (g->A[c] && ctx->stamp[c] != tag)
                {
                    ctx->stamp[c] = tag;
                    ctx->dist[c] = 1;
                    ctx->queue[tail++] = c;
                }
            }

            int max_dist = actual_opt->max_distance > 0 ? actual_opt->max_distance : 3;
            int num_candidates = 0;
            while (head < tail)
            {
                int curr = ctx->queue[head++];
                int d = ctx->dist[curr];
                if (d >= max_dist)
                    continue;

                for (int j = 0; j < g->D[curr]; j++)
                {
                    int v = g->V[curr][j];
                    if (g->A[v] && ctx->stamp[v] != tag)
                    {
                        ctx->stamp[v] = tag;
                        ctx->dist[v] = d + 1;
                        ctx->queue[tail++] = v;
                        ctx->candidates[num_candidates++] = v;
                    }
                }
            }

            if (num_candidates == 0)
                continue;

            int edges_added_u = 0;
            for (int j = 0; j < num_candidates && g->A[u] && !is_tle(); j++)
            {
                int v = ctx->candidates[j];
                if (!g->A[u] || !g->A[v] || graph_is_neighbor(g, u, v))
                    continue;

                if (actual_opt->max_degree > 0 && (g->D[u] > actual_opt->max_degree || g->D[v] > actual_opt->max_degree))
                    continue;

                if (densify_try_add_edge_ctx(ctx, g, u, v, actual_opt))
                {
                    edges_added_u++;
                    round_edges++;
                    imp = 1;

                    // Eager domination check on u and v
                    if (graph_reduce(g, u, folding))
                    {
                        round_reduced++;
                        break;
                    }

                    if (graph_reduce(g, v, folding))
                    {
                        round_reduced++;
                    }

                    if (actual_opt->max_edges_to_add > 0 && edges_added_u >= actual_opt->max_edges_to_add)
                        break;
                }
            }
        }

        // 4. Reduction pass until fixed point
        int red_changed = 1;
        while (red_changed && g->nr > 0 && !is_tle())
        {
            red_changed = 0;
            for (int u = 0; u < g->n && g->nr > 0 && !is_tle(); u++)
            {
                if (!g->A[u])
                    continue;

                if (graph_reduce(g, u, folding))
                {
                    red_changed++;
                    round_reduced++;
                    imp = 1;
                }
            }
        }

        total_edges_added += round_edges;
        total_reduced += round_reduced;

        if (actual_opt->verbose)
        {
            printf("  [Densify-Reduce Round %d] Added %d edges, reduced %d vertices (remaining: nr=%d, m=%d)\n",
                   round, round_edges, round_reduced, g->nr, g->m);
        }

        if (round_edges == 0 && round_reduced == 0)
            break;
    }

    return total_reduced;
}

int densify_alternating_phase(densify_context *ctx, graph *g, int folding, const densify_options *opt)
{
    if (!g || g->nr <= 1)
        return 0;

    densify_options default_opt = densify_default_options();
    const densify_options *actual_opt = opt ? opt : &default_opt;

    densify_context_ensure_n(ctx, g->n);

    int *B = malloc(sizeof(int) * g->n);
    int max_edges_per_vertex = (actual_opt->max_edges_to_add > 0) ? actual_opt->max_edges_to_add : 8;
    int max_dist = (actual_opt->max_distance > 0) ? actual_opt->max_distance : 3;

    printf("============================================================\n");
    printf("Starting Alternating Densify-Expand / Reduction Phase\n");
    printf("Initial graph: vertices=%d, edges=%d, offset=%lld\n", g->nr, g->m, g->off);
    printf("Max edges per vertex per pass: %d (max distance: %d)\n", max_edges_per_vertex, max_dist);
    printf("============================================================\n");
    fflush(stdout);

    int pass = 0;
    int total_edges_added = 0;
    int total_vertices_reduced = 0;
    double t_phase_start = get_wall_time();

    while (g->nr > 0 && !is_tle())
    {
        pass++;
        double t_pass_start = get_wall_time();

        // -------------------------------------------------------------
        // Step 1: Expansion (Densification) pass with random vertex order
        // -------------------------------------------------------------
        int m = 0;
        for (int u = 0; u < g->n; u++)
        {
            if (g->A[u])
                B[m++] = u;
        }

        if (m <= 1)
            break;

        shuffle_list(B, m);

        int edges_added_pass = 0;
        for (int i = 0; i < m && g->nr > 0 && !is_tle(); i++)
        {
            int u = B[i];
            if (!g->A[u])
                continue;

            densify_context_ensure_n(ctx, g->n);

            if (ctx->cur_tag > 1000000000)
            {
                memset(ctx->stamp, 0, sizeof(int) * ctx->capacity_n);
                memset(ctx->col_tag, 0, sizeof(int) * ctx->capacity_n);
                ctx->cur_tag = 0;
            }
            int tag = ++ctx->cur_tag;

            ctx->stamp[u] = tag;
            ctx->dist[u] = 0;
            int head = 0, tail = 0;
            ctx->queue[tail++] = u;

            // Direct neighbors (depth 1)
            for (int j = 0; j < g->D[u]; j++)
            {
                int c = g->V[u][j];
                if (g->A[c] && ctx->stamp[c] != tag)
                {
                    ctx->stamp[c] = tag;
                    ctx->dist[c] = 1;
                    ctx->queue[tail++] = c;
                }
            }

            int num_candidates = 0;
            while (head < tail)
            {
                int curr = ctx->queue[head++];
                int d = ctx->dist[curr];
                if (d >= max_dist)
                    continue;

                for (int j = 0; j < g->D[curr]; j++)
                {
                    int v = g->V[curr][j];
                    if (g->A[v] && ctx->stamp[v] != tag)
                    {
                        ctx->stamp[v] = tag;
                        ctx->dist[v] = d + 1;
                        ctx->queue[tail++] = v;
                        ctx->candidates[num_candidates++] = v;
                    }
                }
            }

            if (num_candidates == 0)
                continue;

            int edges_for_u = 0;
            for (int j = 0; j < num_candidates && g->A[u] && !is_tle(); j++)
            {
                int v = ctx->candidates[j];
                if (!g->A[u] || !g->A[v] || graph_is_neighbor(g, u, v))
                    continue;

                // Maximize speed: no eager domination here, just test pair and add edge!
                if (densify_try_add_edge_ctx(ctx, g, u, v, actual_opt))
                {
                    edges_added_pass++;
                    edges_for_u++;

                    if (edges_for_u >= max_edges_per_vertex)
                        break;
                }
            }
        }

        total_edges_added += edges_added_pass;
        double t_expand = get_wall_time();

        printf("[Pass %2d - Expand] Added %6d edges in %6.2fs (vertices: %d, edges: %d)\n",
               pass, edges_added_pass, t_expand - t_pass_start, g->nr, g->m);
        fflush(stdout);

        // -------------------------------------------------------------
        // Step 2: Reduction loop (by vertex ID, until fixed point)
        // -------------------------------------------------------------
        int reduced_pass = 0;
        int red_round = 0;
        int red_changed = 1;

        while (red_changed && g->nr > 0 && !is_tle())
        {
            red_changed = 0;
            red_round++;
            for (int u = 0; u < g->n && g->nr > 0 && !is_tle(); u++)
            {
                if (!g->A[u])
                    continue;

                if (graph_reduce(g, u, folding))
                {
                    red_changed++;
                    reduced_pass++;
                }
            }
        }

        total_vertices_reduced += reduced_pass;
        double t_reduce = get_wall_time();

        printf("[Pass %2d - Reduce] Reduced %6d vertices in %d round(s) (%6.2fs) (remaining: vertices: %d, edges: %d, offset: %lld)\n",
               pass, reduced_pass, red_round, t_reduce - t_expand, g->nr, g->m, g->off);
        fflush(stdout);

        // -------------------------------------------------------------
        // Step 3: Check convergence (one pass where nothing changed)
        // -------------------------------------------------------------
        if (edges_added_pass == 0 && reduced_pass == 0)
        {
            printf("============================================================\n");
            printf("Alternating Phase Converged at Pass %d!\n", pass);
            printf("Total edges added: %d, Total vertices reduced: %d\n",
                   total_edges_added, total_vertices_reduced);
            printf("Final graph: vertices=%d, edges=%d, offset=%lld (total time: %6.2fs)\n",
                   g->nr, g->m, g->off, t_reduce - t_phase_start);
            printf("============================================================\n");
            fflush(stdout);
            break;
        }
    }

    free(B);
    return total_vertices_reduced;
}
