#include <stdio.h>
#include <stdlib.h>

#define MAX_N 6
#define MAX_STANJA 20000
#define MAX_POTEZA 500

int tabla[2 * MAX_N + 5];
int global_n;

// Struktura za ispis poteza
typedef struct {
    int step;
    int sa;
    int na;
} Potez;

Potez istorija[MAX_POTEZA];
unsigned long long stanja[MAX_STANJA];
int broj_stanja = 0;

int m(int poz) { return poz + global_n + 1; }

void prikazi_stanje() {
    printf("L: ");
    for (int i = -1; i >= -global_n; i--) printf("[%d]", tabla[m(i)]);
    printf(" | G:%d D:%d | R: ", tabla[m(global_n+1)], tabla[m(-(global_n+1))]);
    for (int i = global_n; i >= 1; i--) printf("[%d]", tabla[m(i)]);
    printf("\n");
}

int cilj_postignut() {
    for (int i = 1; i <= global_n; i++) {
        if (tabla[m(i)] == 0) return 0;
    }
    return 1;
}

int put_prohodan(int sa, int na) {
    // Ako je isto polje - nema kretanja
    if (sa == na) return 0;

    // Ako su oba na istoj strani i unutar table (-n..-1 ili 1..n),
    // provjeri samo polja IZMEĐU njih.
    if (sa >= -global_n && sa <= global_n &&
        na >= -global_n && na <= global_n &&
        sa != 0 && na != 0) {

        // ista strana: oba <0 ili oba >0
        if ((sa < 0 && na < 0) || (sa > 0 && na > 0)) {
            int a = sa, b = na;
            if (a > b) { int t = a; a = b; b = t; }

            for (int k = a + 1; k <= b - 1; k++) {
                if (k == 0) continue;
                if (tabla[m(k)] != 0) return 0;
            }
            return 1;
        }
    }

    // Inače (prelaz na “garaže” ili preko sredine):
    // zadrži tvoju staru logiku “do ivice” kao aproksimaciju.
    // (Ako želiš strožije pravilo za prelaz, reci kako tačno važe pravila igre.)
    if (sa >= -global_n && sa <= global_n) {
        if (sa < 0) { for (int i = sa - 1; i >= -global_n; i--) if (tabla[m(i)] != 0) return 0; }
        else        { for (int i = sa + 1; i <=  global_n; i++) if (tabla[m(i)] != 0) return 0; }
    }
    if (na >= -global_n && na <= global_n) {
        if (na < 0) { for (int i = -global_n; i > na; i--) if (tabla[m(i)] != 0) return 0; }
        else        { for (int i =  global_n; i > na; i--) if (tabla[m(i)] != 0) return 0; }
    }
    return 1;
}

int moze_stati(int step, int poz) {
    if (poz > global_n || poz < -global_n) return 1;
    int abs_p = (poz < 0) ? -poz : poz;
    return abs_p >= step;
}

unsigned long long generisi_hash() {
    unsigned long long h = 0;
    for (int i = 0; i < 2 * global_n + 3; i++) h = h * 31 + tabla[i];
    return h;
}

int stanje_vec_vidjeno() {
    unsigned long long h = generisi_hash();
    for (int i = 0; i < broj_stanja; i++) if (stanja[i] == h) return 1;
    return 0;
}

int solve(int dubina) {
    if (cilj_postignut()) return 1;
    if (dubina >= MAX_POTEZA || broj_stanja >= MAX_STANJA) return 0;

    stanja[broj_stanja++] = generisi_hash();

    for (int s = 1; s <= global_n; s++) {
        int trenutna_poz = 0;
        for (int k = -(global_n+1); k <= (global_n+1); k++) {
            if (k != 0 && tabla[m(k)] == s) { trenutna_poz = k; break; }
        }

        for (int cilj = -(global_n+1); cilj <= (global_n+1); cilj++) {
            if (cilj == 0 || cilj == trenutna_poz) continue;

            if (tabla[m(cilj)] == 0 && moze_stati(s, cilj) && put_prohodan(trenutna_poz, cilj)) {
                tabla[m(trenutna_poz)] = 0;
                tabla[m(cilj)] = s;

                if (!stanje_vec_vidjeno()) {
                    istorija[dubina] = (Potez){s, trenutna_poz, cilj};
                    if (solve(dubina + 1)) return 1;
                }

                tabla[m(cilj)] = 0;
                tabla[m(trenutna_poz)] = s;
            }
        }
    }
    broj_stanja--;
    return 0;
}

int main() {
    printf("Unesite n (max 6): ");
    if (scanf("%d", &global_n) != 1) return 1;

    for (int i = 0; i < 2 * global_n + 5; i++) tabla[i] = 0;
    for (int i = 1; i <= global_n; i++) tabla[m(-i)] = i;

    printf("\nTrazenje rjesenja...\n");
    if (solve(0)) {
        // Resetujemo tablu za prikaz korak po korak
        for (int i = 0; i < 2 * global_n + 5; i++) tabla[i] = 0;
        for (int i = 1; i <= global_n; i++) tabla[m(-i)] = i;

        printf("START:"); prikazi_stanje();
        for (int i = 0; istorija[i].step != 0; i++) {
            Potez p = istorija[i];
            tabla[m(p.sa)] = 0;
            tabla[m(p.na)] = p.step;
            printf("Pomjeri %d sa %d na %d", p.step, p.sa, p.na);
            prikazi_stanje();
            if (cilj_postignut()) break;
        }
    } else {
        printf("Nema rjesenja u zadatom broju koraka.\n");
    }
    return 0;
}
