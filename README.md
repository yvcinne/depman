# depman — Bash Dependency Manager

`depman` is a command-line tool written in **Bash 5.x** that automates
the verification and installation of project dependencies from a
`deps.conf` file. It offers three execution modes (subshell, fork,
C threads) and maintains a timestamped log of all actions.

> **Supported distros:** Debian/Ubuntu (`apt`) • Arch (`pacman`) • Fedora/RHEL (`dnf`) • Old CentOS (`yum`) • openSUSE (`zypper`) • Alpine (`apk`)

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
3. [Usage](#usage)
4. [Options](#options)
5. [Error Codes](#error-codes)
6. [Logging](#logging)
7. [Snapshots](#snapshots)
8. [Snapshot Comparison](#snapshot-comparison)
9. [Test Scenarios](#test-scenarios)
10. [Architecture](#architecture)
11. [gen-deps — Generate deps.conf automatically](#gen-deps--generate-depsconf-automatically)

---

## Prerequisites

| Tool | Minimum version | Usage |
|------|-----------------|-------|
| Bash | 5.x | Main script |
| gcc | 9.0 | Compiling `depman_thread.c` |
| getopt | — | Option parsing |

### Supported package managers

| Manager | Distros |
|---------|---------|
| `apt-get` | Debian, Ubuntu, Kali, Mint, Raspbian |
| `pacman` | Arch Linux, Manjaro, EndeavourOS |
| `dnf` | Fedora, RHEL 8+, Rocky, AlmaLinux |
| `yum` | CentOS 7, RHEL 7 and older |
| `zypper` | openSUSE Leap / Tumbleweed |
| `apk` | Alpine Linux, Docker images |

The manager is **auto-detected** at startup via `detect_pm()`.

---

## Installation

```bash
# Clone the repository
git clone https://github.com/yvcinne/depman
cd depman/

# Make the script executable
chmod +x depman

# (Optional) Install globally
sudo cp depman /usr/local/bin/

# Compile and install the thread program (required for the -t option)
sudo gcc -o /usr/local/bin/depman_thread depman_thread.c -lpthread

# Create the log directory (or use -l for an alternative path)
sudo mkdir -p /var/log/depman
sudo chmod 755 /var/log/depman
```

> **Note:** The `/var/log/depman/` directory is created automatically on
> first launch if permissions allow. Otherwise, use `-l` to specify a
> path accessible without root.

> **Important:** When `depman` is installed in `/usr/local/bin/`, it
> looks for the `depman_thread` binary **in the same directory**. The
> `gcc` command above compiles `depman_thread.c` and places the resulting
> binary in the right location. Without this step, the `-t` option fails
> with error 106.

---

## Usage

```bash
depman [OPTIONS] <project>
```

`<project>` is the path to a directory containing a `deps.conf` file.

### Quick examples

```bash
# Light verification in subshell mode
depman -s projet-light

# Parallel verification in fork mode (installs missing packages)
depman -f projet-medium

# Ultra-fast verification in thread mode (C + pthreads)
depman -t projet-heavy

# Alternative log path (no root required)
depman -l ./my.log -s projet-light

# Restore environment (root required)
sudo depman -r projet-medium

# Compare two snapshots
depman --diff python-test 20260511171214 20260511164240

# Generate deps.conf (delegates to gen-deps)
depman -g mon-projet git curl gcc

# Generate deps.conf from a requirements.txt
depman -gR requirements.txt mon-projet
```

## Options

| Option | Description |
|--------|-------------|
| `-s <project>` | **Subshell mode** — sequential verification in a subshell (variable isolation) |
| `-f <project>` | **Fork mode** — one child process per package, parallel verification |
| `-t <project>` | **Thread mode** — runs `depman_thread` (C + POSIX pthreads) — see [Installation](#installation) |
| `-r <project> [timestamp]` | **Restore** — restores from the latest snapshot or a specific snapshot (**root required**) |
| `-d` / `--diff <project> <ts1> <ts2>` | **Comparison** — shows packages added/removed between two snapshots |
| `-g` / `--gen <project> [packages...]` | **Generation** — creates `deps.conf` by delegating to `gen-deps` |
| `-R <requirements.txt>` | Use with `-g`: imports packages from a `requirements.txt` with auto-resolution (system → python) |
| `-l <directory>` | Alternative directory for the log file (`history.log` will be created there) |
| `-n` / `--dry-run` | **Simulation mode** — shows what would be installed without making any changes |
| `-h` / `--help` | Displays full help |

---

## Error Codes

| Code | Description | Trigger |
|------|-------------|---------|
| 100 | Unknown option | `getopt` does not recognize the option |
| 101 | Missing `<project>` parameter | No positional argument provided |
| 102 | `deps.conf` file not found | File absent from the project directory |
| 103 | Package not found in repositories | The detected package manager cannot find the package |
| 104 | Root privileges required | Option `-r` without `sudo`/root |
| 105 | Snapshot not found | Snapshot file absent for `-r` or `--diff` |
| 106 | `depman_thread.c` not found **or** compilation failed | Binary absent from `/usr/local/bin/` or `gcc` returns a non-zero code — see [Installation](#installation) |
| 107 | `gen-deps` not found | Option `-g` without `gen-deps` present alongside `depman` or in PATH |

Each error displays the full help (`-h`) then exits with the corresponding code.

---

## Logging

All actions are recorded **simultaneously** in the terminal and in the
log file via `tee -a`.

**Default file:** `/var/log/depman/history.log`  
**Format:**

```
yyyy-mm-dd-hh-mm-ss:username:INFOS:message
yyyy-mm-dd-hh-mm-ss:username:ERROR:message
```

**Concrete examples:**

```
2026-04-23-14-32-01:alice:INFOS:Checking git >= 2.30 -> OK (v2.43)
2026-04-23-14-32-02:alice:INFOS:nodejs missing, installing...
2026-04-23-14-32-10:alice:INFOS:nodejs installed successfully (v18.19)
2026-04-23-14-32-15:alice:ERROR:[Error 103] gcc not found in apt repositories
```

---

## Snapshots

A snapshot captures the complete state of installed packages.

- **Automatic creation** at the end of each successful run (`-s`, `-f`, `-t`).
- **Location:** `/var/log/depman/snapshots/<project>_YYYYMMDDHHMMSS.snap`
- **Restore:** `sudo depman -r <project>` (code 105 if no snapshot found)

| Manager | Snapshot command | Restore |
|---------|-----------------|---------|
| `apt` | `dpkg --get-selections` | `dpkg --set-selections` + `apt-get` |
| `pacman` | `pacman -Qqe` | `pacman -S` |
| `dnf/yum/zypper` | `rpm -qa` | Informational only |
| `apk` | `apk info` | Informational only |

> **Naming convention:** the project name is normalized (trailing slash removed,
> internal `/` replaced by `_`) to avoid duplicate separators.

---

## Snapshot Comparison

`--diff` compares two snapshots of the same project and displays packages
added or removed between the two states. No root privileges required.

### Syntax

```bash
depman --diff <project> <ts1> <ts2>
# or short form:
depman -d <project> <ts1> <ts2>
```

- `<project>` — project name (same value as for `-s`/`-f`/`-t`)
- `<ts1>` — timestamp of snapshot **A** (reference) in `YYYYMMDDHHMMSS` format
- `<ts2>` — timestamp of snapshot **B** (target) in `YYYYMMDDHHMMSS` format

List available snapshots with:

```bash
ls /var/log/depman/snapshots/
```

### Example

```bash
depman --diff python-test 20260511171214 20260511164240
```

**Typical output:**

```
═══ Snapshot diff: python-test ═══
  [A] 20260511171214  →  python-test_20260511171214.snap
  [B] 20260511164240  →  python-test_20260511164240.snap

+ Packages present in [B] but absent from [A]:
  + htop
  + nvtop

− Packages present in [A] but absent from [B]:
  − cowsay

Summary: +2 added, −1 removed
```

If both snapshots are identical:

```
✔ Both snapshots are identical.
```

---

## Test Scenarios

### Scenario 1 — Light (subshell)

```bash
mkdir -p projet-light
cp deps.conf projet-light/deps.conf   # or create a deps.conf with 2-3 packages
depman -s projet-light
```

**Expected result:**
- 3 `INFOS` lines in `history.log`
- Return code: `0`
- No installation triggered (packages already present)
- Parent shell variables unchanged

---

### Scenario 2 — Medium (fork)

```bash
mkdir -p projet-medium
cp deps.conf projet-medium/deps.conf  # 5-6 packages, 2-3 potentially missing
depman -f projet-medium
```

**Expected result:**
- 5+ child processes created (PIDs shown in logs)
- Missing packages installed via `apt-get`
- Mixed `INFOS` + `ERROR` logs (code 103 if package not in repositories)
- Snapshot created in `snapshots/`

---

### Scenario 3 — Heavy (thread)

> **Prerequisite:** `depman_thread` must be compiled and installed (see [Installation](#installation)).

```bash
# If depman is installed globally, compile depman_thread first:
sudo gcc -o /usr/local/bin/depman_thread depman_thread.c -lpthread

mkdir -p projet-heavy
cp deps.conf projet-heavy/deps.conf   # 10+ packages
sudo depman -t projet-heavy
```

**Expected result:**
- `depman_thread` found in `/usr/local/bin/` and executed directly
- 10+ simultaneous threads launched by `depman_thread`
- `[X/N]` progress displayed during result processing
- Full environment snapshot
- Thread mode faster than fork on 10+ packages

---

## Architecture

```
depman/
├── depman            # Main Bash script (executable)
├── depman_thread.c   # C program (-t option, parallel verification)
├── deps.conf         # Example configuration
├── gen-deps          # Helper: generates deps.conf from installed packages
├── README.md         # This file
└── /var/log/depman/          # Created automatically
    ├── history.log            # Timestamped log of all actions
    └── snapshots/             # Package state backups
        └── project_YYYYMMDDHHMMSS.snap
```

### Components

| Component | Role | Technology |
|-----------|------|------------|
| `depman` | Main script, entry point | Bash 5.x |
| `depman_thread.c` | Parallel verification | C + POSIX pthreads |
| `deps.conf` | Declarative dependency declaration | Custom `.conf` format |
| `gen-deps` | Generates `deps.conf` from installed packages | Bash 5.x |
| `history.log` | Timestamped log | Structured text |
| `snapshots/` | State backups | apt/pacman/rpm/apk |

### Main Bash Functions

| Function | Role |
|----------|------|
| `detect_pm()` | Detects the package manager (`apt`/`pacman`/`dnf`/`yum`/`zypper`/`apk`) |
| `parse_conf()` | Reads and parses `deps.conf` |
| `check_dep()` | Checks if a package is installed and its version (distro-agnostic) |
| `install_dep()` | Installs a package via the appropriate manager (distro-agnostic) |
| `snapshot()` | Captures the current package state |
| `restore_snapshot()` | Restores from a snapshot |
| `diff_snapshots()` | Compares two snapshots, displays additions/removals |
| `log()` / `log_info()` / `log_error()` | Timestamped logging |
| `handle_error()` | Centralized error handling |
| `show_help()` | Displays documentation |
| `compile_thread_prog()` | Automatic compilation of the C program |
| `run_subshell()` | Mode `-s` |
| `run_fork()` | Mode `-f` |
| `run_thread()` | Mode `-t` |
| `run_restore()` | Mode `-r` |
| `run_gen()` | Mode `-g` — delegates to `gen-deps` |

---

## gen-deps — Generate deps.conf automatically

`gen-deps` is a companion Bash script for `depman`. It **inspects the
packages installed on your machine** and automatically generates a
ready-to-use `deps.conf` file. No network access required — everything
is read from your package manager's local database.

---

### Installation

#### Local usage (from the cloned repository)

```bash
cd depman/
./gen-deps
```

#### Global installation (recommended)

```bash
sudo cp gen-deps /usr/local/bin/gen-deps
sudo chmod +x /usr/local/bin/gen-deps
```

> Once installed globally, replace `./gen-deps` with `gen-deps` in
> all the examples below.

---

### Syntax

```
gen-deps [<project>] [package1 package2 ...]
gen-deps [<project>] -r <requirements.txt> [package1 package2 ...]
```

| Argument / Option | Required | Description |
|-------------------|----------|-------------|
| `<project>` | No | Target directory where `deps.conf` will be created. If omitted, generates `./deps.conf`. |
| `package1 package2 ...` | No | System packages to include. If omitted and no `-r`, the default list is used. |
| `-r <file>` | No | Imports packages from a `requirements.txt`. Combined with explicit packages if provided. |
| `-h` / `--help` | No | Displays help. |

---

### Use Cases

#### 1. No argument — current directory, default packages

```bash
./gen-deps
```

Generates `./deps.conf` with all packages from the default list that are
installed on your system.

**When to use:** Quick start, generic development environment.

---

#### 2. Project name only

```bash
./gen-deps mon-projet
```

Creates the `mon-projet/` directory if it doesn't exist, then generates
`mon-projet/deps.conf` with the default packages.

**When to use:** Quickly initialize the configuration for a new project.

---

#### 3. Project + explicit packages

```bash
./gen-deps mon-projet git curl gcc make python
```

Generates `mon-projet/deps.conf` with **only** the listed packages.
Each version is read from the package manager's local database.

**When to use:** You know exactly your project's dependencies and want a
minimal, precise `deps.conf`.

---

#### 4. Current directory (`.`) + explicit packages

```bash
cd ~/projects/mon-app
gen-deps . git curl python openssl
```

Generates `./deps.conf` in the current directory without creating a subdirectory.

**When to use:** You are already in your project directory.

---

#### 5. From a requirements.txt file

```bash
gen-deps mon-projet -r requirements.txt
```

Reads packages from `requirements.txt`, resolves them automatically:
1. Tries as a system package (e.g. `git` → `git`)
2. If not found, tries with python prefix (e.g. `requests` → `python-requests` / `python3-requests`)
3. If still missing, marked as a comment for manual completion

```bash
# Combine requirements.txt and explicit system packages
gen-deps mon-projet -r requirements.txt git curl

# Directly from depman
depman -gR requirements.txt mon-projet
depman -gR requirements.txt mon-projet git curl
```

**When to use:** Python project with an existing `requirements.txt`; avoids manually duplicating dependencies.

---

#### 6. Package not installed on the system

If a requested package **is not installed**, it appears as a comment:

```ini
# nodejs  (not installed — minimum version to be defined manually)
```

You can manually fill in the required minimum version before sharing the
file with your team.

---

### Default Packages

Used when no packages are provided as arguments:

```
git  curl  wget  python (or python3)  gcc  make  cmake
tar  unzip  rsync  openssl  bash
```

> **Cross-distro detection:** `gen-deps` automatically selects the correct
> Python package name based on the detected manager:
>
> | Manager | Package used |
> |---|---|
> | `pacman` (Arch, Manjaro…) | `python` |
> | `apt`, `dnf`, `yum`, `zypper`, `apk` | `python3` |

---

### Version Retrieval

`gen-deps` queries only the **local** database — no network requests.

| Manager | Command used |
|---|---|
| `apt` | `dpkg -l <pkg>` → version field |
| `pacman` | `pacman -Q <pkg>` → version field |
| `dnf / yum / zypper` | `rpm -q --qf '%{VERSION}' <pkg>` |
| `apk` | `apk info <pkg>` → first line |

---

### Generated File Format

```ini
# Dependency configuration file
# Automatically generated on YYYY-MM-DD HH:MM:SS by gen-deps
# Format: package_name >= minimum_version

[project:<project-name>]
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
# build-essential  (not installed — minimum version to be defined manually)
```

**Format rules (compatible with `depman`'s parser):**

| Element | Rule |
|---------|------|
| Comments | Lines starting with `#` → ignored by `depman` |
| Sections | Lines between `[...]` → ignored by `depman` |
| Empty lines | Ignored by `depman` |
| Valid dependency | `<package> >= <version>` (`>=` operator required) |
| Alignment | Package name padded to 20 characters, then `>= version` |
| Header | Automatic timestamp at generation |

---

### Recommended Workflow

```bash
# 1 — Generate deps.conf from your reference machine
./gen-deps mon-projet git curl python gcc make
# or from a requirements.txt:
./gen-deps mon-projet -r requirements.txt

# 2 — Open and review the generated file
cat mon-projet/deps.conf

# 3 — Simulate installation on another machine (no changes made)
depman -n -s mon-projet

# 4 — Run the actual installation
sudo depman -s mon-projet

# 5 — Check the automatically created snapshot
ls /var/log/depman/snapshots/

# 6 — Compare two snapshots after a change
depman -d mon-projet 20260513192916 20260513192922
```

---
