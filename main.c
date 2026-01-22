#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { int step, sa, na; } Potez;

static int *tabla = NULL;
static int n = 0;

static Potez *istorija = NULL;
static int max_poteza = 0;

static inline int m(int p) { return p + n + 1; }

static inline int valid_pos(int p) {
    if (p == 0) return 0;
    if (p >= -n && p <= n && p != 0) return 1;
    if (p == -(n + 1) || p == (n + 1)) return 1;
    return 0;
}

static void prikazi_stanje() {
    printf("L: ");
    for (int i = -1; i >= -n; i--) printf("[%d]", tabla[m(i)]);
    printf(" | G:%d D:%d | R: ", tabla[m(-(n+1))], tabla[m(n+1)]);
    for (int i = n; i >= 1; i--) printf("[%d]", tabla[m(i)]);
    printf("\n");
}

static inline int cilj_postignut() {
    for (int i = 1; i <= n; i++) if (tabla[m(i)] == 0) return 0;
    return 1;
}

static inline int moze_stati(int s, int p) {
    if (p > n || p < -n) return 1;
    int a = p < 0 ? -p : p;
    return a >= s;
}

static uint64_t hash_stanja() {
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

static int *targets = NULL;
static int targets_len = 0;

static void build_targets() {
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

static int find_pos_of(int s) {
    for (int k = -(n + 1); k <= (n + 1); k++) {
        if (k != 0 && tabla[m(k)] == s) return k;
    }
    return 0;
}

typedef struct {
    int s;        // trenutna ploca koju probamo na ovom nivou
    int ti;       // index sljedeceg cilja u targets[]
    int entered;  // 0 ako smo tek usli na ovaj nivo (treba visited), 1 ako nastavljamo
} Frame;

static int solve_iterative(void) {
    Frame *stack = (Frame*)calloc((size_t)max_poteza, sizeof(Frame));
    if (!stack) { fprintf(stderr, "OOM\n"); exit(1); }

    memset(istorija, 0, sizeof(Potez) * (size_t)max_poteza);

    int d = 0;
    stack[0] = (Frame){ .s = n, .ti = 0, .entered = 0 };

    for (;;) {
        if (cilj_postignut()) { free(stack); return 1; }
        if (d >= max_poteza - 1) { free(stack); return 0; }

        Frame *fr = &stack[d];

        if (!fr->entered) {
            uint64_t h = hash_stanja();
            if (hs_has_or_add(&visited, h)) {
                if (d == 0) { free(stack); return 0; }
                d--;
                Potez p = istorija[d];
                tabla[m(p.na)] = 0;
                tabla[m(p.sa)] = p.step;
                istorija[d] = (Potez){0,0,0};
                continue;
            }
            fr->entered = 1;
        }

        int moved = 0;

        while (fr->s >= 1 && !moved) {
            int sa = 0;
            for (int k = -(n + 1); k <= (n + 1); k++) {
                if (k != 0 && tabla[m(k)] == fr->s) { sa = k; break; }
            }

            if (!sa) {
                fr->s--;
                fr->ti = 0;
                continue;
            }

            while (fr->ti < targets_len) {
                int na = targets[fr->ti++];
                if (na == 0 || na == sa) continue;
                if (tabla[m(na)] != 0) continue;
                if (!moze_stati(fr->s, na)) continue;
                if (!put_prohodan(sa, na)) continue;

                tabla[m(sa)] = 0;
                tabla[m(na)] = fr->s;

                istorija[d] = (Potez){fr->s, sa, na};
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
        tabla[m(p.na)] = 0;
        tabla[m(p.sa)] = p.step;
        istorija[d] = (Potez){0,0,0};
    }
}

int main() {
    if (scanf("%d", &n) != 1) return 0;
    if (n < 1) return 0;

    tabla = (int*)calloc((size_t)(2 * n + 5), sizeof(int));
    if (!tabla) { fprintf(stderr, "OOM\n"); return 0; }

    max_poteza = 200000;
    istorija = (Potez*)calloc((size_t)max_poteza, sizeof(Potez));
    if (!istorija) { fprintf(stderr, "OOM\n"); return 0; }

    for (int i = 1; i <= n; i++) tabla[m(-i)] = i;

    hs_init(&visited, 1u << 22);
    build_targets();

    int ok = solve_iterative();

    if (!ok) {
        printf("Nije nadjeno\n");
        hs_free(&visited);
        free(targets);
        free(istorija);
        free(tabla);
        return 0;
    }

    memset(tabla, 0, (size_t)(2 * n + 5) * sizeof(int));
    for (int i = 1; i <= n; i++) tabla[m(-i)] = i;

    printf("START: ");
    prikazi_stanje();
    for (int i = 0; i < max_poteza && istorija[i].step; i++) {
        Potez p = istorija[i];
        tabla[m(p.sa)] = 0;
        tabla[m(p.na)] = p.step;
        printf("Pomjeri %d sa %d na %d ", p.step, p.sa, p.na);
        prikazi_stanje();
        if (cilj_postignut()) break;
    }

    hs_free(&visited);
    free(targets);
    free(istorija);
    free(tabla);
    return 0;
}
