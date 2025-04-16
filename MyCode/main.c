#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // Pour sleep()
#include <stdbool.h>
#include <stdarg.h>   // Pour va_list
#include "../tickettorideapi/ticketToRide.h"
#include "../tickettorideapi/clientAPI.h"
#include "gamestate.h"
#include "player.h"
#include "strategy.h"
#include "rules.h"
#include "manual.h"

// Définition du mode de jeu (0 = IA, 1 = manuel)
#define MANUAL_MODE 0

// Niveau de débogage
// 0 = aucun message de débogage
// 1 = messages importants seulement
// 2 = messages détaillés
// 3 = tous les messages (très verbeux)
#define DEBUG_LEVEL 0

// Fonction utilitaire pour afficher les messages de débogage selon le niveau
void debugPrint(int level, const char* format, ...) {
    if (level <= DEBUG_LEVEL) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}

// Vérifie si le jeu est terminé en analysant les messages d'erreur
bool isGameOver(char* message) {
    if (!message) return false;
    return (strstr(message, "winner") != NULL || 
            strstr(message, "Total score") != NULL || 
            strstr(message, "GAME OVER") != NULL ||
            (strstr(message, "Player") != NULL && strstr(message, "has the longest path") != NULL));
}

// Extrait et affiche le score à partir d'un message du serveur
void printGameResult(char* message) {
    if (!message) {
        printf("\n==================================================\n");
        printf("              PARTIE TERMINÉE                    \n");
        printf("==================================================\n");
        printf("Aucun message de résultat disponible.\n");
        printf("==================================================\n\n");
        return;
    }
    
    printf("\n==================================================\n");
    printf("              RÉSULTAT DE LA PARTIE               \n");
    printf("==================================================\n");
    
    // Extraire et afficher les informations
    // Afficher les objectifs complétés
    char* objectiveStart = message;
    char* nextLine;
    
    // Parcourir et afficher ligne par ligne
    while ((nextLine = strchr(objectiveStart, '\n')) != NULL) {
        int lineLength = nextLine - objectiveStart;
        char line[512] = {0};
        strncpy(line, objectiveStart, lineLength);
        
        // Formater la ligne pour l'affichage
        if (strstr(line, "Objective") || 
            strstr(line, "Total score") || 
            strstr(line, "has the longest path")) {
            printf("%s\n", line);
        }
        
        objectiveStart = nextLine + 1;
    }
    
    printf("==================================================\n\n");
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

    const char* gameSettings = "TRAINING NICE_BOT";
    GameData gameData;

    // Envoi des paramètres de jeu
    result = sendGameSettings(gameSettings, &gameData);
    if (result != ALL_GOOD) {
        printf("Failed to send game settings. Error code: 0x%x\n", result);
        return 1;
    }

    printf("Game started: %s, Seed: %d\n", gameData.gameName, gameData.gameSeed);
    printf("Starter: %d\n", gameData.starter);
    
    // Initialiser notre état de jeu
    GameState gameState;
    StrategyType strategy = STRATEGY_BASIC;
    
    initPlayer(&gameState, strategy, &gameData);
    
    // Afficher l'état initial
    printBoard();
    printGameState(&gameState);
    
    // Variables pour le jeu
    int turnCounter = 0;
    bool gameRunning = true;
    bool firstTurn = true;
    char* finalGameMessage = NULL;
    
    // D'après les tests, il semble que:
    // - Si starter == 0, c'est à nous de commencer
    // - Si starter == 1, c'est à l'adversaire de commencer
    // Nous initialisons ourTurn en conséquence:
    bool ourTurn = (gameData.starter != 1);
    
    printf("Nous %s en premier\n", ourTurn ? "jouons" : "ne jouons pas");
    
    // Boucle principale du jeu
    while (gameRunning) {
        turnCounter++;
        printf("\n\n=== TOUR %d ===\n", turnCounter);
        printf("C'est %s tour\n", ourTurn ? "notre" : "le tour de l'adversaire");
        
        // Vérification des conditions de fin de partie
        if (gameState.opponentWagonsLeft <= 0) {
            printf("L'adversaire n'a plus de wagons. C'est le dernier tour!\n");
            gameState.lastTurn = 1;
        }
        
        if (gameState.wagonsLeft <= 0) {
            printf("Nous n'avons plus de wagons. C'est le dernier tour!\n");
            gameState.lastTurn = 1;
        }
        
        // Mettre à jour l'affichage du plateau
        ResultCode boardResult = printBoard();
        if (boardResult != ALL_GOOD) {
            if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                printf("Fin de partie détectée lors de l'affichage du plateau.\n");
                gameRunning = false;
                break;
            }
        }
        
        // Mettre à jour les cartes visibles
        BoardState boardState;
        ResultCode stateResult = getBoardState(&boardState);
        if (stateResult == ALL_GOOD) {
            for (int i = 0; i < 5; i++) {
                gameState.visibleCards[i] = boardState.card[i];
            }
        } else if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
            printf("Fin de partie détectée lors de la récupération des cartes visibles.\n");
            gameRunning = false;
            break;
        }
        
        printf("Wagons - Nous: %d, Adversaire: %d\n", gameState.wagonsLeft, gameState.opponentWagonsLeft);
        
        if (ourTurn) {
            // Notre tour de jouer
            printf("\n--- Playing our turn ---\n");
            
            ResultCode playCode;
            MoveResult moveResult = {0};
            
            // Jouer selon le mode (manuel ou IA)
            if (MANUAL_MODE) {
                if (firstTurn) {
                    playCode = playManualFirstTurn(&gameState);
                } else {
                    playCode = playManualTurn(&gameState);
                }
            } else {
                if (firstTurn) {
                    playCode = playFirstTurn(&gameState);
                    if (playCode == ALL_GOOD) {
                        firstTurn = false;
                    }
                } else {
                    playCode = playTurn(&gameState, strategy);
                }
            }
            
            if (playCode == ALL_GOOD) {
                printf("Successfully played our turn\n");
                
                // Vérifier si c'est le dernier tour
                if (isLastTurn(&gameState)) {
                    printf("LAST TURN: We have <= 2 wagons left\n");
                    gameState.lastTurn = 1;
                }
                
                // Si on ne rejoue pas, c'est au tour de l'adversaire
                if (moveResult.replay) {
                    printf("We get to play again!\n");
                } else {
                    ourTurn = false;
                }
            } 
            else if (playCode == SERVER_ERROR) {
                // Le serveur a renvoyé une erreur - vérifier si c'est la fin de partie
                if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                    printf("Fin de partie détectée.\n");
                    gameRunning = false;
                    break;
                }
                
                // Si nous avons un message final, c'est probablement la fin de partie
                if (moveResult.message && isGameOver(moveResult.message)) {
                    printf("Fin de partie détectée via le message du serveur.\n");
                    if (finalGameMessage) free(finalGameMessage);
                    finalGameMessage = strdup(moveResult.message);
                    gameRunning = false;
                    cleanupMoveResult(&moveResult);
                    break;
                }
                
                // Autre erreur serveur
                printf("Server error - Game might be over\n");
                gameRunning = false;
                if (moveResult.message) printf("Message: %s\n", moveResult.message);
                cleanupMoveResult(&moveResult);
                continue;
            }
            else if (playCode == PARAM_ERROR || playCode == OTHER_ERROR) {
                // Erreur - Si fin de partie
                if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                    printf("Fin de partie détectée. Dernière erreur: 0x%x\n", playCode);
                    gameRunning = false;
                    break;
                }
                
                // Vérifier si c'est un message de fin de partie
                if (moveResult.message && isGameOver(moveResult.message)) {
                    printf("Fin de partie détectée via le message d'erreur.\n");
                    if (finalGameMessage) free(finalGameMessage);
                    finalGameMessage = strdup(moveResult.message);
                    gameRunning = false;
                    cleanupMoveResult(&moveResult);
                    break;
                }
                
                // Peut-être que c'est le tour de l'adversaire?
                printf("Error playing turn: 0x%x - Trying opponent's turn instead\n", playCode);
                ourTurn = false;
                cleanupMoveResult(&moveResult);
                // Continuer vers le début de la boucle pour essayer l'autre tour
                continue;
            }
            else {
                // Autre erreur
                printf("Unknown error playing turn: 0x%x\n", playCode);
                if (moveResult.message) printf("Message: %s\n", moveResult.message);
                cleanupMoveResult(&moveResult);
                sleep(2);  // Pause plus longue
            }
        } 
        else {
            // C'est le tour de l'adversaire
            printf("\n--- Waiting for opponent's move ---\n");
            
            MoveData opponentMove;
            MoveResult opponentMoveResult;
            ResultCode moveCode = getMove(&opponentMove, &opponentMoveResult);
            
            if (moveCode == ALL_GOOD) {
                // Vérifier si c'est la fin de partie
                if (opponentMoveResult.message && isGameOver(opponentMoveResult.message)) {
                    printf("Fin de partie détectée dans le message de l'adversaire!\n");
                    // Sauvegarder le message pour l'afficher à la fin
                    if (finalGameMessage) free(finalGameMessage);
                    finalGameMessage = strdup(opponentMoveResult.message);
                    
                    gameRunning = false;
                    // Libérer la mémoire
                    cleanupMoveResult(&opponentMoveResult);
                    break;
                }
                
                // L'adversaire a bien joué
                printf("Opponent made a move of type: %d\n", opponentMove.action);
                updateAfterOpponentMove(&gameState, &opponentMove);
                
                // Si l'adversaire ne rejoue pas, c'est notre tour
                if (!opponentMoveResult.replay) {
                    ourTurn = true;
                    printf("Now it's our turn\n");
                } else {
                    printf("Opponent continues playing...\n");
                }
                
                // Nettoyage de la mémoire
                cleanupMoveResult(&opponentMoveResult);
            }
            else if (moveCode == SERVER_ERROR) {
                // Vérifier si c'est la fin de partie
                if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                    printf("Fin de partie détectée par erreur serveur.\n");
                    gameRunning = false;
                    
                    // Vérifier si nous avons un message final
                    if (opponentMoveResult.message && isGameOver(opponentMoveResult.message)) {
                        if (finalGameMessage) free(finalGameMessage);
                        finalGameMessage = strdup(opponentMoveResult.message);
                    }
                    
                    cleanupMoveResult(&opponentMoveResult);
                    break;
                }
                
                // Autre erreur serveur
                printf("Server error - Game might be over\n");
                if (opponentMoveResult.message) printf("Message: %s\n", opponentMoveResult.message);
                cleanupMoveResult(&opponentMoveResult);
                gameRunning = false;
                continue;
            }
            else if (moveCode == PARAM_ERROR || moveCode == OTHER_ERROR) {
                // Erreur - Si fin de partie
                if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                    printf("Fin de partie détectée. Dernière erreur: 0x%x\n", moveCode);
                    gameRunning = false;
                    
                    // Vérifier si nous avons un message final
                    if (opponentMoveResult.message && isGameOver(opponentMoveResult.message)) {
                        if (finalGameMessage) free(finalGameMessage);
                        finalGameMessage = strdup(opponentMoveResult.message);
                    }
                    
                    cleanupMoveResult(&opponentMoveResult);
                    break;
                }
                
                // Peut-être que c'est notre tour?
                printf("Error getting opponent move: 0x%x - Trying our turn instead\n", moveCode);
                ourTurn = true;
                cleanupMoveResult(&opponentMoveResult);
                // Continuer vers le début de la boucle pour essayer l'autre tour
                continue;
            }
            else {
                // Autre erreur
                printf("Unknown error getting opponent move: 0x%x\n", moveCode);
                if (opponentMoveResult.message) printf("Message: %s\n", opponentMoveResult.message);
                cleanupMoveResult(&opponentMoveResult);
                sleep(2);  // Pause plus longue
            }
        }
        
        // Afficher l'état mis à jour
        if (MANUAL_MODE) {
            printManualGameState(&gameState);
        } else {
            printGameState(&gameState);
        }
        
        // Calculer le score actuel
        int currentScore = calculateScore(&gameState);
        printf("Current score: %d\n", currentScore);
        
        // Vérifier si le dernier tour est terminé
        // Vérifier si le dernier tour est terminé
        if (gameState.lastTurn && !ourTurn) {
            printf("Dernier tour terminé. Fin de la partie.\n");
            gameRunning = false;
            
            // Essayer d'obtenir un dernier message du serveur, mais ignorer l'erreur si elle se produit
            if (!finalGameMessage) {
                // Tentative silencieuse - nous ne nous soucions pas de l'erreur ici
                // Nous désactivons temporairement les messages d'erreur pour ne pas afficher
                // "It's not our turn to play" qui est attendu
                MoveData dummyMove;
                MoveResult lastResult = {0};
                dummyMove.action = DRAW_BLIND_CARD;
                
                // Utiliser getBoardState() qui est moins susceptible de générer une erreur
                BoardState lastBoardState;
                if (getBoardState(&lastBoardState) == ALL_GOOD) {
                    // Le serveur répond encore, nous pouvons tenter d'obtenir un message final
                    cleanupMoveResult(&lastResult);
                }
            }
            
            break;
        }
        
        // Attendre un peu avant le prochain tour
        sleep(1);
    }
    
    // Afficher le résultat final si disponible
    if (finalGameMessage) {
        printGameResult(finalGameMessage);
        free(finalGameMessage);
    } else {
        // Si pas de message final, afficher juste un message de fin
        printGameResult(NULL);
    }
    
    // Nettoyage final
    if (gameData.gameName) free(gameData.gameName);
    if (gameData.trackData) free(gameData.trackData);
    
    quitGame();
    printf("Game terminated.\n");
    
    return 0;
}