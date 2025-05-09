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

// Nombre maximum de tours avant d'arrêter automatiquement
#define MAX_TURNS 200

// Niveau de débogage
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
    return (strstr(message, "winner") != NULL || 
            strstr(message, "Total score") != NULL || 
            strstr(message, "GAME OVER") != NULL ||
            (strstr(message, "Player") != NULL && strstr(message, "has the longest path") != NULL));
}

// Extrait et affiche le score à partir d'un message du serveur
void printGameResult(char* message, const char* playerName, int ourCalculatedScore) {
    if (!message) {
        printf("\n==================================================\n");
        printf("              PARTIE TERMINÉE                    \n");
        printf("==================================================\n");
        printf("Notre score calculé: %d\n", ourCalculatedScore);
        printf("Pour connaître le score de l'adversaire et le gagnant,\n");
        printf("consultez le plateau final affiché ci-dessus.\n");
        printf("==================================================\n\n");
        return;
    }
    
    printf("\n==================================================\n");
    printf("              RÉSULTAT DE LA PARTIE               \n");
    printf("==================================================\n");
    
    // Variables pour les scores
    int ourScoreFromMessage = 0;
    int opponentScoreFromMessage = 0;
    char opponentName[50] = "Adversaire";
    char ourName[50] = "Notre bot";
    bool winnerDetermined = false;
    bool weWon = false;
    
    // Afficher l'intégralité du message, ligne par ligne avec formatage
    char* messageCopy = strdup(message);
    char* line = strtok(messageCopy, "\n");
    while (line) {
        printf("%s\n", line);
        
        // Chercher des mentions explicites de victoire
        if (strstr(line, "You are the winner")) {
            winnerDetermined = true;
            weWon = true;
        } else if (strstr(line, "Your opponent is the winner")) {
            winnerDetermined = true;
            weWon = false;
        }
        
        // Extraire les scores si disponibles dans une ligne de score total
        if (strstr(line, "Total score:")) {
            char scoreLine[512];
            strcpy(scoreLine, line);
            
            // Analyse la ligne des scores totaux
            char* token = strtok(scoreLine, ":");
            token = strtok(NULL, ":");  // Skip "Total score"
            
            // Parcourir les tokens pour trouver les scores
            // Format typique: "    PlayNice: 138pts        Georges2222: 3pts"
            while (token) {
                char name[50] = {0};
                int score = 0;
                
                // Essayer d'extraire nom et score
                if (sscanf(token, "%s %dpts", name, &score) == 2) {
                    // Enlever les caractères superflus du nom
                    char* cleanName = name;
                    while (*cleanName == ' ') cleanName++;
                    
                    // Déterminer si c'est notre score ou celui de l'adversaire
                    if (strstr(cleanName, playerName)) {
                        strcpy(ourName, cleanName);
                        ourScoreFromMessage = score;
                    } else {strcpy(opponentName, cleanName);
                        opponentScoreFromMessage = score;
                    }
                }
                
                token = strtok(NULL, ":");
            }
            
            // Si les scores ont été extraits, on peut déterminer le gagnant par les points
            if (ourScoreFromMessage > 0 || opponentScoreFromMessage > 0) {
                if (!winnerDetermined) {
                    weWon = (ourScoreFromMessage > opponentScoreFromMessage);
                    winnerDetermined = true;
                }
            }
        }
        
        line = strtok(NULL, "\n");
    }
    free(messageCopy);
    
    // Si nous n'avons pas trouvé de scores dans le message, utiliser ceux calculés localement
    if (ourScoreFromMessage == 0 && ourCalculatedScore > 0) {
        ourScoreFromMessage = ourCalculatedScore;
    }
    
    // Afficher clairement qui a gagné
    printf("\n==================================================\n");
    if (winnerDetermined) {
        if (weWon) {
            printf("             🏆 VOUS AVEZ GAGNÉ! 🏆             \n");
        } else {
            printf("            😢 VOUS AVEZ PERDU 😢               \n");
        }
        
        if (ourScoreFromMessage > 0 || opponentScoreFromMessage > 0) {
            printf("       Scores finaux: %s = %d, %s = %d       \n", 
                   ourName, ourScoreFromMessage, opponentName, opponentScoreFromMessage);
        }
    } else {
        printf("              RÉSULTAT INDÉTERMINÉ              \n");
        if (ourCalculatedScore > 0) {
            printf("         Notre score calculé: %d               \n", ourCalculatedScore);
            printf("     Consultez le plateau pour plus de détails    \n");
        }
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
    StrategyType strategy = STRATEGY_ADVANCED;  // Utiliser la stratégie avancée
    
    initPlayer(&gameState, strategy, &gameData);
    
    // Afficher l'état initial
    printBoard();
    printGameState(&gameState);
    
    // Variables pour le jeu
    int turnCounter = 0;
    bool gameRunning = true;
    bool firstTurn = true;
    char* finalGameMessage = NULL;
    int consecutiveErrors = 0;   // Compteur d'erreurs consécutives
    
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
            printf("⚠️ Nombre maximum de tours atteint (%d), arrêt du jeu ⚠️\n", MAX_TURNS);
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
            printf("⚠️ Erreur lors de l'affichage du plateau: 0x%x ⚠️\n", boardResult);
            
            if (gameState.lastTurn || gameState.opponentWagonsLeft <= 0 || gameState.wagonsLeft <= 0) {
                printf("Fin de partie détectée lors de l'affichage du plateau.\n");
                gameRunning = false;
                break;
            }
            
            consecutiveErrors++;
            if (consecutiveErrors > 3) {
                printf("⚠️ Trop d'erreurs consécutives, arrêt du jeu ⚠️\n");
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
                consecutiveErrors = 0; // Réinitialiser le compteur d'erreurs
                
                // Vérifier si c'est le dernier tour
                if (isLastTurn(&gameState)) {
                    printf("LAST TURN: We have <= 2 wagons left\n");
                    gameState.lastTurn = 1;
                }
                
                // Gestion explicite du flag replay
                if (moveResult.replay) {
                    printf("⚠️ Nous devons jouer à nouveau! ⚠️\n");
                    ourTurn = true;  // S'assurer que c'est encore notre tour
                } else {
                    ourTurn = false;  // Passer au tour de l'adversaire
                }
            } 
            else if (playCode == SERVER_ERROR) {
                printf("⚠️ Erreur serveur pendant notre tour: 0x%x ⚠️\n", playCode);
                
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
                    if (finalGameMessage) free(finalGameMessage);
                    finalGameMessage = strdup(moveResult.message);
                    gameRunning = false;
                    cleanupMoveResult(&moveResult);
                    break;
                }
                
                // Si le message contient "It's our turn", c'est probablement une désynchronisation
                if (moveResult.message && strstr(moveResult.message, "It's our turn")) {
                    printf("⚠️ Le serveur indique que c'est notre tour, réessayer ⚠️\n");
                    // Ne pas changer ourTurn, déjà à true
                    cleanupMoveResult(&moveResult);
                    continue;
                }
                
                // Si le message contient des indications de fin de jeu
                if (moveResult.message && 
                    (strstr(moveResult.message, "Bad protocol") || 
                     strstr(moveResult.message, "WAIT_GAME"))) {
                    printf("⚠️ Le serveur indique que le jeu est terminé ⚠️\n");
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
                    printf("⚠️ Trop d'erreurs consécutives, arrêt du jeu ⚠️\n");
                    gameRunning = false;
                    break;
                }
                
                // Essayer de continuer avec le tour de l'adversaire
                ourTurn = false;
                continue;
            }
            else if (playCode == PARAM_ERROR || playCode == OTHER_ERROR) {
                printf("⚠️ Erreur pendant notre tour: 0x%x ⚠️\n", playCode);
                
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
                    if (finalGameMessage) free(finalGameMessage);
                    finalGameMessage = strdup(moveResult.message);
                    gameRunning = false;
                    cleanupMoveResult(&moveResult);
                    break;
                }
                
                if (moveResult.message) printf("Message du serveur: %s\n", moveResult.message);
                cleanupMoveResult(&moveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("⚠️ Trop d'erreurs consécutives, arrêt du jeu ⚠️\n");
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
                if (moveResult.message) printf("Message: %s\n", moveResult.message);
                cleanupMoveResult(&moveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("⚠️ Trop d'erreurs consécutives, arrêt du jeu ⚠️\n");
                    gameRunning = false;
                    break;
                }
                
                // Attendre un peu et essayer l'autre joueur
                sleep(2);
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
                
                // Vérifier si c'est la fin de partie ou si le message contient le score
                if (opponentMoveResult.message) {
                    if (isGameOver(opponentMoveResult.message)) {
                        printf("Fin de partie détectée dans le message de l'adversaire!\n");
                        // Sauvegarder le message pour l'afficher à la fin
                        if (finalGameMessage) free(finalGameMessage);
                        finalGameMessage = strdup(opponentMoveResult.message);
                        
                        gameRunning = false;
                        // Libérer la mémoire
                        cleanupMoveResult(&opponentMoveResult);
                        break;
                    }
                    // Même si ce n'est pas la fin, on garde le message s'il contient un score
                    else if (strstr(opponentMoveResult.message, "Total score")) {
                        if (finalGameMessage) free(finalGameMessage);
                        finalGameMessage = strdup(opponentMoveResult.message);
                    }
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
                printf("⚠️ Erreur serveur en attendant le coup de l'adversaire: 0x%x ⚠️\n", moveCode);
                
                // Si le serveur indique que c'est notre tour
                if (opponentMoveResult.message && 
                    (strstr(opponentMoveResult.message, "It's our turn") || 
                     strstr(opponentMoveResult.message, "cannot ask for a move"))) {
                    
                    printf("⚠️ CORRECTION DE TOUR: Le serveur indique que c'est notre tour ⚠️\n");
                    ourTurn = true;  // Forcer notre tour
                    cleanupMoveResult(&opponentMoveResult);
                    continue;  // Revenir au début de la boucle
                }
                
                // Si le message contient des indications de fin de jeu
                if (opponentMoveResult.message && 
                    (strstr(opponentMoveResult.message, "Bad protocol") || 
                     strstr(opponentMoveResult.message, "WAIT_GAME"))) {
                    printf("⚠️ Le serveur indique que le jeu est terminé ⚠️\n");
                    gameRunning = false;
                    cleanupMoveResult(&opponentMoveResult);
                    break;
                }
                
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
                if (opponentMoveResult.message) printf("Message du serveur: %s\n", opponentMoveResult.message);
                cleanupMoveResult(&opponentMoveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("⚠️ Trop d'erreurs consécutives, arrêt du jeu ⚠️\n");
                    gameRunning = false;
                    break;
                }
                
                // Essayer notre tour au lieu du tour de l'adversaire
                ourTurn = true;
                continue;
            }
            else if (moveCode == PARAM_ERROR || moveCode == OTHER_ERROR) {
                printf("⚠️ Erreur en attendant le coup de l'adversaire: 0x%x ⚠️\n", moveCode);
                
                // Si le serveur indique que c'est notre tour
                if (opponentMoveResult.message && 
                    (strstr(opponentMoveResult.message, "It's our turn") || 
                     strstr(opponentMoveResult.message, "cannot ask for a move"))) {
                    
                    printf("⚠️ CORRECTION DE TOUR: Le serveur indique que c'est notre tour ⚠️\n");
                    ourTurn = true;  // Forcer notre tour
                    cleanupMoveResult(&opponentMoveResult);
                    continue;  // Revenir au début de la boucle
                }
                
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
                
                if (opponentMoveResult.message) printf("Message du serveur: %s\n", opponentMoveResult.message);
                cleanupMoveResult(&opponentMoveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("⚠️ Trop d'erreurs consécutives, arrêt du jeu ⚠️\n");
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
                if (opponentMoveResult.message) printf("Message: %s\n", opponentMoveResult.message);
                cleanupMoveResult(&opponentMoveResult);
                
                // Incrémenter le compteur d'erreurs
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    printf("⚠️ Trop d'erreurs consécutives, arrêt du jeu ⚠️\n");
                    gameRunning = false;
                    break;
                }
                
                // Attendre un peu et essayer notre tour
                sleep(2);
                ourTurn = true;
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
        if (gameState.lastTurn && !ourTurn) {
            printf("Dernier tour terminé. Fin de la partie.\n");
            gameRunning = false;
            
            // Afficher le plateau final
            printf("\n\n===== PLATEAU FINAL =====\n");
            printf("EXAMINEZ CE PLATEAU POUR VOIR LES SCORES FINAUX DES JOUEURS!\n");
            printBoard();
            printf("==========================\n\n");
            
            break;
        }
        
        // Attendre un peu avant le prochain tour
        sleep(1);
    }
    
    // Calculer notre score final
    int finalScore = calculateScore(&gameState);
    
    // Afficher le résultat final si disponible
    if (finalGameMessage) {
        printGameResult(finalGameMessage, playerName, finalScore);
        free(finalGameMessage);
    } else {
        // Si pas de message final, utiliser les informations calculées localement
        printf("\n==================================================\n");
        printf("              RÉSULTAT DE LA PARTIE               \n");
        printf("==================================================\n");
        
        // Utiliser uniquement notre score calculé
        printf("Notre score final calculé: %d\n", finalScore);
        printf("Pour connaître le score de l'adversaire et le gagnant,\n");
        printf("consultez le plateau final affiché ci-dessus.\n");
        
        printf("==================================================\n\n");
    }
    
    // Nettoyage final
    if (gameData.gameName) free(gameData.gameName);
    if (gameData.trackData) free(gameData.trackData);
    
    quitGame();
    printf("Game terminated.\n");
    
    return 0;
}