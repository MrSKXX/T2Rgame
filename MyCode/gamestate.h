/**
 * gamestate.h
 * Structure pour suivre l'état du jeu
 */
#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "../tickettorideapi/ticketToRide.h"

#define MAX_CARDS 100       // Nombre maximum de cartes dans la main du joueur
#define MAX_OBJECTIVES 15   // Nombre maximum d'objectifs que le joueur peut avoir
#define MAX_ROUTES 150      // Nombre maximum de routes sur le plateau
#define MAX_CITIES 50       // Nombre maximum de villes


// Structure pour représenter une route
typedef struct {
    int from;               // Ville de départ
    int to;                 // Ville d'arrivée
    int length;             // Longueur de la route
    CardColor color;        // Couleur de la route
    CardColor secondColor;  // Seconde couleur (pour les routes doubles)
    int owner;              // Propriétaire de la route (0 = personne, 1 = nous, 2 = adversaire)
} Route;

// Structure pour suivre l'état du jeu
typedef struct {
    // Infos sur le plateau
    int nbCities;
    int nbTracks;
    Route routes[MAX_ROUTES];
    
    // Nos cartes en main
    CardColor cards[MAX_CARDS];
    int nbCards;
    int nbCardsByColor[10]; // Nombre de cartes par couleur (index = couleur)
    
    // Nos objectifs
    Objective objectives[MAX_OBJECTIVES];
    int nbObjectives;
    
    // Cartes visibles sur le plateau
    CardColor visibleCards[5];
    
    // Routes que nous avons prises
    int claimedRoutes[MAX_ROUTES];
    int nbClaimedRoutes;
    
    // Connectivité des villes pour le joueur
    int cityConnected[MAX_CITIES][MAX_CITIES]; // 1 si les villes sont connectées par nos routes
    
    // État du jeu
    int lastTurn;           // 1 si c'est le dernier tour, 0 sinon
    int wagonsLeft;         // Nombre de wagons restants
    
    // Info sur l'adversaire
    int opponentWagonsLeft; // Nombre de wagons restants à l'adversaire
    int opponentCardCount;  // Nombre de cartes de l'adversaire
    int opponentObjectiveCount; // Nombre d'objectifs de l'adversaire
    
    // Compteur de tours
    int turnCount;          // Nombre de tours joués
} GameState;


void updateOpponentObjectiveModel(GameState* state, int from, int to);

/**
 * Initialise l'état du jeu à partir des données initiales
 */
void initGameState(GameState* state, GameData* gameData);

/**
 * Met à jour l'état du jeu après avoir reçu une carte
 */
void addCardToHand(GameState* state, CardColor card);

/**
 * Met à jour l'état du jeu après avoir joué des cartes pour prendre une route
 */
void removeCardsForRoute(GameState* state, CardColor color, int length, int nbLocomotives);

/**
 * Met à jour l'état du jeu après avoir pris une route
 */
void addClaimedRoute(GameState* state, int from, int to);

/**
 * Met à jour l'état du jeu après une action de l'adversaire
 */
void updateAfterOpponentMove(GameState* state, MoveData* moveData);

/**
 * Met à jour la matrice de connectivité des villes
 */
void updateCityConnectivity(GameState* state);

/**
 * Ajoute des objectifs à notre main
 */
void addObjectives(GameState* state, Objective* objectives, int count);

/**
 * Affiche l'état du jeu actuel
 */
void printGameState(GameState* state);


void printConnectivityMatrix(GameState* state);

/**
 * Analyse le réseau de routes existant
 * @param state État actuel du jeu
 * @param cityConnectivity Tableau à remplir avec le nombre de connexions par ville
 */
void analyzeExistingNetwork(GameState* state, int* cityConnectivity);

/**
 * Structure pour stocker les informations sur les connexions manquantes
 */
typedef struct {
    int city;
    int connectionsNeeded;
    int priority;
} MissingConnection;

/**
 * Trouve les connexions manquantes entre les hubs et les objectifs
 * @param state État actuel du jeu
 * @param cityConnectivity Tableau de connexions par ville
 * @param missingConnections Tableau à remplir avec les connexions manquantes
 * @param count Nombre de connexions trouvées
 */
void findMissingConnections(GameState* state, int* cityConnectivity, MissingConnection* missingConnections, int* count);

/**
 * Trouve le chemin le plus intelligent (pas forcément le plus court) entre deux villes
 * @param state État actuel du jeu
 * @param start Ville de départ
 * @param end Ville d'arrivée
 * @param path Tableau à remplir avec le chemin
 * @param pathLength Longueur du chemin
 * @return Distance du chemin, -1 si aucun chemin n'est trouvé
 */
int findSmartestPath(GameState* state, int start, int end, int* path, int* pathLength);
#endif // GAMESTATE_H