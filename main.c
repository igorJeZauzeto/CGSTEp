#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { int step, sa, na; } Move;

static int n;

static inline int valid_pos(int p) {
    if (p == 0) return 0;
    if (p >= -n && p <= n && p != 0) return 1;
    if (p == -(n + 1) || p == (n + 1)) return 1;
    return 0;
}

static inline int m(int p) { return p + n + 1; }
static inline int len_tabla(void) { return 2 * n + 3; }

static int *all_pos;
static int all_pos_len;

static void build_all_pos(void) {
    all_pos_len = 2 * n + 2;
    all_pos = (int*)malloc(sizeof(int) * (size_t)all_pos_len);
    if (!all_pos) { fprintf(stderr, "OOM\n"); exit(1); }
    int t = 0;
    for (int p = -1; p >= -n; p--) all_pos[t++] = p;
    all_pos[t++] = (n + 1);
    all_pos[t++] = -(n + 1);
    for (int p = 1; p <= n; p++) all_pos[t++] = p;
    all_pos_len = t;
}

static inline int can_fit(int step, int p) {
    if (p == (n+1) || p == -(n+1)) return 1;
    int a = (p < 0) ? -p : p;
    return a >= step;
}

static int path_clear(int sa, int na, const int *board) {
    if (!valid_pos(sa) || !valid_pos(na)) return 0;
    if (sa == na) return 0;

    int saL = (sa >= -n && sa <= -1);
    int naL = (na >= -n && na <= -1);
    int saR = (sa >= 1 && sa <= n);
    int naR = (na >= 1 && na <= n);

    if ((saL && naL) || (saR && naR)) {
        int a = sa < na ? sa : na;
        int b = sa < na ? na : sa;
        for (int k = a + 1; k <= b - 1; k++) {
            if (k == 0) continue;
            if (board[m(k)] != 0) return 0;
        }
        return 1;
    }

    if (saL) {
        for (int i = sa - 1; i >= -n; i--) if (board[m(i)] != 0) return 0;
    } else if (saR) {
        for (int i = sa + 1; i <= n; i++) if (board[m(i)] != 0) return 0;
    }

    if (naL) {
        for (int i = -n; i < na; i++) if (board[m(i)] != 0) return 0;
    } else if (naR) {
        for (int i = n; i > na; i--) if (board[m(i)] != 0) return 0;
    }

    return 1;
}

typedef struct { uint64_t lo, hi; } Key;

static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static inline uint64_t key_hash(Key k) {
    return mix64(k.lo ^ mix64(k.hi + 0x9e3779b97f4a7c15ULL));
}

static Key pack_board(const int *board) {
    int len = len_tabla();
    Key k;
    k.lo = 0;
    k.hi = 0;
    int bit = 0;
    for (int i = 0; i < len; i++) {
        uint64_t v = (uint64_t)(board[i] & 0xF);
        if (bit < 64) k.lo |= (v << bit);
        else          k.hi |= (v << (bit - 64));
        bit += 4;
    }
    return k;
}

static void unpack_board(Key k, int *board) {
    int len = len_tabla();
    int bit = 0;
    for (int i = 0; i < len; i++) {
        uint64_t v;
        if (bit < 64) v = (k.lo >> bit) & 0xFULL;
        else          v = (k.hi >> (bit - 64)) & 0xFULL;
        board[i] = (int)v;
        bit += 4;
    }
    board[m(0)] = 0;
}

static int is_goal_state(Key k) {
    int len = len_tabla();
    int *board = (int*)alloca(sizeof(int) * (size_t)len);
    unpack_board(k, board);
    for (int step = 1; step <= n; step++) {
        if (board[m(step)] != step) return 0;
    }
    return 1;
}

typedef struct {
    Key *keys;
    int *vals;
    unsigned char *used;
    size_t cap, size;
} KeyMap;

