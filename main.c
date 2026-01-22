#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { int step, sa, na; } Potez;

static int n;
static int *tabla;
static int *pos_of;

static Potez *istorija;
static int hist_cap;

static int *targets;
static int targets_len;

static inline int m(int p) { return p + n + 1; }

static inline int valid_pos(int p) {
    if (p == 0) return 0;
    if (p >= -n && p <= n && p != 0) return 1;
    if (p == -(n + 1) || p == (n + 1)) return 1;
    return 0;
}

static void prikazi_stanje(void) {
    printf("L: ");
    for (int i = -1; i >= -n; i--) printf("[%d]", tabla[m(i)]);
    printf(" | G:%d D:%d | R: ", tabla[m(-(n+1))], tabla[m(n+1)]);
    for (int i = n; i >= 1; i--) printf("[%d]", tabla[m(i)]);
    printf("\n");
}

static inline int cilj_postignut(void) {
    for (int i = 1; i <= n; i++) {
        int p = pos_of[i];
        if (!(p >= 1 && p <= n)) return 0;
    }
    return 1;
}

static inline int moze_stati(int s, int p) {
    if (p > n || p < -n) return 1;
    int a = (p < 0) ? -p : p;
    return a >= s;
}

static uint64_t hash_stanja(void) {
    uint64_t h = 1469598103934665603ULL;
    int len = 2 * n + 3;
    for (int i = 0; i < len; i++) {
        uint64_t x = (uint64_t)tabla[i];
        h ^= x + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
    }
    return h;
}

static int put_prohodan(int sa, int na) {
    if (!valid_pos(sa) || !valid_pos(na)) return 0;
    if (sa == na) return 0;
    if (sa == 0 || na == 0) return 0;

    int saL = (sa >= -n && sa <= -1);
    int naL = (na >= -n && na <= -1);
    int saR = (sa >= 1 && sa <= n);
    int naR = (na >= 1 && na <= n);

    if ((saL && naL) || (saR && naR)) {
        int a = sa < na ? sa : na;
        int b = sa < na ? na : sa;
        for (int k = a + 1; k <= b - 1; k++) {
            if (k == 0) continue;
            if (tabla[m(k)] != 0) return 0;
        }
        return 1;
    }

    if (saL) {
        for (int i = sa - 1; i >= -n; i--) if (tabla[m(i)] != 0) return 0;
    } else if (saR) {
        for (int i = sa + 1; i <= n; i++) if (tabla[m(i)] != 0) return 0;
    }

    if (naL) {
        for (int i = -n; i < na; i++) if (tabla[m(i)] != 0) return 0;
    } else if (naR) {
        for (int i = n; i > na; i--) if (tabla[m(i)] != 0) return 0;
    }

    return 1;
}

typedef struct {
    uint64_t *keys;
    unsigned char *used;
    size_t cap;
    size_t size;
} HashSet;

static void hs_init(HashSet *hs, size_t cap_pow2) {
    hs->cap = cap_pow2;
    hs->size = 0;
    hs->keys = (uint64_t*)calloc(hs->cap, sizeof(uint64_t));
    hs->used = (unsigned char*)calloc(hs->cap, 1);
    if (!hs->keys || !hs->used) { fprintf(stderr, "OOM\n"); exit(1); }
}

static void hs_free(HashSet *hs) {
    free(hs->keys);
    free(hs->used);
    hs->keys = NULL;
    hs->used = NULL;
    hs->cap = hs->size = 0;
}

static void hs_rehash(HashSet *hs, size_t new_cap) {
    uint64_t *old_keys = hs->keys;
    unsigned char *old_used = hs->used;
    size_t old_cap = hs->cap;

    hs->cap = new_cap;
    hs->size = 0;
    hs->keys = (uint64_t*)calloc(hs->cap, sizeof(uint64_t));
    hs->used = (unsigned char*)calloc(hs->cap, 1);
    if (!hs->keys || !hs->used) { fprintf(stderr, "OOM\n"); exit(1); }

    for (size_t i = 0; i < old_cap; i++) {
        if (!old_used[i]) continue;
        uint64_t h = old_keys[i];
        size_t idx = (size_t)(h & (hs->cap - 1));
        for (;;) {
            if (!hs->used[idx]) {
                hs->used[idx] = 1;
                hs->keys[idx] = h;
                hs->size++;
                break;
            }
            idx = (idx + 1) & (hs->cap - 1);
        }
    }

    free(old_keys);
    free(old_used);
}

static int hs_has_or_add(HashSet *hs, uint64_t h) {
    if ((hs->size + 1) * 10 >= hs->cap * 7) {
        hs_rehash(hs, hs->cap * 2);
    }

    size_t idx = (size_t)(h & (hs->cap - 1));
    for (;;) {
        if (!hs->used[idx]) {
            hs->used[idx] = 1;
            hs->keys[idx] = h;
            hs->size++;
            return 0;
        }
        if (hs->keys[idx] == h) return 1;
        idx = (idx + 1) & (hs->cap - 1);
    }
}

