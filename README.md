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
4. [Options](#options)
5. [Codes d'erreur](#codes-derreur)
6. [Journalisation](#journalisation)
7. [Snapshots](#snapshots)
8. [Scénarios de test](#scénarios-de-test)
9. [Architecture](#architecture)
10. [gen-deps — Générer deps.conf automatiquement](#gen-deps--générer-depsconf-automatiquement)

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

# Compiler et installer le programme thread (requis pour l'option -t)
sudo gcc -o /usr/local/bin/depman_thread depman_thread.c -lpthread

# Créer le répertoire de log (ou utiliser -l pour un chemin alternatif)
sudo mkdir -p /var/log/depman
sudo chmod 755 /var/log/depman
```

> **Note :** Le répertoire `/var/log/depman/` est créé automatiquement au
> premier lancement si les droits le permettent. Sinon, utilisez `-l` pour
> spécifier un chemin accessible sans root.

> **Important :** Lorsque `depman` est installé dans `/usr/local/bin/`, il
> cherche le binaire `depman_thread` **dans le même répertoire**. La commande
> `gcc` ci-dessus compile `depman_thread.c` et place le binaire résultant
> au bon endroit. Sans cette étape, l'option `-t` échoue avec l'erreur 106.

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



## Options

| Option | Description |
|--------|-------------|
| `-s <projet>` | **Mode subshell** — vérification séquentielle dans un sous-shell (isolation des variables) |
| `-f <projet>` | **Mode fork** — un processus fils par paquet, vérification parallèle |
| `-t <projet>` | **Mode thread** — exécute `depman_thread` (C + POSIX pthreads) — voir [Installation](#installation) |
| `-r <projet> [horodatage]` | **Restauration** — restaure depuis le dernier snapshot ou un snapshot précis (**root requis**) |
| `-l <répertoire>` | Répertoire alternatif pour le fichier de log (`history.log` y sera créé) |
| `-n` / `--dry-run` | **Mode simulation** — affiche ce qui serait installé sans rien modifier |
| `-h` / `--help` | Affiche l'aide complète |

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
| 106 | `depman_thread.c` introuvable **ou** compilation échouée | Binaire absent de `/usr/local/bin/` ou `gcc` retourne un code non-zéro — voir [Installation](#installation) |

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

> **Prérequis :** `depman_thread` doit être compilé et installé (voir [Installation](#installation)).

```bash
# Si depman est installé globalement, compiler depman_thread d'abord :
sudo gcc -o /usr/local/bin/depman_thread depman_thread.c -lpthread

mkdir -p projet-heavy
cp deps.conf projet-heavy/deps.conf   # 10+ paquets
sudo depman -t projet-heavy
```

**Résultat attendu :**
- `depman_thread` trouvé dans `/usr/local/bin/` et exécuté directement
- 10+ threads simultanés lancés par `depman_thread`
- Progression `[X/N]` affichée lors du traitement des résultats
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

## gen-deps — Générer deps.conf automatiquement

`gen-deps` est un script Bash companion de `depman`. Il **inspecte les paquets
installés sur votre machine** et génère automatiquement un fichier `deps.conf`
prêt à l'emploi. Aucun accès réseau n'est requis — tout est lu depuis la base
de données locale de votre gestionnaire de paquets.

---

### Installation

#### Utilisation locale (depuis le dépôt cloné)

```bash
cd depman/
./gen-deps
```

#### Installation globale (recommandée)

```bash
sudo cp gen-deps /usr/local/bin/gen-deps
sudo chmod +x /usr/local/bin/gen-deps
```

> Une fois installé globalement, remplacez `./gen-deps` par `gen-deps` dans
> tous les exemples ci-dessous.

---

### Syntaxe

```
gen-deps [<projet>] [paquet1 paquet2 ...]
```

| Argument | Obligatoire | Description |
|----------|-------------|-------------|
| `<projet>` | Non | Répertoire cible où `deps.conf` sera créé. Si omis, le fichier est généré dans le dossier courant (`./deps.conf`). |
| `paquet1 paquet2 ...` | Non | Paquets à inclure. Si omis, la liste par défaut est utilisée. |

---

### Cas d'utilisation

#### 1. Sans argument — dossier courant, paquets par défaut

```bash
./gen-deps
```

Génère `./deps.conf` avec tous les paquets de la liste par défaut qui sont
installés sur votre système.

**Quand l'utiliser :** Démarrage rapide, environnement de développement
générique.

---

#### 2. Nom de projet seulement

```bash
./gen-deps mon-projet
```

Crée le dossier `mon-projet/` s'il n'existe pas, puis génère
`mon-projet/deps.conf` avec les paquets par défaut.

**Quand l'utiliser :** Initialiser rapidement la configuration d'un nouveau
projet.

---

#### 3. Projet + paquets explicites

```bash
./gen-deps mon-projet git curl gcc make python
```

Génère `mon-projet/deps.conf` avec **uniquement** les paquets listés.
Chaque version est lue depuis la base locale du gestionnaire de paquets.

**Quand l'utiliser :** Vous connaissez exactement les dépendances de votre
projet et voulez un `deps.conf` minimal et précis.

---

#### 4. Dossier courant (`.`) + paquets explicites

```bash
cd ~/projects/mon-app
gen-deps . git curl python openssl
```

Génère `./deps.conf` dans le dossier courant sans créer de sous-dossier.

**Quand l'utiliser :** Vous êtes déjà dans le répertoire de votre projet.


---

#### 5. Paquet non installé sur le système

Si un paquet demandé **n'est pas installé**, il apparaît en commentaire :

```ini
# nodejs  (non installé — version minimale à définir manuellement)
```

Vous pouvez compléter manuellement la version minimale requise avant de
partager le fichier avec votre équipe.

---

### Paquets par défaut

Utilisés lorsqu'aucun paquet n'est fourni en argument :

```
git  curl  wget  python (ou python3)  gcc  make  cmake
tar  unzip  rsync  openssl  bash
```

> **Détection cross-distro :** `gen-deps` choisit automatiquement le bon
> nom du paquet Python selon le gestionnaire détecté :
>
> | Gestionnaire | Paquet utilisé |
> |---|---|
> | `pacman` (Arch, Manjaro…) | `python` |
> | `apt`, `dnf`, `yum`, `zypper`, `apk` | `python3` |

---

### Récupération des versions

`gen-deps` interroge uniquement la base de données **locale** — pas de
requête réseau.

| Gestionnaire | Commande utilisée |
|---|---|
| `apt` | `dpkg -l <pkg>` → champ version |
| `pacman` | `pacman -Q <pkg>` → champ version |
| `dnf / yum / zypper` | `rpm -q --qf '%{VERSION}' <pkg>` |
| `apk` | `apk info <pkg>` → première ligne |

---

### Format du fichier généré

```ini
# Fichier de configuration des dépendances
# Généré automatiquement le YYYY-MM-DD HH:MM:SS par gen-deps
# Format : nom_paquet >= version_minimale

[project:<nom-projet>]
git                  >= 2.54.0
curl                 >= 8.20.0
wget                 >= 1.25.0
python               >= 3.14.4
gcc                  >= 16.1.1
make                 >= 4.4.1
cmake                >= 4.3.2
tar                  >= 1.35
unzip                >= 6.0
rsync                >= 3.4.2
openssl              >= 3.6.2
bash                 >= 5.3.9
# build-essential  (non installé — version minimale à définir manuellement)
```

**Règles de format (compatibles avec le parser de `depman`) :**

| Élément | Règle |
|---------|-------|
| Commentaires | Lignes commençant par `#` → ignorées par `depman` |
| Sections | Lignes entre `[...]` → ignorées par `depman` |
| Lignes vides | Ignorées par `depman` |
| Dépendance valide | `<paquet> >= <version>` (opérateur `>=` obligatoire) |
| Alignement | Nom du paquet sur 20 caractères, puis `>= version` |
| En-tête | Horodatage automatique à la génération |

---

### Workflow recommandé

```bash
# 1 — Générer le deps.conf depuis votre machine de référence
./gen-deps mon-projet git curl python gcc make

# 2 — Ouvrir et vérifier le fichier généré
cat mon-projet/deps.conf

# 3 — Simuler l'installation sur une autre machine (aucune modification)
depman -n -s mon-projet

# 4 — Lancer l'installation réelle
sudo depman -s mon-projet

# 5 — Vérifier le snapshot créé automatiquement
ls /var/log/depman/snapshots/
```

---