static void km_init(KeyMap *mp, size_t cap_pow2) {
    mp->cap = cap_pow2;
    mp->size = 0;
    mp->keys = (Key*)calloc(mp->cap, sizeof(Key));
    mp->vals = (int*)calloc(mp->cap, sizeof(int));
    mp->used = (unsigned char*)calloc(mp->cap, 1);
    if (!mp->keys || !mp->vals || !mp->used) { fprintf(stderr, "OOM\n"); exit(1); }
}

static void km_free(KeyMap *mp) {
    free(mp->keys); free(mp->vals); free(mp->used);
    mp->keys = NULL; mp->vals = NULL; mp->used = NULL;
    mp->cap = mp->size = 0;
}

static void km_rehash(KeyMap *mp, size_t new_cap) {
    Key *old_keys = mp->keys;
    int *old_vals = mp->vals;
    unsigned char *old_used = mp->used;
    size_t old_cap = mp->cap;

    mp->cap = new_cap;
    mp->size = 0;
    mp->keys = (Key*)calloc(mp->cap, sizeof(Key));
    mp->vals = (int*)calloc(mp->cap, sizeof(int));
    mp->used = (unsigned char*)calloc(mp->cap, 1);
    if (!mp->keys || !mp->vals || !mp->used) { fprintf(stderr, "OOM\n"); exit(1); }

    for (size_t i = 0; i < old_cap; i++) {
        if (!old_used[i]) continue;
        Key k = old_keys[i];
        int v = old_vals[i];
        size_t idx = (size_t)(key_hash(k) & (mp->cap - 1));
        for (;;) {
            if (!mp->used[idx]) {
                mp->used[idx] = 1;
                mp->keys[idx] = k;
                mp->vals[idx] = v;
                mp->size++;
                break;
            }
            idx = (idx + 1) & (mp->cap - 1);
        }
    }

    free(old_keys); free(old_vals); free(old_used);
}

static int km_get(const KeyMap *mp, Key k, int *out_val) {
    size_t idx = (size_t)(key_hash(k) & (mp->cap - 1));
    for (;;) {
        if (!mp->used[idx]) return 0;
        if (mp->keys[idx].lo == k.lo && mp->keys[idx].hi == k.hi) {
            *out_val = mp->vals[idx];
            return 1;
        }
        idx = (idx + 1) & (mp->cap - 1);
    }
}

static void km_put(KeyMap *mp, Key k, int val) {
    if ((mp->size + 1) * 10 >= mp->cap * 7) km_rehash(mp, mp->cap * 2);
    size_t idx = (size_t)(key_hash(k) & (mp->cap - 1));
    for (;;) {
        if (!mp->used[idx]) {
            mp->used[idx] = 1;
            mp->keys[idx] = k;
            mp->vals[idx] = val;
            mp->size++;
            return;
        }
        if (mp->keys[idx].lo == k.lo && mp->keys[idx].hi == k.hi) return;
        idx = (idx + 1) & (mp->cap - 1);
    }
}

typedef struct {
    Key state;
    int parent;
    Move mv;
} Node;

typedef struct {
    Node *a;
    int cap;
    int size;
} NodeVec;

static void nv_init(NodeVec *v, int cap0) {
    v->cap = cap0;
    v->size = 0;
    v->a = (Node*)malloc(sizeof(Node) * (size_t)v->cap);
    if (!v->a) { fprintf(stderr, "OOM\n"); exit(1); }
}
static int nv_push(NodeVec *v, Node nd) {
    if (v->size >= v->cap) {
        int new_cap = v->cap * 2;
        Node *na = (Node*)realloc(v->a, sizeof(Node) * (size_t)new_cap);
        if (!na) { fprintf(stderr, "OOM\n"); exit(1); }
        v->a = na;
        v->cap = new_cap;
    }
    v->a[v->size] = nd;
    return v->size++;
}
static void nv_free(NodeVec *v) { free(v->a); v->a=NULL; v->cap=v->size=0; }

