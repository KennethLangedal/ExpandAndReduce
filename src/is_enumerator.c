#include "is_enumerator.h"

#include <assert.h>

is_enumerator *is_enumerator_init(int *L_list, int ln, is_enumerator *L_req, int *R_list, int rn, is_enumerator *R_req)
{
    assert((L_list == NULL || L_req == NULL) && (L_list != NULL || L_req != NULL));
    assert((R_list == NULL || R_req == NULL) && (R_list != NULL || R_req != NULL));

    is_enumerator *ie = malloc(sizeof(is_enumerator));

    ie->n = 0;
    ie->_a = (1 << 10);

    ie->E = malloc(sizeof(element) * ie->_a);

    ie->hs = hashset64_init((1 << 10));
    ie->h = heap_init((1 << 10));

    ie->L_list = L_list;
    ie->ln = ln;
    ie->L_req = L_req;

    ie->R_list = R_list;
    ie->rn = rn;
    ie->R_req = R_req;

    return ie;
}

void is_enumerator_free(is_enumerator *ie)
{
    free(ie->E);

    hashset64_free(ie->hs);
    heap_free(ie->h);

    free(ie);
}

int is_enumerator_mark(int *List, int n, is_enumerator *req, int p, graph *g, uint8_t *V)
{
    if (List == NULL)
    {
        return is_enumerator_mark(req->L_list, req->ln, req->L_req, req->E[p].a, g, V) +
               is_enumerator_mark(req->R_list, req->rn, req->R_req, req->E[p].b, g, V);
    }
    else if (p < n)
    {
        int u = List[p];
        int res = 0;
        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];
            res += V[v];
            V[v] = 1;
        }
        return res;
    }
    return 0;
}

int is_enumerator_check(int *List, int n, is_enumerator *req, int p, graph *g, uint8_t *V)
{
    if (List == NULL)
    {
        if (!is_enumerator_check(req->L_list, req->ln, req->L_req, req->E[p].a, g, V) ||
            !is_enumerator_check(req->R_list, req->rn, req->R_req, req->E[p].b, g, V))
            return 0;
    }
    else if (p < n)
    {
        int u = List[p];
        if (V[u])
            return 0;
    }
    return 1;
}

int is_enumerator_dom_check(int *List, int n, is_enumerator *req, int p, graph *g, uint8_t *V)
{
    if (List == NULL)
    {
        return is_enumerator_dom_check(req->L_list, req->ln, req->L_req, req->E[p].a, g, V) &&
               is_enumerator_dom_check(req->R_list, req->rn, req->R_req, req->E[p].b, g, V);
    }
    else if (p < n)
    {
        int u = List[p];
        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];
            if (!V[v])
                return 0;
        }
    }
    return 1;
}

void is_enumerator_clear(int *List, int n, is_enumerator *req, int p, graph *g, uint8_t *V)
{
    if (List == NULL)
    {
        is_enumerator_clear(req->L_list, req->ln, req->L_req, req->E[p].a, g, V);
        is_enumerator_clear(req->R_list, req->rn, req->R_req, req->E[p].b, g, V);
    }
    else if (p < n)
    {
        int u = List[p];
        for (int j = 0; j < g->D[u]; j++)
        {
            int v = g->V[u][j];
            V[v] = 0;
        }
    }
}

long long is_enumerator_get_weight(int *List, int n, is_enumerator *req, graph *g, uint8_t *V, int p)
{
    if (List != NULL)
    {
        if (p >= n)
            return 0;
        return g->W[List[p]];
    }
    else
    {
        if (req->n <= p)
            is_enumerator_get_next(req, g, V, 0);
        return req->E[p].w;
    }
}

