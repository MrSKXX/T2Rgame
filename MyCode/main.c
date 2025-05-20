#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include "../tickettorideapi/ticketToRide.h"
#include "../tickettorideapi/clientAPI.h"
#include "gamestate.h"
#include "player.h"
#include "strategy.h"
#include "rules.h"

#define MAX_TURNS 200
#define DEBUG_LEVEL 0

void debugPrint(int level, const char* format, ...) {
    if (level <= DEBUG_LEVEL) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}

void printGameResult(int finalScore, char* finalResultsMessage) {
    printf("\n==================================================\n");
    printf("              RÉSULTATS FINAUX                    \n");
    printf("==================================================\n");
    
    if (finalResultsMessage && strlen(finalResultsMessage) > 0) {
        printf("%s\n", finalResultsMessage);
    } else {
        printf("Impossible de récupérer les résultats détaillés du serveur.\n");
    }
    
    printf("Notre score calculé localement: %d\n", finalScore);
    printf("==================================================\n\n");
}

bool updateBoardState(GameState* gameState) {
    BoardState boardState;
    ResultCode result = getBoardState(&boardState);
    
    if (result != ALL_GOOD) {
        printf("WARNING: Failed to get board state: 0x%x\n", result);
        return false;
    }
    
    for (int i = 0; i < 5; i++) {
        gameState->visibleCards[i] = boardState.card[i];
    }
    
    return true;
}

bool isGameOver(char* message) {
    if (!message) return false;
    
    return (strstr(message, "winner") != NULL || 
            strstr(message, "Total score") != NULL || 
            strstr(message, "GAME OVER") != NULL ||
            strstr(message, "last turn has ended") != NULL ||
            strstr(message, "Bad protocol") != NULL ||
            strstr(message, "won") != NULL ||
            (strstr(message, "Player") != NULL && strstr(message, "has the longest path") != NULL) ||
            strstr(message, "Final Score") != NULL);
}

