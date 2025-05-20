#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // Pour usleep()
#include <stdbool.h>
#include <stdarg.h>   // Pour va_list
#include "../tickettorideapi/ticketToRide.h"
#include "../tickettorideapi/clientAPI.h"
#include "gamestate.h"
#include "player.h"
#include "strategy.h"
#include "rules.h"

// Nombre maximum de tours avant d'arrêter automatiquement
#define MAX_TURNS 200

// Niveau de débogage
#define DEBUG_LEVEL 0


// Version améliorée pour afficher les résultats finaux
void printGameResult(int finalScore, char* finalResultsMessage) {
    printf("\n==================================================\n");
    printf("              RÉSULTATS FINAUX                    \n");
    printf("==================================================\n");
    
    if (finalResultsMessage && strlen(finalResultsMessage) > 0) {
        printf("%s\n", finalResultsMessage);
    } else {
        printf("Impossible de récupérer les résultats détaillés du serveur.\n");
    }
    
    // Affichage du score calculé localement
    printf("Notre score calculé localement: %d\n", finalScore);
    printf("==================================================\n\n");
}

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

// Update board state and visible cards
bool updateBoardState(GameState* gameState) {
    BoardState boardState;
    ResultCode result = getBoardState(&boardState);
    
    if (result != ALL_GOOD) {
        printf("WARNING: Failed to get board state: 0x%x\n", result);
        return false;
    }
    
    // Update visible cards
    for (int i = 0; i < 5; i++) {
        gameState->visibleCards[i] = boardState.card[i];
    }
    
    return true;
}

// Vérifie si le jeu est terminé en analysant les messages d'erreur
bool isGameOver(char* message) {
    if (!message) return false;
    
    // Recherche des indications de fin de partie dans le message
    return (strstr(message, "winner") != NULL || 
            strstr(message, "Total score") != NULL || 
            strstr(message, "GAME OVER") != NULL ||
            strstr(message, "last turn has ended") != NULL ||
            strstr(message, "Bad protocol") != NULL ||
            (strstr(message, "Player") != NULL && strstr(message, "has the longest path") != NULL) ||
            strstr(message, "Final Score") != NULL);
}

