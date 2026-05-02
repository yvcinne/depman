# python-test — Projet de test pour depman

Un projet Python minimal qui affiche un rapport système.  
Son seul but est de démontrer le workflow complet de `depman`.

## Utilisation

### 1. Vérifier les dépendances avec depman

```bash
# Depuis le dossier depman/
./depman -s python-test
```

### 2. Lancer le projet

```bash
python3 python-test/sysinfo.py
```

## Structure

```
python-test/
├── deps.conf     # dépendances système déclarées
├── sysinfo.py    # script Python principal
└── README.md
```

## deps.conf

```ini
[project:python-test]
python3    >= 3.8
python3-pip >= 22.0
curl       >= 7.0
git        >= 2.30
```