static int gen_moves(Key st, Move *out, int out_cap) {
    int len = len_tabla();
    int *board = (int*)alloca(sizeof(int) * (size_t)len);
    int *pos_of = (int*)alloca(sizeof(int) * (size_t)(n + 1));
    unpack_board(st, board);

    memset(pos_of, 0, sizeof(int) * (size_t)(n + 1));
    for (int idx = 0; idx < all_pos_len; idx++) {
        int p = all_pos[idx];
        int v = board[m(p)];
        if (v > 0 && v <= n) pos_of[v] = p;
    }

    int empties_len = 0;
    int *empties = (int*)alloca(sizeof(int) * (size_t)all_pos_len);
    for (int i = 0; i < all_pos_len; i++) {
        int p = all_pos[i];
        if (board[m(p)] == 0) empties[empties_len++] = p;
    }

    int cnt = 0;

    for (int ei = 0; ei < empties_len; ei++) {
        int na = empties[ei];

        for (int s = n; s >= 1; s--) {
            int sa = pos_of[s];
            if (!sa) continue;
            if (sa == na) continue;
            if (!can_fit(s, na)) continue;
            if (!path_clear(sa, na, board)) continue;

            if (sa == s && !(na == (n+1) || na == -(n+1))) continue;

            if (sa < 0 && na < 0) {
                if (na <= sa) continue;
            } else if (sa > 0 && na > 0) {
                int dist_before = sa - s; if (dist_before < 0) dist_before = -dist_before;
                int dist_after  = na - s; if (dist_after  < 0) dist_after  = -dist_after;
                if (dist_after >= dist_before) continue;
            }

            if (cnt < out_cap) {
                out[cnt++] = (Move){ .step = s, .sa = sa, .na = na };
            } else {
                return cnt;
            }
        }
    }
    return cnt;
}

static Key apply_move_to_key(Key st, Move mv) {
    int len = len_tabla();
    int *board = (int*)alloca(sizeof(int) * (size_t)len);
    unpack_board(st, board);
    board[m(mv.sa)] = 0;
    board[m(mv.na)] = mv.step;
    return pack_board(board);
}

static inline Move invert_move(Move mv) {
    Move r = mv;
    int tmp = r.sa; r.sa = r.na; r.na = tmp;
    return r;
}

static Move *reconstruct_path(NodeVec *F, int meet_f, NodeVec *B, int meet_b, int *out_len) {
    int f_len = 0, f_cap = 1024;
    Move *f_moves = (Move*)malloc(sizeof(Move) * (size_t)f_cap);
    if (!f_moves) { fprintf(stderr, "OOM\n"); exit(1); }

    for (int v = meet_f; v != -1 && F->a[v].parent != -1; v = F->a[v].parent) {
        if (f_len >= f_cap) {
            f_cap *= 2;
            f_moves = (Move*)realloc(f_moves, sizeof(Move) * (size_t)f_cap);
            if (!f_moves) { fprintf(stderr, "OOM\n"); exit(1); }
        }
        f_moves[f_len++] = F->a[v].mv;
    }
    for (int i = 0; i < f_len / 2; i++) {
        Move tmp = f_moves[i];
        f_moves[i] = f_moves[f_len - 1 - i];
        f_moves[f_len - 1 - i] = tmp;
    }

    int b_len = 0, b_cap = 1024;
    Move *b_moves = (Move*)malloc(sizeof(Move) * (size_t)b_cap);
    if (!b_moves) { fprintf(stderr, "OOM\n"); exit(1); }

    for (int v = meet_b; v != -1 && B->a[v].parent != -1; v = B->a[v].parent) {
        if (b_len >= b_cap) {
            b_cap *= 2;
            b_moves = (Move*)realloc(b_moves, sizeof(Move) * (size_t)b_cap);
            if (!b_moves) { fprintf(stderr, "OOM\n"); exit(1); }
        }
        b_moves[b_len++] = invert_move(B->a[v].mv);
    }

    int total = f_len + b_len;
    Move *path = (Move*)malloc(sizeof(Move) * (size_t)total);
    if (!path) { fprintf(stderr, "OOM\n"); exit(1); }

    memcpy(path, f_moves, sizeof(Move) * (size_t)f_len);
    memcpy(path + f_len, b_moves, sizeof(Move) * (size_t)b_len);

    free(f_moves);
    free(b_moves);

    *out_len = total;
    return path;
}