void is_enumerator_get_next(is_enumerator *ie, graph *g, uint8_t *V, int verbose)
{
    if (ie->n == ie->_a)
    {
        ie->_a *= 2;
        ie->E = realloc(ie->E, sizeof(element) * ie->_a);
    }

    if (ie->h->n == 0 && !hashset64_contains(ie->hs, 0))
    {
        hashset64_insert(ie->hs, 0);
        element e = {
            .a = 0,
            .b = 0,
            .n = 0,
            .wa = is_enumerator_get_weight(ie->L_list, ie->ln, ie->L_req, g, V, 0),
            .wb = is_enumerator_get_weight(ie->R_list, ie->rn, ie->R_req, g, V, 0),
            .w = 0};
        e.w = e.wa + e.wb;

        e.n = is_enumerator_mark(ie->L_list, ie->ln, ie->L_req, e.a, g, V) +
              is_enumerator_mark(ie->R_list, ie->rn, ie->R_req, e.b, g, V);

        heap_insert(ie->h, e);

        is_enumerator_clear(ie->L_list, ie->ln, ie->L_req, e.a, g, V);
        is_enumerator_clear(ie->R_list, ie->rn, ie->R_req, e.b, g, V);

        // printf("Added 0 0 element\n");
    }

    int found = 0;
    while (!found)
    {
        element e = ie->h->H[0];
        heap_pop(ie->h);

        if (verbose)
        {
            printf("\r%10d %10d", e.a, e.b);
            fflush(stdout);
        }

        element l = {
            .a = e.a + 1,
            .b = e.b,
            .n = 0,
            .wa = is_enumerator_get_weight(ie->L_list, ie->ln, ie->L_req, g, V, e.a + 1),
            .wb = is_enumerator_get_weight(ie->R_list, ie->rn, ie->R_req, g, V, e.b),
            .w = 0};
        l.w = l.wa + l.wb;

        l.n = is_enumerator_mark(ie->L_list, ie->ln, ie->L_req, l.a, g, V) +
              is_enumerator_mark(ie->R_list, ie->rn, ie->R_req, l.b, g, V);

        is_enumerator_clear(ie->L_list, ie->ln, ie->L_req, l.a, g, V);
        is_enumerator_clear(ie->R_list, ie->rn, ie->R_req, l.b, g, V);

        uint64_t lk = ((uint64_t)l.a << 32) | l.b;
        if (e.wa != 0 && !hashset64_contains(ie->hs, lk))
        {
            hashset64_insert(ie->hs, lk);
            heap_insert(ie->h, l);
        }

        element r = {
            .a = e.a,
            .b = e.b + 1,
            .n = 0,
            .wa = is_enumerator_get_weight(ie->L_list, ie->ln, ie->L_req, g, V, e.a),
            .wb = is_enumerator_get_weight(ie->R_list, ie->rn, ie->R_req, g, V, e.b + 1),
            .w = 0};
        r.w = r.wa + r.wb;

        r.n = is_enumerator_mark(ie->L_list, ie->ln, ie->L_req, r.a, g, V) +
              is_enumerator_mark(ie->R_list, ie->rn, ie->R_req, r.b, g, V);

        is_enumerator_clear(ie->L_list, ie->ln, ie->L_req, r.a, g, V);
        is_enumerator_clear(ie->R_list, ie->rn, ie->R_req, r.b, g, V);

        uint64_t rk = ((uint64_t)r.a << 32) | r.b;
        if (e.wb != 0 && !hashset64_contains(ie->hs, rk))
        {
            hashset64_insert(ie->hs, rk);
            heap_insert(ie->h, r);
        }

        is_enumerator_mark(ie->L_list, ie->ln, ie->L_req, e.a, g, V);

        if (is_enumerator_check(ie->R_list, ie->rn, ie->R_req, e.b, g, V))
        {
            is_enumerator_mark(ie->R_list, ie->rn, ie->R_req, e.b, g, V);

            int valid = 1;
            for (int i = 0; i < ie->n; i++)
            {
                if (is_enumerator_dom_check(ie->L_list, ie->ln, ie->L_req, ie->E[i].a, g, V) &&
                    is_enumerator_dom_check(ie->R_list, ie->rn, ie->R_req, ie->E[i].b, g, V))
                {
                    valid = 0;
                    break;
                }
            }

            if (valid)
            {
                // Valid combination
                ie->E[ie->n] = e;
                ie->n++;
                found = 1;
            }

            is_enumerator_clear(ie->R_list, ie->rn, ie->R_req, e.b, g, V);
        }

        is_enumerator_clear(ie->L_list, ie->ln, ie->L_req, e.a, g, V);
    }
}

void is_enumerator_print_solution(is_enumerator *ie, graph *g, int p)
{
    element e = ie->E[p];

    printf("|%lld|(", e.w);

    if (ie->L_list != NULL)
    {
        if (e.a < ie->ln)
            printf("%d (%lld), ", ie->L_list[e.a], g->W[ie->L_list[e.a]]);
        else
            printf("--, ");
    }
    else
    {
        is_enumerator_print_solution(ie->L_req, g, e.a);
    }

    if (ie->R_list != NULL)
    {
        if (e.b < ie->rn)
            printf("%d (%lld), ", ie->R_list[e.b], g->W[ie->R_list[e.b]]);
        else
            printf("--, ");
    }
    else
    {
        is_enumerator_print_solution(ie->R_req, g, e.b);
    }
    printf(")");
}