/*
 * depman_thread.c — Vérification parallèle des dépendances via POSIX pthreads
 * ESET Mohammedia 2025-2026
 *
 * Usage : ./depman_thread <paquet1> <paquet2> ... <paquetN>
 * Sortie (une ligne par paquet) : nom_paquet:1 (présent) ou nom_paquet:0 (absent)
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

/* ── Structure d'une dépendance ──────────────────────────────────────────── */
typedef struct {
    char name[64];      /* nom du paquet              */
    char version[16];   /* version requise (optionnel) */
    int  result;        /* 1 = présent/OK, 0 = absent  */
} Dep;

/* ── Mutex pour protéger les sorties printf ──────────────────────────────── */
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Fonction exécutée par chaque thread ─────────────────────────────────── */
void *check_package(void *arg) {
    Dep *d = (Dep *)arg;
    char cmd[256];

    /*
     * Vérifie si le paquet est installé (état "ii" dans dpkg -l).
     * On redirige stderr vers /dev/null pour ne pas polluer la sortie.
     */
    snprintf(cmd, sizeof(cmd),
             "dpkg -l %s 2>/dev/null | grep -q '^ii'",
             d->name);

    d->result = (system(cmd) == 0) ? 1 : 0;

    /* Affichage thread-safe */
    pthread_mutex_lock(&print_mutex);
    printf("%s:%d\n", d->name, d->result);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);

    return NULL;
}

/* ── Point d'entrée ──────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <paquet1> [paquet2 ...]\n", argv[0]);
        return 1;
    }

    int n = argc - 1;
    Dep        *deps    = calloc((size_t)n, sizeof(Dep));
    pthread_t  *threads = calloc((size_t)n, sizeof(pthread_t));

    if (!deps || !threads) {
        perror("calloc");
        free(deps);
        free(threads);
        return 1;
    }

    /* Créer un thread par paquet */
    for (int i = 0; i < n; i++) {
        strncpy(deps[i].name, argv[i + 1], sizeof(deps[i].name) - 1);
        deps[i].name[sizeof(deps[i].name) - 1] = '\0';
        deps[i].result = 0;

        if (pthread_create(&threads[i], NULL, check_package, &deps[i]) != 0) {
            perror("pthread_create");
            /* Annuler les threads déjà lancés */
            for (int j = 0; j < i; j++) {
                pthread_cancel(threads[j]);
            }
            free(deps);
            free(threads);
            return 1;
        }
    }

    /* Attendre la fin de tous les threads */
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&print_mutex);
    free(deps);
    free(threads);
    return 0;
}
