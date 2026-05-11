/*
 * depman_thread.c — Vérification parallèle des dépendances via POSIX pthreads
 * ESET Mohammedia 2025-2026
 *
 * Usage : ./depman_thread <pm> <paquet1> <paquet2> ... <paquetN>
 *   <pm>  : gestionnaire de paquets détecté (apt | pacman | dnf | yum | zypper | apk)
 *
 * Sortie (une ligne par paquet) : nom_paquet:1 (présent) ou nom_paquet:0 (absent)
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

/* ── Gestionnaire de paquets (global, en lecture seule après init) ─────────── */
static char g_pm[32] = "unknown";

/* ── Structure d'une dépendance ──────────────────────────────────────────── */
typedef struct {
    char name[64];   /* nom du paquet             */
    int  result;     /* 1 = présent/OK, 0 = absent */
} Dep;

/* ── Mutex pour protéger les sorties printf ──────────────────────────────── */
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Construit la commande de vérification selon le PM ───────────────────── */
static void build_check_cmd(char *buf, size_t bufsz, const char *pkg) {
    if (strcmp(g_pm, "apt") == 0) {
        snprintf(buf, bufsz,
                 "dpkg -l %s 2>/dev/null | grep -q '^ii'", pkg);
    } else if (strcmp(g_pm, "pacman") == 0) {
        snprintf(buf, bufsz,
                 "pacman -Q %s >/dev/null 2>&1", pkg);
    } else if (strcmp(g_pm, "dnf")    == 0 ||
               strcmp(g_pm, "yum")    == 0 ||
               strcmp(g_pm, "zypper") == 0) {
        snprintf(buf, bufsz,
                 "rpm -q %s >/dev/null 2>&1", pkg);
    } else if (strcmp(g_pm, "apk") == 0) {
        snprintf(buf, bufsz,
                 "apk info -e %s >/dev/null 2>&1", pkg);
    } else {
        /* Fallback générique : vérifie si la commande est dans le PATH */
        snprintf(buf, bufsz,
                 "command -v %s >/dev/null 2>&1", pkg);
    }
}

/* ── Fonction exécutée par chaque thread ─────────────────────────────────── */
void *check_package(void *arg) {
    Dep *d = (Dep *)arg;
    char cmd[256];

    build_check_cmd(cmd, sizeof(cmd), d->name);
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
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <pm> <paquet1> [paquet2 ...]\n"
                "  <pm> : apt | pacman | dnf | yum | zypper | apk\n",
                argv[0]);
        return 1;
    }

    /* Premier argument : gestionnaire de paquets */
    strncpy(g_pm, argv[1], sizeof(g_pm) - 1);
    g_pm[sizeof(g_pm) - 1] = '\0';

    int n = argc - 2;   /* nombre de paquets (argv[2..argc-1]) */
    Dep       *deps    = calloc((size_t)n, sizeof(Dep));
    pthread_t *threads = calloc((size_t)n, sizeof(pthread_t));

    if (!deps || !threads) {
        perror("calloc");
        free(deps);
        free(threads);
        return 1;
    }

    /* Créer un thread par paquet */
    for (int i = 0; i < n; i++) {
        strncpy(deps[i].name, argv[i + 2], sizeof(deps[i].name) - 1);
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
