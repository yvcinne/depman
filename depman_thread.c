/*
 * depman_thread.c — Vérification parallèle des dépendances via POSIX pthreads
 * Cross-distro : apt · pacman · dnf/rpm · apk
 *
 * Usage : ./depman_thread <paquet1> <paquet2> ... <paquetN>
 * Sortie (une ligne par paquet) : nom_paquet:1 (présent) ou nom_paquet:0 (absent)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* ── Gestionnaires de paquets supportés ──────────────────────────────────── */
typedef enum {
    PM_APT,
    PM_PACMAN,
    PM_RPM,   /* dnf / yum / zypper */
    PM_APK,
    PM_UNKNOWN
} PmType;

/* Global — détecté une seule fois dans main(), lu en lecture seule ensuite */
static PmType g_pm = PM_UNKNOWN;

/* ── Structure d'une dépendance ──────────────────────────────────────────── */
typedef struct {
    char name[128];  /* nom du paquet              */
    int  result;     /* 1 = présent, 0 = absent    */
} Dep;

/* ── Mutex pour protéger les sorties printf ──────────────────────────────── */
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Détecte le gestionnaire de paquets ──────────────────────────────────── */
static PmType detect_pm(void) {
    if (access("/usr/bin/dpkg",   X_OK) == 0 ||
        access("/bin/dpkg",       X_OK) == 0) return PM_APT;
    if (access("/usr/bin/pacman", X_OK) == 0) return PM_PACMAN;
    if (access("/usr/bin/rpm",    X_OK) == 0) return PM_RPM;
    if (access("/sbin/apk",       X_OK) == 0 ||
        access("/usr/bin/apk",    X_OK) == 0) return PM_APK;
    return PM_UNKNOWN;
}

/* ── Construit la commande de vérification selon le PM ───────────────────── */
static void build_check_cmd(const char *pkg, char *buf, size_t bufsz) {
    switch (g_pm) {
        case PM_APT:
            snprintf(buf, bufsz,
                     "dpkg -l '%s' 2>/dev/null | grep -q '^ii'", pkg);
            break;
        case PM_PACMAN:
            snprintf(buf, bufsz,
                     "pacman -Q '%s' >/dev/null 2>&1", pkg);
            break;
        case PM_RPM:
            snprintf(buf, bufsz,
                     "rpm -q '%s' >/dev/null 2>&1", pkg);
            break;
        case PM_APK:
            snprintf(buf, bufsz,
                     "apk info -e '%s' >/dev/null 2>&1", pkg);
            break;
        default:
            /* Commande qui échoue toujours → paquet marqué absent */
            snprintf(buf, bufsz, "false");
            break;
    }
}

/* ── Fonction exécutée par chaque thread ─────────────────────────────────── */
static void *check_package(void *arg) {
    Dep  *d = (Dep *)arg;
    char  cmd[512];

    build_check_cmd(d->name, cmd, sizeof(cmd));
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

    /* Détection du gestionnaire de paquets */
    g_pm = detect_pm();
    if (g_pm == PM_UNKNOWN) {
        fprintf(stderr, "depman_thread: aucun gestionnaire de paquets reconnu\n");
        return 1;
    }

    int         n       = argc - 1;
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
            for (int j = 0; j < i; j++) pthread_cancel(threads[j]);
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