static HashSet visited;

static void build_targets(void) {
    targets_len = 2 * n + 2;
    targets = (int*)malloc(sizeof(int) * (size_t)targets_len);
    if (!targets) { fprintf(stderr, "OOM\n"); exit(1); }

    int t = 0;
    targets[t++] = (n + 1);
    for (int x = n; x >= 1; x--) targets[t++] = x;
    targets[t++] = -(n + 1);
    for (int x = -1; x >= -n; x--) targets[t++] = x;
    targets_len = t;
}

static void apply_move(int s, int sa, int na) {
    tabla[m(sa)] = 0;
    tabla[m(na)] = s;
    pos_of[s] = na;
}

static void undo_move(int s, int sa, int na) {
    tabla[m(na)] = 0;
    tabla[m(sa)] = s;
    pos_of[s] = sa;
}

typedef struct {
    int s;
    int ti;
    int entered;
} Frame;

static int solve_fast_any(void) {
    Frame *stack = (Frame*)calloc((size_t)hist_cap, sizeof(Frame));
    if (!stack) { fprintf(stderr, "OOM\n"); exit(1); }

    memset(istorija, 0, sizeof(Potez) * (size_t)hist_cap);

    int d = 0;
    stack[0] = (Frame){ .s = n, .ti = 0, .entered = 0 };

    for (;;) {
        if (cilj_postignut()) { free(stack); return 1; }
        if (d >= hist_cap - 1) { free(stack); return 0; }

        Frame *fr = &stack[d];

        if (!fr->entered) {
            uint64_t h = hash_stanja();
            if (hs_has_or_add(&visited, h)) {
                if (d == 0) { free(stack); return 0; }
                d--;
                Potez p = istorija[d];
                undo_move(p.step, p.sa, p.na);
                istorija[d] = (Potez){0,0,0};
                continue;
            }
            fr->entered = 1;
        }

        int moved = 0;

        while (fr->s >= 1 && !moved) {
            int s = fr->s;
            int sa = pos_of[s];
            if (!sa) { fr->s--; fr->ti = 0; continue; }

            while (fr->ti < targets_len) {
                int na = targets[fr->ti++];

                if (na == 0 || na == sa) continue;
                if (tabla[m(na)] != 0) continue;
                if (!moze_stati(s, na)) continue;
                if (!put_prohodan(sa, na)) continue;

                if (d > 0) {
                    Potez prev = istorija[d - 1];
                    if (prev.step == s && prev.sa == na && prev.na == sa) continue;
                }

                apply_move(s, sa, na);

                istorija[d] = (Potez){s, sa, na};
                istorija[d + 1] = (Potez){0,0,0};

                d++;
                stack[d] = (Frame){ .s = n, .ti = 0, .entered = 0 };

                moved = 1;
                break;
            }

            if (!moved) {
                fr->s--;
                fr->ti = 0;
            }
        }

        if (moved) continue;

        if (d == 0) { free(stack); return 0; }
        d--;
        Potez p = istorija[d];
        undo_move(p.step, p.sa, p.na);
        istorija[d] = (Potez){0,0,0};
    }
}

int main(void) {
    if (scanf("%d", &n) != 1) return 0;
    if (n < 1) return 0;

    tabla = (int*)calloc((size_t)(2 * n + 5), sizeof(int));
    pos_of = (int*)calloc((size_t)(n + 1), sizeof(int));
    if (!tabla || !pos_of) { fprintf(stderr, "OOM\n"); return 0; }

    hist_cap = (n <= 10) ? 600000 : 1200000;
    istorija = (Potez*)calloc((size_t)hist_cap, sizeof(Potez));
    if (!istorija) { fprintf(stderr, "OOM\n"); return 0; }

    for (int i = 1; i <= n; i++) {
        int p = -i;
        tabla[m(p)] = i;
        pos_of[i] = p;
    }

    build_targets();

    hs_init(&visited, (n <= 10) ? (1u << 24) : (1u << 25));

    int ok = solve_fast_any();

    if (!ok) {
        printf("Nije nadjeno\n");
        hs_free(&visited);
        free(targets);
        free(istorija);
        free(pos_of);
        free(tabla);
        return 0;
    }

    memset(tabla, 0, (size_t)(2 * n + 5) * sizeof(int));
    memset(pos_of, 0, (size_t)(n + 1) * sizeof(int));
    for (int i = 1; i <= n; i++) {
        int p = -i;
        tabla[m(p)] = i;
        pos_of[i] = p;
    }

    printf("START: ");
    prikazi_stanje();
    for (int i = 0; i < hist_cap && istorija[i].step; i++) {
        Potez p = istorija[i];
        apply_move(p.step, p.sa, p.na);
        printf("Pomjeri %d sa %d na %d ", p.step, p.sa, p.na);
        prikazi_stanje();
        if (cilj_postignut()) break;
    }

    hs_free(&visited);
    free(targets);
    free(istorija);
    free(pos_of);
    free(tabla);
    return 0;
}
