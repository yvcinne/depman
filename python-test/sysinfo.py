#!/usr/bin/env python3
"""
sysinfo.py — Affiche des informations système simples.
Projet de test pour depman.
"""

import platform
import os
import subprocess
import datetime


def get_os_info():
    return {
        "Système":    platform.system(),
        "Version":    platform.version(),
        "Machine":    platform.machine(),
        "Hostname":   platform.node(),
    }


def get_python_info():
    return {
        "Version Python": platform.python_version(),
        "Implémentation": platform.python_implementation(),
        "Exécutable":     os.sys.executable,
    }


def get_disk_usage():
    stat = os.statvfs("/")
    total  = stat.f_blocks * stat.f_frsize
    free   = stat.f_bfree  * stat.f_frsize
    used   = total - free
    pct    = (used / total) * 100 if total else 0
    return {
        "Total":  f"{total  // (1024**3)} Go",
        "Utilisé": f"{used  // (1024**3)} Go",
        "Libre":  f"{free   // (1024**3)} Go",
        "Usage":  f"{pct:.1f}%",
    }


def get_installed_packages():
    """Retourne quelques paquets clés — compatible Arch (pacman) et Debian (dpkg)."""
    pkgs = ["python3", "git", "curl", "gcc", "make"]
    result = {}

    # Détecter le gestionnaire de paquets disponible
    use_pacman = subprocess.call(
        ["which", "pacman"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    ) == 0

    for pkg in pkgs:
        try:
            if use_pacman:
                # Arch Linux
                out = subprocess.check_output(
                    ["pacman", "-Q", pkg], stderr=subprocess.DEVNULL, text=True
                )
                # Sortie : "git 2.45.1-1"
                parts = out.strip().split()
                result[pkg] = parts[1] if len(parts) > 1 else "?"
            else:
                # Debian / Ubuntu
                out = subprocess.check_output(
                    ["dpkg", "-l", pkg], stderr=subprocess.DEVNULL, text=True
                )
                for line in out.splitlines():
                    if line.startswith("ii"):
                        parts = line.split()
                        result[pkg] = parts[2] if len(parts) > 2 else "?"
                        break
                else:
                    result[pkg] = "non installé"
        except subprocess.CalledProcessError:
            result[pkg] = "non installé"
    return result


def print_section(title, data):
    width = 40
    print(f"\n{'─' * width}")
    print(f"  {title}")
    print(f"{'─' * width}")
    for k, v in data.items():
        print(f"  {k:<20} {v}")


def main():
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"\n{'═' * 40}")
    print(f"  🐍 Rapport Système — {now}")
    print(f"{'═' * 40}")

    print_section("Système d'exploitation", get_os_info())
    print_section("Python", get_python_info())
    print_section("Espace disque (/)", get_disk_usage())
    print_section("Paquets système (via dpkg)", get_installed_packages())

    print(f"\n{'═' * 40}\n")


if __name__ == "__main__":
    main()
