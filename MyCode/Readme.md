# Ticket to Ride AI Bot

Bot d'intelligence artificielle pour le jeu Ticket to Ride utilisant des stratégies de pathfinding et d'optimisation.

## Structure

```
MyCode/
├── main.c              # Point d'entrée et gestion des parties
├── gamestate.c/.h      # Gestion de l'état du jeu
├── player.c/.h         # Interface joueur
├── rules.c/.h          # Règles et validation
├── strategy.c/.h       # Stratégies d'IA
└── Makefile           # Compilation
```

## Modules

### GameState
Maintient l'état complet du jeu : cartes, routes, objectifs, matrice de connectivité.

### Player
Interface entre l'IA et le serveur de jeu, gestion des tours.

### Rules
Validation des routes, calcul des scores, détection des objectifs complétés.

### Strategy
Pathfinding intelligent, sélection d'objectifs, stratégies adaptatives.

## Stratégies Principales

- **Sélection d'objectifs** : Évitement côte Est (-70%), bonus réseau (+100%)
- **Pathfinding** : Dijkstra modifié (coût 0 pour nos routes)
- **Modes adaptatifs** : Normal, fin de partie, urgence

## Utilisation

```bash
make clean && make
./tickettoridebot
```

Configuration : serveur `82.29.170.160:15001`, mode `TRAINING NICE_BOT`, 3 parties.