// Fonction pour capturer les résultats finaux de façon agressive
// Fonction pour capturer les résultats finaux de façon agressive
// Fonction pour capturer les résultats finaux de façon agressive
// Fonction pour capturer les résultats finaux de façon agressive
// Fonction pour capturer les résultats finaux de façon agressive
// Fonction pour capturer les résultats finaux de façon agressive
// Fonction pour capturer les résultats finaux de façon agressive
bool captureEndGameResults(char* finalResultsMessage, size_t messageSize) {
    bool resultsFound = false;
    MoveResult moveResult = {0};
    
    // Afficher le plateau final plusieurs fois pour s'assurer qu'il est mis à jour
    for (int i = 0; i < 3; i++) {
        printBoard();
        usleep(200000);
    }
    
    // Essayer d'abord getMove pour voir si on obtient les résultats
    printf("Tentative initiale avec getMove...\n");
    MoveData dummyMove;
    moveResult.message = NULL;
    ResultCode getCode = getMove(&dummyMove, &moveResult);
    
    // Si getMove réussit et contient les objectifs, c'est parfait
    if (getCode == ALL_GOOD && moveResult.message && 
        strstr(moveResult.message, "Objective") != NULL) {
        
        printf("Résultats obtenus via getMove!\n");
        strncpy(finalResultsMessage, moveResult.message, messageSize-1);
        resultsFound = true;
    }
    // Si getMove indique que c'est notre tour, essayer de jouer un coup
    else if (moveResult.message && strstr(moveResult.message, "It's our turn")) {
        printf("Le serveur indique que c'est notre tour, tentative avec sendMove...\n");
        
        // Libérer la mémoire du message d'erreur
        if (moveResult.message) free(moveResult.message);
        if (moveResult.opponentMessage) free(moveResult.opponentMessage);
        
        // Essayer plusieurs types de coups
        MoveData moves[4];
        
        // Préparer différentes actions à essayer
        moves[0].action = DRAW_BLIND_CARD;
        
        moves[1].action = DRAW_CARD;
        moves[1].drawCard = 1;
        
        moves[2].action = DRAW_OBJECTIVES;
        
        moves[3].action = CLAIM_ROUTE;
        moves[3].claimRoute.from = 0;
        moves[3].claimRoute.to = 1;
        moves[3].claimRoute.color = 1;
        moves[3].claimRoute.nbLocomotives = 0;
        
        for (int i = 0; i < 4; i++) {
            printf("Tentative sendMove avec action %d...\n", i);
            moveResult.message = NULL;
            
            ResultCode sendCode = sendMove(&moves[i], &moveResult);
            
            // Si le coup réussit et contient les résultats
            if (sendCode == ALL_GOOD && moveResult.message) {
                if (strstr(moveResult.message, "Objective") != NULL ||
                    strstr(moveResult.message, "score") != NULL ||
                    strstr(moveResult.message, "Score") != NULL ||
                    strstr(moveResult.message, "winner") != NULL ||
                    strstr(moveResult.message, "longest path") != NULL ||
                    strstr(moveResult.message, "Total score") != NULL) {
                    
                    printf("Résultats obtenus via sendMove (action %d)!\n", i);
                    strncpy(finalResultsMessage, moveResult.message, messageSize-1);
                    resultsFound = true;
                    
                    if (moveResult.message) free(moveResult.message);
                    if (moveResult.opponentMessage) free(moveResult.opponentMessage);
                    
                    break;
                }
            }
            
            if (moveResult.message) free(moveResult.message);
            if (moveResult.opponentMessage) free(moveResult.opponentMessage);
            
            usleep(200000);
        }
        
        // Si aucun move ne fonctionne, réessayer un getMove après avoir joué
        if (!resultsFound) {
            printf("Aucun résultat via sendMove, nouvel essai getMove...\n");
            
            moveResult.message = NULL;
            getMove(&dummyMove, &moveResult);
            
            if (moveResult.message && 
                (strstr(moveResult.message, "Objective") != NULL ||
                 strstr(moveResult.message, "score") != NULL ||
                 strstr(moveResult.message, "Score") != NULL ||
                 strstr(moveResult.message, "winner") != NULL ||
                 strstr(moveResult.message, "Total score") != NULL)) {
                
                printf("Résultats obtenus via getMove après sendMove!\n");
                strncpy(finalResultsMessage, moveResult.message, messageSize-1);
                resultsFound = true;
            }
            
            if (moveResult.message) free(moveResult.message);
            if (moveResult.opponentMessage) free(moveResult.opponentMessage);
        }
    }
    else {
        // Libérer la mémoire du message d'erreur
        if (moveResult.message) free(moveResult.message);
        if (moveResult.opponentMessage) free(moveResult.opponentMessage);
    }
    
    // Si nous n'avons toujours rien trouvé, utiliser un message par défaut
    if (!resultsFound) {
        printf("Aucun résultat détaillé trouvé, utilisation des informations du plateau...\n");
        sprintf(finalResultsMessage, "Résultats disponibles uniquement sur le plateau affiché ci-dessus.\n");
        resultsFound = true;
        
        // Afficher une dernière fois le plateau
        printBoard();
    }
    
    return resultsFound;
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

    //les bots qu'on peut jouer contre sont: PLAY_RANDOM, DO_NOTHING, NICE_BOT
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
    StrategyType strategy = STRATEGY_ADVANCED;  // Utiliser la stratégie avancée
    
    initPlayer(&gameState, strategy, &gameData);
    
    // Afficher l'état initial
    printBoard();
    printGameState(&gameState);
    
    // Variables pour le jeu
    int turnCounter = 0;
    bool gameRunning = true;
    bool firstTurn = true;
    int consecutiveErrors = 0;   // Compteur d'erreurs consécutives
    int cardDrawnThisTurn = 0;   // Pour suivre si on a déjà pioché une carte ce tour-ci
    char lastErrorMessage[1024] = {0};
    
    // D'après les tests, il semble que:
    // - Si starter == 0, c'est à nous de commencer
    // - Si starter == 1, c'est à l'adversaire de commencer
    bool ourTurn = (gameData.starter != 1);
    
    printf("Nous %s en premier\n", ourTurn ? "jouons" : "ne jouons pas");
    
    // Boucle principale du jeu
    while (gameRunning) {
        turnCounter++;
        printf("\n\n=== TOUR %d ===\n", turnCounter);
        
        // Mettre à jour les cartes visibles
        updateBoardState(&gameState);
        printf("C'est %s tour\n", ourTurn ? "notre" : "le tour de l'adversaire");
        
        // Vérification du timeout
        if (turnCounter > MAX_TURNS) {
            printf("Nombre maximum de tours atteint (%d), arrêt du jeu\n", MAX_TURNS);
            gameRunning = false;
            break;
        }
        
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
            printf("Erreur lors de l'affichage du plateau: 0x%x\n", boardResult);
            
            if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                printf("Fin de partie détectée lors de l'affichage du plateau.\n");
                gameRunning = false;
                break;
            }
            
            consecutiveErrors++;
            if (consecutiveErrors > 3) {
                printf("Trop d'erreurs consécutives, arrêt du jeu\n");
                gameRunning = false;
                break;
            }
            
            continue;
        }
        
        printf("Wagons - Nous: %d, Adversaire: %d\n", gameState.wagonsLeft, gameState.opponentWagonsLeft);
        
        if (ourTurn) {
            // Notre tour de jouer
            printf("\n--- Playing our turn ---\n");
            
            ResultCode playCode;
            MoveResult moveResult = {0};  // Initialize to zeros
            
            // Jouer notre tour
            if (firstTurn) {
                playCode = playFirstTurn(&gameState);
                if (playCode == ALL_GOOD) {
                    firstTurn = false;
                }
            } else {
                playCode = playTurn(&gameState, strategy);
            }
            
            // Vérification de l'intégrité de l'état du jeu
            if (gameState.nbClaimedRoutes > MAX_ROUTES || gameState.nbCards > MAX_CARDS || 
                gameState.nbObjectives > MAX_OBJECTIVES) {
                printf("ERREUR CRITIQUE: État du jeu corrompu après notre tour\n");
                printf("Routes: %d/%d, Cartes: %d/%d, Objectifs: %d/%d\n", 
                       gameState.nbClaimedRoutes, MAX_ROUTES, 
                       gameState.nbCards, MAX_CARDS, 
                       gameState.nbObjectives, MAX_OBJECTIVES);
                // Vous pourriez vouloir corriger l'état ici ou simplement continuer
            }

            if (playCode == ALL_GOOD) {
                printf("Successfully played our turn\n");
                consecutiveErrors = 0; // Réinitialiser le compteur d'erreurs
                
                // Vérifier si c'est le dernier tour
                if (isLastTurn(&gameState)) {
                    printf("LAST TURN: We have <= 2 wagons left\n");
                    gameState.lastTurn = 1;
                }
                
                // Gérer le flag cardDrawnThisTurn si la pioche est terminée
                if (cardDrawnThisTurn == 1) {
                    // Si on a joué un coup complet après avoir pioché une carte
                    cardDrawnThisTurn = 0;
                }
                
                // Passer au tour de l'adversaire
                ourTurn = false;
            } 
            else if (playCode == SERVER_ERROR) {
                printf("Erreur serveur pendant notre tour: 0x%x\n", playCode);
                
                // Sauvegarder le message d'erreur pour l'analyse finale
                if (moveResult.message) {
                    strncpy(lastErrorMessage, moveResult.message, sizeof(lastErrorMessage)-1);
                }
                
                // Le serveur a renvoyé une erreur - vérifier si c'est la fin de partie
                if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                    printf("Fin de partie détectée.\n");
                    gameRunning = false;
                    if (moveResult.message) {  // Check for NULL
                        cleanupMoveResult(&moveResult);
                    }
                    break;
                }
                
                // Si nous avons un message final, c'est probablement la fin de partie
                if (moveResult.message && isGameOver(moveResult.message)) {
                    printf("Fin de partie détectée via le message du serveur.\n");
                    gameRunning = false;
                    cleanupMoveResult(&moveResult);
                    break;
                }
                
                // Si le message contient "It's our turn", c'est probablement une désynchronisation
                if (moveResult.message && strstr(moveResult.message, "It's our turn")) {
                    printf("Le serveur indique que c'est notre tour, réessayer\n");
                    // Ne pas changer ourTurn, déjà à true
                    cleanupMoveResult(&moveResult);
                    continue;
                }
                
                // Si le message contient des indications de fin de jeu
                if (moveResult.message && 
                    (strstr(moveResult.message, "Bad protocol") || 
                     strstr(moveResult.message, "WAIT_GAME"))) {
                    printf("Le serveur indique que le jeu est terminé\n");
                    gameRunning = false;
                    cleanupMoveResult(&moveResult);
                    break;
                }
                
                // Autre erreur serveur
                printf("Server error - Game might be over\n");
                if (moveResult.message) printf("Message du serveur: %s\n", moveResult.message);
                cleanupMoveResult(&moveResult);
                
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("Trop d'erreurs consécutives, arrêt du jeu\n");
                    gameRunning = false;
                    break;
                }
                
                // Essayer de continuer avec le tour de l'adversaire
                ourTurn = false;
                continue;
            }
            else if (playCode == PARAM_ERROR || playCode == OTHER_ERROR) {
                printf("Erreur pendant notre tour: 0x%x\n", playCode);
                
                // Sauvegarder le message d'erreur pour l'analyse finale
                if (moveResult.message) {
                    strncpy(lastErrorMessage, moveResult.message, sizeof(lastErrorMessage)-1);
                }
                
                // Erreur - Si fin de partie
                if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                    printf("Fin de partie détectée. Dernière erreur: 0x%x\n", playCode);
                    gameRunning = false;
                    if (moveResult.message) {  // Check for NULL
                        cleanupMoveResult(&moveResult);
                    }
                    break;
                }
                
                // Vérifier si c'est un message de fin de partie
                if (moveResult.message && isGameOver(moveResult.message)) {
                    printf("Fin de partie détectée via le message d'erreur.\n");
                    gameRunning = false;
                    cleanupMoveResult(&moveResult);
                    break;
                }
                
                if (moveResult.message) printf("Message du serveur: %s\n", moveResult.message);
                cleanupMoveResult(&moveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("Trop d'erreurs consécutives, arrêt du jeu\n");
                    gameRunning = false;
                    break;
                }
                
                // Peut-être que c'est le tour de l'adversaire?
                printf("Error playing turn: 0x%x - Trying opponent's turn instead\n", playCode);
                ourTurn = false;
                continue;
            }
            else {
                // Autre erreur
                printf("Unknown error playing turn: 0x%x\n", playCode);
                if (moveResult.message) {
                    printf("Message: %s\n", moveResult.message);
                    strncpy(lastErrorMessage, moveResult.message, sizeof(lastErrorMessage)-1);
                }
                cleanupMoveResult(&moveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("Trop d'erreurs consécutives, arrêt du jeu\n");
                    gameRunning = false;
                    break;
                }
                
                // Attendre un peu et essayer l'autre joueur
                usleep(200000); // Remplacé sleep(2) par usleep pour être plus réactif
                ourTurn = false;
            }
        } 
        else {
            // C'est le tour de l'adversaire
            printf("\n--- Waiting for opponent's move ---\n");
            
            MoveData opponentMove;
            MoveResult opponentMoveResult = {0};  // Initialize to zeros
            ResultCode moveCode = getMove(&opponentMove, &opponentMoveResult);
            
            if (moveCode == ALL_GOOD) {
                // Réinitialiser le compteur d'erreurs
                consecutiveErrors = 0;
                
                // Vérifier explicitement si c'est la fin de partie
                if (opponentMoveResult.state != NORMAL_MOVE) {
                    // Vérifier si c'est un coup gagnant ou perdant
                    if (opponentMoveResult.state == 1 || // WINNING_MOVE
                        opponentMoveResult.state == -1) { // LOOSING_MOVE
                        printf("Fin de partie signalée par le serveur! État: %d\n", opponentMoveResult.state);
                        if (opponentMoveResult.message) {
                            strncpy(lastErrorMessage, opponentMoveResult.message, sizeof(lastErrorMessage)-1);
                        }
                        gameRunning = false;
                        cleanupMoveResult(&opponentMoveResult);
                        break;
                    }
                }
                
                // Vérifier si c'est la fin de partie ou si le message contient le score
                if (opponentMoveResult.message) {
                    if (isGameOver(opponentMoveResult.message)) {
                        printf("Fin de partie détectée dans le message de l'adversaire!\n");
                        strncpy(lastErrorMessage, opponentMoveResult.message, sizeof(lastErrorMessage)-1);
                        gameRunning = false;
                        cleanupMoveResult(&opponentMoveResult);
                        break;
                    }
                }
                
                // Vérifier si le serveur indique une désynchronisation
                if (opponentMoveResult.message && 
                    (strstr(opponentMoveResult.message, "It's our turn") || 
                     strstr(opponentMoveResult.message, "cannot ask for a move"))) {
                    
                    printf("CORRECTION DE TOUR: Le serveur indique que c'est notre tour\n");
                    ourTurn = true;  // Forcer notre tour
                    cleanupMoveResult(&opponentMoveResult);
                    continue;  // Revenir au début de la boucle
                }
                
                // L'adversaire a bien joué
                printf("Opponent made a move of type: %d\n", opponentMove.action);
                updateAfterOpponentMove(&gameState, &opponentMove);
                
                // Si l'adversaire ne rejoue pas, c'est notre tour
                if (!opponentMoveResult.replay) {
                    ourTurn = true;
                    printf("Now it's our turn\n");
                } else {
                    ourTurn = false; // C'est encore le tour de l'adversaire
                    printf("Opponent continues playing...\n");
                }
                
                // Nettoyage de la mémoire
                cleanupMoveResult(&opponentMoveResult);
            }
            else if (moveCode == SERVER_ERROR) {
                printf("Erreur serveur en attendant le coup de l'adversaire: 0x%x\n", moveCode);
                
                // Sauvegarder le message d'erreur pour l'analyse finale
                if (opponentMoveResult.message) {
                    strncpy(lastErrorMessage, opponentMoveResult.message, sizeof(lastErrorMessage)-1);
                }
                
                // Si le serveur indique que c'est notre tour
                if (opponentMoveResult.message && 
                    (strstr(opponentMoveResult.message, "It's our turn") || 
                     strstr(opponentMoveResult.message, "cannot ask for a move"))) {
                    
                    printf("CORRECTION DE TOUR: Le serveur indique que c'est notre tour\n");
                    ourTurn = true;  // Forcer notre tour
                    cleanupMoveResult(&opponentMoveResult);
                    continue;  // Revenir au début de la boucle
                }
                
                // Si le message contient des indications de fin de jeu
                if (opponentMoveResult.message && 
                    (strstr(opponentMoveResult.message, "Bad protocol") || 
                     strstr(opponentMoveResult.message, "WAIT_GAME"))) {
                    printf("Le serveur indique que le jeu est terminé\n");
                    gameRunning = false;
                    cleanupMoveResult(&opponentMoveResult);
                    break;
                }
                
                // Vérifier si c'est la fin de partie
                if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                    printf("Fin de partie détectée par erreur serveur.\n");
                    gameRunning = false;
                    cleanupMoveResult(&opponentMoveResult);
                    break;
                }
                
                // Autre erreur serveur
                printf("Server error - Game might be over\n");
                if (opponentMoveResult.message) printf("Message du serveur: %s\n", opponentMoveResult.message);
                cleanupMoveResult(&opponentMoveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("Trop d'erreurs consécutives, arrêt du jeu\n");
                    gameRunning = false;
                    break;
                }
                
                // Essayer notre tour au lieu du tour de l'adversaire
                ourTurn = true;
                continue;
            }
            else if (moveCode == PARAM_ERROR || moveCode == OTHER_ERROR) {
                printf("Erreur en attendant le coup de l'adversaire: 0x%x\n", moveCode);
                
                // Sauvegarder le message d'erreur pour l'analyse finale
                if (opponentMoveResult.message) {
                    strncpy(lastErrorMessage, opponentMoveResult.message, sizeof(lastErrorMessage)-1);
                }
                
                // Si le serveur indique que c'est notre tour
                if (opponentMoveResult.message && 
                    (strstr(opponentMoveResult.message, "It's our turn") || 
                     strstr(opponentMoveResult.message, "cannot ask for a move"))) {
                    
                    printf("CORRECTION DE TOUR: Le serveur indique que c'est notre tour\n");
                    ourTurn = true;  // Forcer notre tour
                    cleanupMoveResult(&opponentMoveResult);
                    continue;  // Revenir au début de la boucle
                }
                
                // Erreur - Si fin de partie
                if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                    printf("Fin de partie détectée. Dernière erreur: 0x%x\n", moveCode);
                    gameRunning = false;
                    cleanupMoveResult(&opponentMoveResult);
                    break;
                }
                
                if (opponentMoveResult.message) printf("Message du serveur: %s\n", opponentMoveResult.message);
                cleanupMoveResult(&opponentMoveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("Trop d'erreurs consécutives, arrêt du jeu\n");
                    gameRunning = false;
                    break;
                }
                
                // Peut-être que c'est notre tour?
                printf("Error getting opponent move: 0x%x - Trying our turn instead\n", moveCode);
                ourTurn = true;
                continue;
            }
            else {
                // Autre erreur
                printf("Unknown error getting opponent move: 0x%x\n", moveCode);
                if (opponentMoveResult.message) {
                    printf("Message: %s\n", opponentMoveResult.message);
                    strncpy(lastErrorMessage, opponentMoveResult.message, sizeof(lastErrorMessage)-1);
                }
                cleanupMoveResult(&opponentMoveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("Trop d'erreurs consécutives, arrêt du jeu\n");
                    gameRunning = false;
                    break;
                }
                
                // Attendre un peu et essayer notre tour
                usleep(200000); // Remplacé sleep(2) par usleep pour être plus réactif
                ourTurn = true;
            }
        }
        
        // Afficher l'état mis à jour
        printGameState(&gameState);
        
        // Calculer le score actuel
        int currentScore = calculateScore(&gameState);
        printf("Current score: %d\n", currentScore);
        
        // Vérification explicite du dernier tour
        if (gameState.wagonsLeft <= 2) {
            printf("DERNIER TOUR: Il nous reste <= 2 wagons!\n");
            gameState.lastTurn = 1;
        }
        
        if (gameState.opponentWagonsLeft <= 2) {
            printf("DERNIER TOUR: L'adversaire a <= 2 wagons!\n");
            gameState.lastTurn = 1;
        }
        
        // Vérifier si le dernier tour est terminé
        if (gameState.lastTurn && !ourTurn) {
            printf("Dernier tour terminé. Fin de la partie.\n");
            gameRunning = false;
            break;
        }
        
        // Attendre un peu avant le prochain tour
        usleep(150000); // Remplacé sleep(1) par usleep pour être plus réactif
    }
    
    // Calculer notre score final
    int finalScore = calculateScore(&gameState);
    
    // Afficher le plateau final
    printf("\n\n===== PLATEAU FINAL =====\n");
    printBoard();
    printf("==========================\n\n");
    
    // Tentative agressive de récupération des résultats finaux
    printf("\n===== FIN DE PARTIE =====\n");
    printf("Récupération des résultats finaux...\n");
    
    // Pour s'assurer que le jeu est vraiment terminé
    usleep(500000);
    
    // Utiliser notre nouvelle fonction spécialisée pour obtenir les résultats
    char finalResultsMessage[2048] = {0};
bool resultsFound = captureEndGameResults(finalResultsMessage, sizeof(finalResultsMessage));
    if (!resultsFound && strlen(lastErrorMessage) > 0) {
        printf("Utilisation du dernier message d'erreur comme résultat\n");
        strncpy(finalResultsMessage, lastErrorMessage, sizeof(finalResultsMessage)-1);
        resultsFound = true;
    }
    
    // Si toujours pas de résultats, essayer d'extraire les données du plateau
    if (!resultsFound) {
        // Créer un message à partir des données visibles
        snprintf(finalResultsMessage, sizeof(finalResultsMessage),
                "Résultats extraits du plateau:\n"
                "Notre score final (calculé): %d\n"
                "Nombre de wagons restants: %d\n"
                "Nombre d'objectifs complétés: %d\n",
                finalScore, gameState.wagonsLeft, gameState.nbObjectives);
    }
    
    // Afficher le résultat final
    printGameResult(finalScore, finalResultsMessage);
    
    // Nettoyage final
    if (gameData.gameName) free(gameData.gameName);
    if (gameData.trackData) free(gameData.trackData);
    
    quitGame();
    printf("Game terminated.\n");
    
    return 0;
}