typedef struct {
    int *q;
    int cap;
    int head, tail;
} Queue;

static void q_init(Queue *q, int cap0) {
    q->cap = cap0;
    q->q = (int*)malloc(sizeof(int) * (size_t)q->cap);
    if (!q->q) { fprintf(stderr, "OOM\n"); exit(1); }
    q->head = q->tail = 0;
}
static void q_free(Queue *q) { free(q->q); q->q=NULL; q->cap=q->head=q->tail=0; }
static int q_size(const Queue *q) { return q->tail - q->head; }
static void q_push(Queue *q, int v) {
    if (q->tail >= q->cap) {
        if (q->head > 0) {
            int sz = q->tail - q->head;
            memmove(q->q, q->q + q->head, sizeof(int) * (size_t)sz);
            q->head = 0;
            q->tail = sz;
        } else {
            int new_cap = q->cap * 2;
            int *nq = (int*)realloc(q->q, sizeof(int) * (size_t)new_cap);
            if (!nq) { fprintf(stderr, "OOM\n"); exit(1); }
            q->q = nq;
            q->cap = new_cap;
        }
    }
    q->q[q->tail++] = v;
}
static int q_pop(Queue *q) { return q->q[q->head++]; }

static int bidir_bfs(Key start, Key goal, Move **out_path, int *out_path_len) {
    NodeVec F, B;
    KeyMap MF, MB;
    Queue QF, QB;

    nv_init(&F, 1<<21);
    nv_init(&B, 1<<21);

    km_init(&MF, 1u<<23);
    km_init(&MB, 1u<<23);

    q_init(&QF, 1<<21);
    q_init(&QB, 1<<21);

    int rootF = nv_push(&F, (Node){ .state=start, .parent=-1, .mv=(Move){0,0,0} });
    int rootB = nv_push(&B, (Node){ .state=goal,  .parent=-1, .mv=(Move){0,0,0} });

    km_put(&MF, start, rootF);
    km_put(&MB, goal,  rootB);

    q_push(&QF, rootF);
    q_push(&QB, rootB);

    Move moves[512];

    for (;;) {
        int szF = q_size(&QF);
        int szB = q_size(&QB);
        if (szF == 0 || szB == 0) break;

        int expand_forward = (szF <= szB);

        if (expand_forward) {
            int vid = q_pop(&QF);
            Key st = F.a[vid].state;

            int mc = gen_moves(st, moves, (int)(sizeof(moves)/sizeof(moves[0])));
            for (int i = 0; i < mc; i++) {
                Move mv = moves[i];
                Key nx = apply_move_to_key(st, mv);

                int dummy;
                if (km_get(&MF, nx, &dummy)) continue;

                int nid = nv_push(&F, (Node){ .state=nx, .parent=vid, .mv=mv });
                km_put(&MF, nx, nid);
                q_push(&QF, nid);

                int meet_b;
                if (km_get(&MB, nx, &meet_b)) {
                    *out_path = reconstruct_path(&F, nid, &B, meet_b, out_path_len);
                    q_free(&QF); q_free(&QB);
                    km_free(&MF); km_free(&MB);
                    nv_free(&F); nv_free(&B);
                    return 1;
                }
            }
        } else {
            int vid = q_pop(&QB);
            Key st = B.a[vid].state;

            int mc = gen_moves(st, moves, (int)(sizeof(moves)/sizeof(moves[0])));
            for (int i = 0; i < mc; i++) {
                Move mv = moves[i];
                Key nx = apply_move_to_key(st, mv);

                int dummy;
                if (km_get(&MB, nx, &dummy)) continue;

                int nid = nv_push(&B, (Node){ .state=nx, .parent=vid, .mv=mv });
                km_put(&MB, nx, nid);
                q_push(&QB, nid);

                int meet_f;
                if (km_get(&MF, nx, &meet_f)) {
                    *out_path = reconstruct_path(&F, meet_f, &B, nid, out_path_len);
                    q_free(&QF); q_free(&QB);
                    km_free(&MF); km_free(&MB);
                    nv_free(&F); nv_free(&B);
                    return 1;
                }
            }
        }
    }

    q_free(&QF); q_free(&QB);
    km_free(&MF); km_free(&MB);
    nv_free(&F); nv_free(&B);
    return 0;
}

