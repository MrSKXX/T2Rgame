# Ticket to Ride AI Bot

## Description
Ce projet implémente une intelligence artificielle pour jouer au jeu Ticket to Ride. Le bot utilise des stratégies avancées de pathfinding et d'optimisation pour maximiser son score contre d'autres joueurs.

## Architecture du Projet

### Structure des Fichiers
```
MyCode/
├── main.c              # Point d'entrée principal et gestion des parties
├── gamestate.c/.h      # Gestion de l'état du jeu
├── player.c/.h         # Interface joueur et logique de tours
├── rules.c/.h          # Règles du jeu et validation des coups
├── strategy.c/.h      # Stratégies d'IA et prise de décision
├── Makefile           # Configuration de compilation
└── README.md          # Ce fichier
```

### Modules Principaux

#### 1. **GameState** (`gamestate.c/.h`)
- **Fonction** : Maintient l'état complet du jeu
- **Fonctionnalités** :
  - Tracking des cartes, routes, et objectifs
  - Matrice de connectivité des villes (Floyd-Warshall)
  - Gestion des mouvements adverses
  - Calcul automatique des connexions réseau

#### 2. **Player** (`player.c/.h`)
- **Fonction** : Interface entre l'IA et le serveur de jeu
- **Fonctionnalités** :
  - Gestion des tours (premier tour vs tours normaux)
  - Communication avec l'API du serveur
  - Validation des mouvements avant envoi
  - Détection automatique de fin de partie

#### 3. **Rules** (`rules.c/.h`)
- **Fonction** : Implémentation des règles du jeu
- **Fonctionnalités** :
  - Validation des routes (cartes suffisantes, wagons, etc.)
  - Calcul des scores (routes + objectifs)
  - Détection des objectifs complétés
  - Recherche des coups possibles

#### 4. **Strategy** (`strategy_simple.c/.h`)
- **Fonction** : Cœur de l'intelligence artificielle
- **Fonctionnalités** :
  - Pathfinding intelligent avec priorité aux routes possédées
  - Évitement proactif des zones difficiles (côte Est)
  - Stratégies adaptatives selon le contexte de jeu
  - Gestion des situations de blocage

## Stratégies Implémentées

### Sélection d'Objectifs Intelligente
- **Évitement géographique** : Pénalité -70% pour les objectifs côte Est
- **Bonus réseau** : +100% pour objectifs utilisant routes existantes
- **Analyse de faisabilité** : Évaluation du nombre de routes nécessaires
- **Sélection adaptive** : Choix entre 1-2 objectifs selon leur qualité

### Pathfinding Avancé
```c
// Algorithme de Dijkstra modifié
// Coût = 0 pour nos routes, longueur normale pour routes libres, ∞ pour routes adverses
int findSmartestPath(GameState* state, int start, int end, int* path, int* pathLength)
```

### Stratégies Contextuelles

#### Mode Normal
1. **Analyse globale** : Évaluation de tous les objectifs simultanément
2. **Priorisation** : Routes utiles pour plusieurs objectifs
3. **Optimisation** : Réutilisation maximale du réseau existant

#### Mode Fin de Partie
- **Complétion immédiate** : Focus sur objectifs à 1-2 routes
- **Routes haute valeur** : Privilégier routes longues (5-6 wagons)
- **Abandon sélectif** : Ignorer objectifs impossibles

#### Mode Urgence
- **Déblocage forcé** : Si >40 cartes accumulées
- **Prise opportuniste** : N'importe quelle route profitable
- **Stratégies alternatives** : Chemins de contournement

## Utilisation

### Compilation
```bash
make clean
make
```

### Exécution
```bash
./tickettoridebot
```

### Configuration
- **Serveur** : `82.29.170.160:15001`
- **Mode** : `TRAINING NICE_BOT`
- **Parties** : 3 parties consécutives (configurable dans `main.c`)

## Fonctionnalités Avancées

### Analyse Réseau
- Matrice de connectivité mise à jour en temps réel
- Détection automatique des composantes connexes
- Optimisation des extensions de réseau

### Gestion Multi-Parties
- Session automatique de 3 parties
- Statistiques détaillées par partie
- Calculs de moyennes et taux de réussite

### Optimisations Performance
- Limitation des routes analysées (MAX_ROUTES_TO_PROCESS = 50)
- Cache des calculs de chemins
- Validation précoce des mouvements impossibles

### Robustesse
- Gestion d'erreurs complète
- Validation de tous les paramètres
- Récupération automatique des situations critiques
- Détection multiple des fins de partie

## Algorithmes Clés

### Sélection d'Objectifs
```
Score Final = (Points / Distance) × Bonus Réseau × Pénalité Géographique × Facteur Difficulté
```

### Évaluation de Routes
```
Priorité = Valeur Objectifs × Multiplicateur Multi-Usage + Bonus Longueur
```

### Pathfinding Intelligent
- **Coût routes possédées** : 0
- **Coût routes libres** : Longueur réelle
- **Coût routes adverses** : ∞ (évitement total)

## Résultats Attendus
- **Score moyen** : 80-120 points par partie
- **Taux complétion objectifs** : 60-80%
- **Performance** : Victoires régulières contre NICE_BOT

## Points Forts de l'IA
1. **Adaptabilité** : Stratégies différentes selon contexte
2. **Efficacité** : Réutilisation maximale des investissements
3. **Prévoyance** : Évitement proactif des pièges géographiques
4. **Robustesse** : Gestion d'erreurs et récupération automatique

