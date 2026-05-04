# depman — Gestionnaire de Dépendances Bash

`depman` est un outil en ligne de commande écrit en **Bash 5.x** qui automatise
la vérification et l'installation des dépendances d'un projet à partir d'un
fichier `deps.conf`. Il propose trois modes d'exécution (subshell, fork,
threads C) et maintient un journal horodaté de toutes les actions.

> **Distros supportées :** Debian/Ubuntu (`apt`) • Arch (`pacman`) • Fedora/RHEL (`dnf`) • CentOS ancien (`yum`) • openSUSE (`zypper`) • Alpine (`apk`)

---

## Table des matières

1. [Prérequis](#prérequis)
2. [Installation](#installation)
3. [Utilisation](#utilisation)
4. [Format de deps.conf](#format-de-depsconf)
5. [Options](#options)
6. [Codes d'erreur](#codes-derreur)
7. [Journalisation](#journalisation)
8. [Snapshots](#snapshots)
9. [Scénarios de test](#scénarios-de-test)
10. [Architecture](#architecture)

---

## Prérequis

| Outil | Version minimale | Usage |
|-------|-----------------|-------|
| Bash | 5.x | Script principal |
| gcc | 9.0 | Compilation de `depman_thread.c` |
| getopt | — | Parsing des options |

### Gestionnaires de paquets supportés

| Gestionnaire | Distros |
|-------------|--------|
| `apt-get` | Debian, Ubuntu, Kali, Mint, Raspbian |
| `pacman` | Arch Linux, Manjaro, EndeavourOS |
| `dnf` | Fedora, RHEL 8+, Rocky, AlmaLinux |
| `yum` | CentOS 7, RHEL 7 et antérieurs |
| `zypper` | openSUSE Leap / Tumbleweed |
| `apk` | Alpine Linux, images Docker |

Le gestionnaire est **détecté automatiquement** au démarrage via `detect_pm()`.

---

## Installation

```bash
# Cloner le dépôt
git clone https://github.com/yvcinne/depman
cd depman/

# Rendre le script exécutable
chmod +x depman

# (Optionnel) Installer globalement
sudo cp depman /usr/local/bin/

# Créer le répertoire de log (ou utiliser -l pour un chemin alternatif)
sudo mkdir -p /var/log/depman
sudo chmod 755 /var/log/depman
```

> **Note :** Le répertoire `/var/log/depman/` est créé automatiquement au
> premier lancement si les droits le permettent. Sinon, utilisez `-l` pour
> spécifier un chemin accessible sans root.

---

## Utilisation

```bash
depman [OPTIONS] <projet>
```

`<projet>` est le chemin vers un répertoire contenant un fichier `deps.conf`.

### Exemples rapides

```bash
# Vérification légère en mode subshell
depman -s projet-light

# Vérification parallèle en mode fork (installe les manquants)
depman -f projet-medium

# Vérification ultra-rapide en mode thread (C + pthreads)
depman -t projet-heavy

# Log alternatif (sans root)
depman -l ./my.log -s projet-light

# Restauration de l'environnement (root requis)
sudo depman -r projet-medium
```

---

## Format de deps.conf

```ini
# Fichier de configuration des dépendances
# Format : nom_paquet >= version_minimale

[project:monapp]
git >= 2.30
curl >= 7.0
python3 >= 3.8
nodejs >= 16.0
gcc >= 9.0
make >= 4.0
```

**Règles de parsing :**
- Lignes commençant par `#` → ignorées (commentaires)
- Lignes entre `[...]` → ignorées (sections)
- Lignes vides → ignorées
- Chaque ligne valide : `<paquet> >= <version>`

---

## Options

| Option | Description |
|--------|-------------|
| `-s <projet>` | **Mode subshell** — vérification séquentielle dans un sous-shell (isolation des variables) |
| `-f <projet>` | **Mode fork** — un processus fils par paquet, vérification parallèle |
| `-t <projet>` | **Mode thread** — compile et exécute `depman_thread.c` (POSIX pthreads) |
| `-l <fichier>` | Chemin alternatif pour le fichier de log |
| `-r <projet>` | **Restauration** — rétablit l'état dpkg depuis le dernier snapshot (**root requis**) |
| `-h` | Affiche l'aide complète |

---

## Codes d'erreur

| Code | Description | Déclencheur |
|------|-------------|-------------|
| 100 | Option inexistante | `getopt` ne reconnaît pas l'option |
| 101 | Paramètre `<projet>` manquant | Aucun argument positionnel fourni |
| 102 | Fichier `deps.conf` introuvable | Fichier absent dans le répertoire projet |
| 103 | Paquet introuvable dans les dépôts | Le gestionnaire de paquets détecté ne trouve pas le paquet |
| 104 | Privilèges root requis | Option `-r` sans `sudo`/root |
| 105 | Snapshot introuvable | Fichier snapshot absent pour `-r` |
| 106 | Compilation `depman_thread.c` échouée | `gcc` retourne un code non-zéro |

Chaque erreur affiche l'aide complète (`-h`) puis quitte avec le code correspondant.

---

## Journalisation

Toutes les actions sont enregistrées **simultanément** dans le terminal et dans
le fichier log via `tee -a`.

**Fichier par défaut :** `/var/log/depman/history.log`  
**Format :**

```
yyyy-mm-dd-hh-mm-ss:username:INFOS:message
yyyy-mm-dd-hh-mm-ss:username:ERROR:message
```

**Exemples concrets :**

```
2026-04-23-14-32-01:alice:INFOS:Vérification de git >= 2.30 -> OK (v2.43)
2026-04-23-14-32-02:alice:INFOS:nodejs absent, installation en cours...
2026-04-23-14-32-10:alice:INFOS:nodejs installé avec succès (v18.19)
2026-04-23-14-32-15:alice:ERROR:[Erreur 103] gcc introuvable dans les dépôts apt
```

---

## Snapshots

Un snapshot capture l'état complet des paquets installés.

- **Création automatique** à la fin de chaque exécution réussie (`-s`, `-f`, `-t`).
- **Emplacement :** `/var/log/depman/snapshots/<projet>_YYYYMMDDHHMMSS.snap`
- **Restauration :** `sudo depman -r <projet>` (code 105 si aucun snapshot trouvé)

| Gestionnaire | Commande snapshot | Restauration |
|-------------|------------------|--------------|
| `apt` | `dpkg --get-selections` | `dpkg --set-selections` + `apt-get` |
| `pacman` | `pacman -Qqe` | `pacman -S` |
| `dnf/yum/zypper` | `rpm -qa` | Informatif uniquement |
| `apk` | `apk info` | Informatif uniquement |

---

## Scénarios de test

### Scénario 1 — Léger (subshell)

```bash
mkdir -p projet-light
cp deps.conf projet-light/deps.conf   # ou créer un deps.conf avec 2-3 paquets
depman -s projet-light
```

**Résultat attendu :**
- 3 lignes `INFOS` dans `history.log`
- Code retour : `0`
- Aucune installation déclenchée (paquets déjà présents)
- Variables du shell parent inchangées

---

### Scénario 2 — Moyen (fork)

```bash
mkdir -p projet-medium
cp deps.conf projet-medium/deps.conf  # 5-6 paquets, 2-3 potentiellement absents
depman -f projet-medium
```

**Résultat attendu :**
- 5+ processus fils créés (PID affichés dans les logs)
- Paquets manquants installés via `apt-get`
- Logs mixtes `INFOS` + `ERROR` (code 103 si paquet absent des dépôts)
- Snapshot créé dans `snapshots/`

---

### Scénario 3 — Lourd (thread)

```bash
mkdir -p projet-heavy
cp deps.conf projet-heavy/deps.conf   # 10+ paquets
depman -t projet-heavy
```

**Résultat attendu :**
- `depman_thread.c` compilé automatiquement
- 10+ threads simultanés lancés par `depman_thread`
- Rapport de temps (`time`) affiché
- Snapshot complet de l'environnement
- Mode thread plus rapide que fork sur 10+ paquets

---

## Architecture

```
depman/
├── depman            # Script principal Bash (exécutable)
├── depman_thread.c   # Programme C (option -t, vérification parallèle)
├── deps.conf         # Exemple de configuration
├── gen-deps          # Helper : génère deps.conf depuis les paquets installés
├── README.md         # Ce fichier
└── /var/log/depman/          # Créé automatiquement
    ├── history.log            # Journal horodaté de toutes les actions
    └── snapshots/             # Sauvegardes d'état des paquets
        └── projet_YYYYMMDDHHMMSS.snap
```

### Composants

| Composant | Rôle | Technologie |
|-----------|------|-------------|
| `depman` | Script principal, point d'entrée | Bash 5.x |
| `depman_thread.c` | Vérification parallèle | C + POSIX pthreads |
| `deps.conf` | Déclaration déclarative des dépendances | Format `.conf` maison |
| `gen-deps` | Génère `deps.conf` depuis les paquets installés | Bash 5.x |
| `history.log` | Journal horodaté | Texte structuré |
| `snapshots/` | Sauvegardes d'état | apt/pacman/rpm/apk |

### Fonctions Bash principales

| Fonction | Rôle |
|----------|------|
| `detect_pm()` | Détecte le gestionnaire de paquets (`apt`/`pacman`/`dnf`/`yum`/`zypper`/`apk`) |
| `parse_conf()` | Lit et parse `deps.conf` |
| `check_dep()` | Vérifie si un paquet est installé et sa version (distro-agnostique) |
| `install_dep()` | Installe un paquet via le bon gestionnaire (distro-agnostique) |
| `snapshot()` | Capture l'état courant des paquets |
| `restore_snapshot()` | Restaure depuis un snapshot |
| `log()` / `log_info()` / `log_error()` | Journalisation horodatée |
| `handle_error()` | Gestion centralisée des erreurs |
| `show_help()` | Affichage de la documentation |
| `compile_thread_prog()` | Compilation automatique du programme C |
| `run_subshell()` | Mode `-s` |
| `run_fork()` | Mode `-f` |
| `run_thread()` | Mode `-t` |
| `run_restore()` | Mode `-r` |

---