static Key make_start(void) {
    int len = len_tabla();
    int *board = (int*)calloc((size_t)len, sizeof(int));
    if (!board) { fprintf(stderr, "OOM\n"); exit(1); }
    for (int k = 1; k <= n; k++) board[m(-k)] = k;
    board[m(0)] = 0;
    Key st = pack_board(board);
    free(board);
    return st;
}

static Key make_goal(void) {
    int len = len_tabla();
    int *board = (int*)calloc((size_t)len, sizeof(int));
    if (!board) { fprintf(stderr, "OOM\n"); exit(1); }
    for (int k = 1; k <= n; k++) board[m(k)] = k;
    board[m(0)] = 0;
    Key st = pack_board(board);
    free(board);
    return st;
}

static void print_cell(int v, int w) {
    if (v == 0) {
        printf("%*s", w, ".");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", v);
        printf("%*s", w, buf);
    }
}

static void print_space(int w) {
    printf("%*s", w, "");
}

static void print_state_from_board(const int *board) {
    int w = (n >= 10) ? 4 : 3;

    for (int i = -1; i >= -n; i--) print_space(w);
    print_cell(board[m(n+1)], w);
    for (int i = n; i >= 1; i--) print_space(w);
    putchar('\n');

    for (int i = -1; i >= -n; i--) print_cell(board[m(i)], w);
    printf("%*s", w, "0");
    for (int i = n; i >= 1; i--) print_cell(board[m(i)], w);
    putchar('\n');

    for (int i = -1; i >= -n; i--) print_space(w);
    print_cell(board[m(-(n+1))], w);
    for (int i = n; i >= 1; i--) print_space(w);
    putchar('\n');
}

int main(void) {
    if (scanf("%d", &n) != 1) return 0;
    if (n < 1 || n > 15) {
        fprintf(stderr, "n mora biti u [1..15]\n");
        return 0;
    }

    build_all_pos();

    Key start = make_start();
    Key goal  = make_goal();

    if (is_goal_state(start)) {
        free(all_pos);
        return 0;
    }

    Move *path = NULL;
    int path_len = 0;

    int ok = bidir_bfs(start, goal, &path, &path_len);
    if (!ok) {
        printf("Nije nadjeno\n");
        free(all_pos);
        return 0;
    }

    int len = len_tabla();
    int *board = (int*)calloc((size_t)len, sizeof(int));
    if (!board) { fprintf(stderr, "OOM\n"); exit(1); }
    for (int k = 1; k <= n; k++) board[m(-k)] = k;
    board[m(0)] = 0;

    print_state_from_board(board);

    for (int i = 0; i < path_len; i++) {
        printf("Pomjeri %d sa %d na %d\n", path[i].step, path[i].sa, path[i].na);
        board[m(path[i].sa)] = 0;
        board[m(path[i].na)] = path[i].step;
        print_state_from_board(board);
    }

    free(board);
    free(path);
    free(all_pos);
    return 0;
}