int main() {
    const char* serverAddress = "82.29.170.160";
    unsigned int serverPort = 15001;
    const char* playerName = "Georges"; 
    
    printf("=== Ticket to Ride AI Player - %s ===\n", playerName);
    printf("Connecting to server at %s:%d\n", serverAddress, serverPort);
    
    // Connexion au serveur
    ResultCode result = connectToCGS(serverAddress, serverPort, playerName);
    if (result != ALL_GOOD) {
        printf("Failed to connect to server. Error code: 0x%x\n", result);
        return 1;
    }
    printf("Successfully connected to server.\n");

    // Configuration du jeu
    const char* gameSettings = "TRAINING NICE_BOT";
    GameData gameData;

    result = sendGameSettings(gameSettings, &gameData);
    if (result != ALL_GOOD) {
        printf("Failed to send game settings. Error code: 0x%x\n", result);
        return 1;
    }

    printf("Game started: %s, Seed: %d\n", gameData.gameName, gameData.gameSeed);
    printf("Starter: %d\n", gameData.starter);
    
    // Initialisation
    GameState gameState;
    StrategyType strategy = STRATEGY_ADVANCED;
    
    initPlayer(&gameState, strategy, &gameData);
    
    printBoard();
    printGameState(&gameState);
    
    // Variables de contrôle
    int turnCounter = 0;
    bool firstTurn = true;
    int consecutiveErrors = 0;
    char lastErrorMessage[2048] = {0};
    bool gameRunning = true;
    
    // Boucle principale - simplifiée et sans gestion spéciale de fin de partie
    while (gameRunning && turnCounter < MAX_TURNS) {
        turnCounter++;
        
        // Obtenir l'état actuel du jeu
        MoveData dummyMove;
        MoveResult currentResult = {0};
        ResultCode currentCode = getMove(&dummyMove, &currentResult);
        
        // Détecter la fin de partie
        if (currentResult.message && isGameOver(currentResult.message)) {
            printf("Fin de partie détectée: %s\n", currentResult.message);
            strncpy(lastErrorMessage, currentResult.message, sizeof(lastErrorMessage)-1);
            cleanupMoveResult(&currentResult);
            break;
        }
        
        // C'est notre tour ou pas ?
        bool itsOurTurn = false;
        
        // Si getMove a réussi normalement, c'est le tour de l'adversaire
        if (currentCode == ALL_GOOD) {
            printf("\n\n=== TOUR %d (Adversaire) ===\n", turnCounter);
            updateAfterOpponentMove(&gameState, &dummyMove);
            
            // Suivre l'état du jeu, mais sans traitement spécial pour le dernier tour
            if (gameState.opponentWagonsLeft <= 2) {
                printf("INFO: L'adversaire a <= 2 wagons!\n");
                gameState.lastTurn = 1;
            }
            
            // Le prochain tour sera le nôtre si l'adversaire ne rejoue pas
            if (!currentResult.replay) {
                itsOurTurn = true;
            }
        }
        // Si on a une erreur "It's our turn", c'est notre tour
        else if (currentCode == SERVER_ERROR && currentResult.message && 
                 strstr(currentResult.message, "It's our turn")) {
            itsOurTurn = true;
        }
        
        cleanupMoveResult(&currentResult);
        
        // Jouer notre tour si c'est à nous
        if (itsOurTurn) {
            printf("\n\n=== TOUR %d (Notre tour) ===\n", turnCounter);
            
            // Mettre à jour l'état du jeu
            updateBoardState(&gameState);
            printBoard();
            
            printf("Wagons - Nous: %d, Adversaire: %d\n", gameState.wagonsLeft, gameState.opponentWagonsLeft);
            
            // Jouer notre tour normalement, sans traitement spécial pour le dernier tour
            ResultCode playCode;
            
            if (firstTurn) {
                playCode = playFirstTurn(&gameState);
                if (playCode == ALL_GOOD) {
                    firstTurn = false;
                }
            } else {
                playCode = playTurn(&gameState, strategy);
            }
            
            if (playCode == ALL_GOOD) {
                printf("Tour joué avec succès\n");
                consecutiveErrors = 0;
                
                // Suivre l'état du jeu normalement
                if (gameState.wagonsLeft <= 2) {
                    printf("INFO: Nous avons <= 2 wagons!\n");
                    gameState.lastTurn = 1;
                }
            } 
            else {
                printf("Erreur lors de notre tour: 0x%x\n", playCode);
                
                // Vérifier si c'est une erreur de fin de partie
                MoveResult errorResult = {0};
                getMove(&dummyMove, &errorResult);
                
                if (errorResult.message) {
                    printf("Message d'erreur: %s\n", errorResult.message);
                    
                    if (isGameOver(errorResult.message)) {
                        strncpy(lastErrorMessage, errorResult.message, sizeof(lastErrorMessage)-1);
                        cleanupMoveResult(&errorResult);
                        break;
                    }
                    
                    strncpy(lastErrorMessage, errorResult.message, sizeof(lastErrorMessage)-1);
                }
                
                cleanupMoveResult(&errorResult);
                
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("Trop d'erreurs consécutives, arrêt du jeu\n");
                    gameRunning = false;
                }
            }
            
            // Afficher l'état du jeu
            printGameState(&gameState);
            int currentScore = calculateScore(&gameState);
            printf("Score actuel: %d\n", currentScore);
        }
        
    }
    
    // Fin de partie - récupération et affichage des résultats
    printf("\n===== FIN DE PARTIE =====\n");
    int finalScore = calculateScore(&gameState);
    
    // Afficher le plateau final
    printf("\n\n===== PLATEAU FINAL =====\n");
    printBoard();
    
    // Compter les objectifs complétés
    int completedObjectives = 0;
    for (int i = 0; i < gameState.nbObjectives; i++) {
        if (isObjectiveCompleted(&gameState, gameState.objectives[i])) {
            completedObjectives++;
        }
    }
    
    // Préparer le message final
    char finalResultsMessage[2048] = {0};
    
    // Utiliser le dernier message d'erreur s'il contient des informations de fin de partie
    if (strlen(lastErrorMessage) > 0 && 
        (strstr(lastErrorMessage, "score") || 
         strstr(lastErrorMessage, "winner") || 
         strstr(lastErrorMessage, "won") || 
         strstr(lastErrorMessage, "Total"))) {
        
        strncpy(finalResultsMessage, lastErrorMessage, sizeof(finalResultsMessage)-1);
    }
    else {
        // Créer un résultat minimal
        snprintf(finalResultsMessage, sizeof(finalResultsMessage),
                "Résultats extraits du plateau:\n"
                "Notre score final (calculé): %d\n"
                "Nombre de wagons restants: %d\n"
                "Nombre d'objectifs complétés: %d/%d\n",
                finalScore, gameState.wagonsLeft, 
                completedObjectives, gameState.nbObjectives);
    }
    
    printGameResult(finalScore, finalResultsMessage);
    
    if (gameData.gameName) free(gameData.gameName);
    if (gameData.trackData) free(gameData.trackData);
    
    quitGame();
    printf("Game terminated.\n");
    
    return 0;
}