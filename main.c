#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_N 6
#define MAX_POTEZA 2000
#define VIS_SIZE 1048576

int tabla[2 * MAX_N + 5];
int global_n;

typedef struct { int step, sa, na; } Potez;

Potez istorija[MAX_POTEZA];

unsigned long long vis[VIS_SIZE];
unsigned char vis_used[VIS_SIZE];

int m(int p) { return p + global_n + 1; }

int valid_pos(int p) {
    if (p == 0) return 0;
    if (p >= -global_n && p <= global_n && p != 0) return 1;
    if (p == -(global_n + 1) || p == (global_n + 1)) return 1;
    return 0;
}

void prikazi_stanje() {
    printf("L: ");
    for (int i = -1; i >= -global_n; i--) printf("[%d]", tabla[m(i)]);
    printf(" | G:%d D:%d | R: ", tabla[m(-(global_n+1))], tabla[m(global_n+1)]);
    for (int i = global_n; i >= 1; i--) printf("[%d]", tabla[m(i)]);
    printf("\n");
}

int cilj_postignut() {
    for (int i = 1; i <= global_n; i++) if (tabla[m(i)] == 0) return 0;
    return 1;
}

int moze_stati(int s, int p) {
    if (p > global_n || p < -global_n) return 1;
    int a = p < 0 ? -p : p;
    return a >= s;
}

unsigned long long hash_stanja() {
    unsigned long long h = 1469598103934665603ULL;
    for (int i = 0; i < 2 * global_n + 3; i++) {
        h ^= (unsigned long long)tabla[i] + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
    }
    return h;
}

int vis_check_add(unsigned long long h) {
    unsigned long long idx = h & (VIS_SIZE - 1);
    for (int t = 0; t < 64; t++) {
        unsigned long long j = (idx + t) & (VIS_SIZE - 1);
        if (!vis_used[j]) {
            vis_used[j] = 1;
            vis[j] = h;
            return 0;
        }
        if (vis[j] == h) return 1;
    }
    return 0;
}

int put_prohodan(int sa, int na) {
    if (!valid_pos(sa) || !valid_pos(na)) return 0;
    if (sa == na) return 0;
    if (sa == 0 || na == 0) return 0;

    int saL = (sa >= -global_n && sa <= -1);
    int naL = (na >= -global_n && na <= -1);
    int saR = (sa >= 1 && sa <= global_n);
    int naR = (na >= 1 && na <= global_n);

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
        for (int i = sa - 1; i >= -global_n; i--) {
            if (tabla[m(i)] != 0) return 0;
        }
    } else if (saR) {
        for (int i = sa + 1; i <= global_n; i++) {
            if (tabla[m(i)] != 0) return 0;
        }
    }

    if (naL) {
        for (int i = -global_n; i < na; i++) {
            if (tabla[m(i)] != 0) return 0;
        }
    } else if (naR) {
        for (int i = global_n; i > na; i--) {
            if (tabla[m(i)] != 0) return 0;
        }
    }

    return 1;
}

int solve(int d) {
    if (cilj_postignut()) return 1;
    if (d >= MAX_POTEZA) return 0;

    unsigned long long h = hash_stanja();
    if (vis_check_add(h)) return 0;

    for (int s = 1; s <= global_n; s++) {
        int sa = 0;
        for (int k = -(global_n + 1); k <= (global_n + 1); k++) {
            if (k != 0 && tabla[m(k)] == s) { sa = k; break; }
        }
        if (!sa) continue;

        for (int na = -(global_n + 1); na <= (global_n + 1); na++) {
            if (na == 0 || na == sa) continue;
            if (tabla[m(na)] != 0) continue;
            if (!moze_stati(s, na)) continue;
            if (!put_prohodan(sa, na)) continue;

            tabla[m(sa)] = 0;
            tabla[m(na)] = s;

            istorija[d] = (Potez){s, sa, na};
            istorija[d + 1] = (Potez){0, 0, 0};

            if (solve(d + 1)) return 1;

            tabla[m(na)] = 0;
            tabla[m(sa)] = s;
            istorija[d] = (Potez){0, 0, 0};
        }
    }
    return 0;
}

int main() {
    if (scanf("%d", &global_n) != 1) return 0;
    if (global_n < 1 || global_n > MAX_N) return 0;

    memset(tabla, 0, sizeof(tabla));
    memset(istorija, 0, sizeof(istorija));
    memset(vis_used, 0, sizeof(vis_used));

    for (int i = 1; i <= global_n; i++) tabla[m(-i)] = i;

    if (!solve(0)) {
        printf("Nema rjesenja\n");
        return 0;
    }

    memset(tabla, 0, sizeof(tabla));
    for (int i = 1; i <= global_n; i++) tabla[m(-i)] = i;

    printf("START: ");
    prikazi_stanje();
    for (int i = 0; i < MAX_POTEZA && istorija[i].step; i++) {
        Potez p = istorija[i];
        tabla[m(p.sa)] = 0;
        tabla[m(p.na)] = p.step;
        printf("Pomjeri %d sa %d na %d ", p.step, p.sa, p.na);
        prikazi_stanje();
        if (cilj_postignut()) break;
    }

    return 0;
}
