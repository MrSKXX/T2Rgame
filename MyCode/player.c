#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "gamestate.h"
#include "strategy.h"
#include "rules.h"

// Fonction pour afficher les informations d'une carte
void printCardName(CardColor card) {
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    if (card >= 0 && card < 10) {
        printf("Card color: %s\n", cardNames[card]);
    } else {
        printf("Unknown card: %d\n", card);
    }
}

// Fonction pour afficher les informations d'un objectif
void printObjective(Objective objective) {
    printf("From city %d to city %d, score %d\n", 
           objective.from, objective.to, objective.score);
    printf("  From: ");
    printCity(objective.from);
    printf(" to ");
    printCity(objective.to);
    printf("\n");
}

// Libère la mémoire allouée dans MoveResult
void cleanupMoveResult(MoveResult *moveResult) {
    if (moveResult->opponentMessage) free(moveResult->opponentMessage);
    if (moveResult->message) free(moveResult->message);
    moveResult->opponentMessage = NULL;
    moveResult->message = NULL;
}
// Initialise le joueur
void initPlayer(GameState* state, StrategyType strategy, GameData* gameData) {
    // Vérifier les paramètres
    if (!state || !gameData) {
        printf("Error: NULL state or gameData in initPlayer\n");
        return;
    }
    
    // Initialise l'état du jeu avec les données initiales
    initGameState(state, gameData);
    
    printf("Player initialized with strategy type: %d\n", strategy);
    printf("Starting game with %d cities and %d tracks\n", state->nbCities, state->nbTracks);
    
    // Vérifier l'état après initialisation
    printf("Debug - Wagons left after init: %d\n", state->wagonsLeft);
    
    // Initialisation des cartes en main (les 4 cartes initiales)
    for (int i = 0; i < 4; i++) {
        if (gameData->cards[i] >= 0 && gameData->cards[i] < 10) {
            addCardToHand(state, gameData->cards[i]);
        } else {
            printf("Warning: Invalid card color: %d\n", gameData->cards[i]);
        }
    }
    
    // Affiche l'état initial
    printGameState(state);
}

// Gère le premier tour (choix des objectifs)
ResultCode playFirstTurn(GameState* state) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult = {0};  // Initialisation explicite à zéro
    
    printf("First turn: drawing objectives\n");
    
    // Demande de piocher des objectifs
    myMove.action = DRAW_OBJECTIVES;
    
    // Envoie la requête
    returnCode = sendMove(&myMove, &myMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Error sending DRAW_OBJECTIVES: 0x%x\n", returnCode);
        if (myMoveResult.message) {
            printf("Server message: %s\n", myMoveResult.message);
        }
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    printf("Received objectives, now choosing which to keep\n");
    
    // CORRECTION: Vérifier que les objectifs reçus sont valides
    bool objectivesValid = true;
    for (int i = 0; i < 3; i++) {
        if (myMoveResult.objectives[i].from < 0 || 
            myMoveResult.objectives[i].from >= state->nbCities ||
            myMoveResult.objectives[i].to < 0 || 
            myMoveResult.objectives[i].to >= state->nbCities) {
            printf("WARNING: Invalid objective received: From %d to %d\n", 
                   myMoveResult.objectives[i].from, myMoveResult.objectives[i].to);
            objectivesValid = false;
        } else {
            // Affiche les objectifs reçus
            printf("Objective %d: ", i+1);
            printObjective(myMoveResult.objectives[i]);
        }
    }
    
    // Si les objectifs ne sont pas valides, on retourne une erreur
    if (!objectivesValid) {
        printf("ERROR: Invalid objectives received from server\n");
        cleanupMoveResult(&myMoveResult);
        return PARAM_ERROR;
    }
    
    // Choisit quels objectifs garder (stratégie simple: garder tous les objectifs)
    bool chooseObjectives[3] = {true, true, true};
    
    // Si nous avons une stratégie plus avancée pour choisir les objectifs
    chooseObjectivesStrategy(state, myMoveResult.objectives, chooseObjectives);
    
    // Vérifier qu'au moins un objectif est choisi
    bool atLeastOneChosen = false;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            atLeastOneChosen = true;
            break;
        }
    }
    
    if (!atLeastOneChosen) {
        printf("WARNING: No objectives chosen, selecting the first one by default\n");
        chooseObjectives[0] = true;
    }
    
    // Prépare la réponse avec les choix d'objectifs
    MoveData chooseMove;
    MoveResult chooseMoveResult = {0};  // Initialisation explicite à zéro
    
    chooseMove.action = CHOOSE_OBJECTIVES;
    chooseMove.chooseObjectives[0] = chooseObjectives[0];
    chooseMove.chooseObjectives[1] = chooseObjectives[1];
    chooseMove.chooseObjectives[2] = chooseObjectives[2];
    
    // Compte combien d'objectifs nous gardons pour ajouter à notre état
    int objectivesToKeep = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            objectivesToKeep++;
        }
    }
    
    // Ajoute les objectifs choisis à notre état
    Objective chosenObjectives[3];
    int idx = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            chosenObjectives[idx++] = myMoveResult.objectives[i];
        }
    }
    
    // Libère la mémoire du résultat précédent
    cleanupMoveResult(&myMoveResult);
    
    // Envoie les choix
    returnCode = sendMove(&chooseMove, &chooseMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Error choosing objectives: 0x%x\n", returnCode);
        if (chooseMoveResult.message) {
            printf("Server message: %s\n", chooseMoveResult.message);
        }
        cleanupMoveResult(&chooseMoveResult);
        return returnCode;
    }
    
    // CORRECTION: Ajouter les objectifs à notre état seulement après confirmation du serveur
    addObjectives(state, chosenObjectives, objectivesToKeep);
    
    printf("Successfully chose objectives\n");
    cleanupMoveResult(&chooseMoveResult);
    
    return ALL_GOOD;
}

// Joue un tour normal
// Modification de la fonction playTurn pour capturer le message final
ResultCode playTurn(GameState* state, StrategyType strategy) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult = {0};  // Initialisation à zéro
    BoardState boardState;
    
    // Variable statique pour suivre si une carte a déjà été piochée ce tour-ci
    static int cardDrawnThisTurn = 0;  // 0 = début de tour, 1 = une carte non-locomotive déjà piochée
    
    // Récupère l'état du plateau (cartes visibles)
    returnCode = getBoardState(&boardState);
    if (returnCode != ALL_GOOD) {
        printf("Error getting board state: 0x%x\n", returnCode);
        return returnCode;
    }
    
    // Met à jour les cartes visibles dans notre état
    for (int i = 0; i < 5; i++) {
        state->visibleCards[i] = boardState.card[i];
    }
    
    checkObjectivesPaths(state);

    // Si nous avons déjà pioché une carte non-locomotive ce tour-ci
    if (cardDrawnThisTurn == 1) {
        printf("Second card draw this turn - cannot draw a visible locomotive\n");
        
        // Pour la seconde carte, on ne peut pas prendre de locomotive visible
        // Soit une carte visible non-locomotive, soit une carte aveugle
        
        // Vérifier s'il y a des cartes visibles non-locomotive
        CardColor cardToDraw = (CardColor)-1;
        for (int i = 0; i < 5; i++) {
            if (state->visibleCards[i] != LOCOMOTIVE && state->visibleCards[i] != NONE) {
                cardToDraw = state->visibleCards[i];
                break;
            }
        }
        
        if (cardToDraw != (CardColor)-1) {
            // Il y a une carte visible non-locomotive disponible
            myMove.action = DRAW_CARD;
            myMove.drawCard = cardToDraw;
            printf("Drawing second visible card: ");
            printCardName(cardToDraw);
        } else {
            // Pas de carte visible non-locomotive, on pioche une carte aveugle
            myMove.action = DRAW_BLIND_CARD;
            printf("Drawing second blind card\n");
        }
        
        // Réinitialiser le compteur pour le prochain tour - TOUJOURS FAIT APRÈS AVOIR ENVOYÉ LE COUP
        cardDrawnThisTurn = 0;
    } else {
        // Début de tour normal - toutes les options sont disponibles
        
        // Décide de l'action à effectuer selon la stratégie
        if (!decideNextMove(state, strategy, &myMove)) {
            // Si aucune décision n'a été prise, on pioche une carte aveugle par défaut
            printf("No specific move decided, drawing blind card by default\n");
            myMove.action = DRAW_BLIND_CARD;
        }
        
        // NE PAS RÉINITIALISER cardDrawnThisTurn ICI - On le fera après avoir vérifié le résultat du serveur
    }
    
    // Affiche l'action choisie
    printf("Decided action: ");
    switch (myMove.action) {
        case CLAIM_ROUTE:
            printf("Claim route from ");
            printCity(myMove.claimRoute.from);
            printf(" to ");
            printCity(myMove.claimRoute.to);
            printf(" with color ");
            printCardName(myMove.claimRoute.color);
            printf(" using %d locomotives\n", myMove.claimRoute.nbLocomotives);
            break;
            
        case DRAW_CARD:
            printf("Draw visible card: ");
            printCardName(myMove.drawCard);
            break;
            
        case DRAW_BLIND_CARD:
            printf("Draw blind card\n");
            break;
            
        case DRAW_OBJECTIVES:
            printf("Draw objectives\n");
            break;
            
        default:
            printf("Unknown action type: %d\n", myMove.action);
    }
    
    // Envoie l'action
    returnCode = sendMove(&myMove, &myMoveResult);
    
    // Vérifier si c'est la fin du jeu en recherchant dans le message
    if (returnCode == SERVER_ERROR || returnCode == PARAM_ERROR) {
        printf("Game has ended or error occurred. Return code: 0x%x\n", returnCode);
        
        // Si le message contient "Total score" ou "winner", c'est une fin normale de partie
        if (myMoveResult.message && 
            (strstr(myMoveResult.message, "Total score") || strstr(myMoveResult.message, "winner"))) {
            
            printf("\n==================================================\n");
            printf("              RÉSULTAT DE LA PARTIE               \n");
            printf("==================================================\n");
            printf("%s\n", myMoveResult.message);
            printf("==================================================\n\n");
            
            // Même si c'est une fin de partie, on retourne SERVER_ERROR pour indiquer au main de terminer
            cleanupMoveResult(&myMoveResult);
            return SERVER_ERROR;
        }
        
        if (myMoveResult.opponentMessage) {
            printf("Server message: %s\n", myMoveResult.opponentMessage);
        }
        if (myMoveResult.message) {
            printf("Message: %s\n", myMoveResult.message);
        }
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    if (returnCode != ALL_GOOD) {
        printf("Error sending move: 0x%x\n", returnCode);
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    // CORRECTION: Adapter la valeur de cardDrawnThisTurn en fonction de l'action et du résultat
    bool needSecondCard = false;
    
    // Traite le résultat de l'action
    switch (myMove.action) {
        case CLAIM_ROUTE:
            // Met à jour notre état après avoir pris une route
            addClaimedRoute(state, myMove.claimRoute.from, myMove.claimRoute.to);
            
            // Trouve la longueur de la route
            int routeLength = 0;
            for (int i = 0; i < state->nbTracks; i++) {
                if ((state->routes[i].from == myMove.claimRoute.from && state->routes[i].to == myMove.claimRoute.to) ||
                    (state->routes[i].from == myMove.claimRoute.to && state->routes[i].to == myMove.claimRoute.from)) {
                    routeLength = state->routes[i].length;
                    break;
                }
            }
            
            // Retire les cartes utilisées
            removeCardsForRoute(state, myMove.claimRoute.color, routeLength, myMove.claimRoute.nbLocomotives);
            printf("Successfully claimed route\n");
            
            // Après une prise de route, le tour est terminé
            cardDrawnThisTurn = 0;
            break;
            
        case DRAW_CARD:
            // Ajoute la carte piochée à notre main
            addCardToHand(state, myMove.drawCard);
            printf("Successfully drew visible card\n");
            
            // Si c'est une locomotive visible, le tour est terminé
            if (myMove.drawCard == LOCOMOTIVE) {
                printf("Drew a locomotive - turn ends\n");
                cardDrawnThisTurn = 0;
            } else {
                // Sinon, on marque qu'on a pioché une carte et on pourra en piocher une seconde
                if (cardDrawnThisTurn == 0) {  // Si c'est la première carte du tour
                    printf("Drew a non-locomotive card - can draw a second card\n");
                    needSecondCard = true;
                    cardDrawnThisTurn = 1;
                } else {
                    // C'était la seconde carte, le tour est terminé
                    printf("Drew second card - turn ends\n");
                    cardDrawnThisTurn = 0;
                }
            }
            break;
            
        case DRAW_BLIND_CARD:
            // Ajoute la carte piochée à notre main
            addCardToHand(state, myMoveResult.card);
            printf("Successfully drew blind card: ");
            printCardName(myMoveResult.card);
            
            // Si on a pioché une locomotive aveugle, on ne piochera pas de seconde carte
            if (myMoveResult.card == LOCOMOTIVE && myMoveResult.replay == false) {
                printf("Blind card is a locomotive that doesn't allow a second draw - turn ends\n");
                cardDrawnThisTurn = 0;
            } else {
                // Pour les autres cartes ou si le serveur indique explicitement qu'on peut rejouer
                if (cardDrawnThisTurn == 0 && myMoveResult.replay) {  // Si c'est la première carte du tour
                    printf("Drew blind card - can draw a second card\n");
                    needSecondCard = true;
                    cardDrawnThisTurn = 1;
                } else {
                    // C'était la seconde carte ou le serveur n'autorise pas de seconde carte
                    printf("Drew blind card - turn ends\n");
                    cardDrawnThisTurn = 0;
                }
            }
            break;
            
        case DRAW_OBJECTIVES:
            // CORRECTION: Les objectifs nécessitent une étape supplémentaire
            // Le tour n'est pas terminé juste après avoir pioché les objectifs
            
            // Affiche les objectifs reçus
            printf("Received objectives, now choosing which to keep\n");
            for (int i = 0; i < 3; i++) {
                printf("Objective %d: ", i+1);
                printObjective(myMoveResult.objectives[i]);
            }
            
            // Choisit quels objectifs garder
            bool chooseObjectives[3] = {true, true, true};
            chooseObjectivesStrategy(state, myMoveResult.objectives, chooseObjectives);
            
            // Prépare la réponse
            MoveData chooseMove;
            MoveResult chooseMoveResult;
            
            chooseMove.action = CHOOSE_OBJECTIVES;
            chooseMove.chooseObjectives[0] = chooseObjectives[0];
            chooseMove.chooseObjectives[1] = chooseObjectives[1];
            chooseMove.chooseObjectives[2] = chooseObjectives[2];
            
            // Compte combien d'objectifs nous gardons
            int objectivesToKeep = 0;
            for (int i = 0; i < 3; i++) {
                if (chooseObjectives[i]) objectivesToKeep++;
            }
            
            // Crée un tableau des objectifs choisis
            Objective chosenObjectives[3];
            int idx = 0;
            for (int i = 0; i < 3; i++) {
                if (chooseObjectives[i]) {
                    chosenObjectives[idx++] = myMoveResult.objectives[i];
                }
            }
            
            // Libère la mémoire
            cleanupMoveResult(&myMoveResult);
            
            // Envoie les choix
            returnCode = sendMove(&chooseMove, &chooseMoveResult);
            
            if (returnCode != ALL_GOOD) {
                printf("Error choosing objectives: 0x%x\n", returnCode);
                cleanupMoveResult(&chooseMoveResult);
                return returnCode;
            }
            
            // Ajoute les objectifs choisis à notre état
            addObjectives(state, chosenObjectives, objectivesToKeep);
            
            printf("Successfully chose objectives\n");
            cleanupMoveResult(&chooseMoveResult);
            
            // Après avoir choisi les objectifs, le tour est terminé
            cardDrawnThisTurn = 0;
            break;
    }
    
    // Libère la mémoire si ce n'est pas déjà fait (pour DRAW_OBJECTIVES, c'est déjà fait)
    if (myMove.action != DRAW_OBJECTIVES) {
        cleanupMoveResult(&myMoveResult);
    }
    
    // Met à jour la connectivité des villes après notre action
    updateCityConnectivity(state);
    
    // CORRECTION: Si ce n'est pas la fin du tour et qu'on doit piocher une seconde carte
    // CORRECTION: Si ce n'est pas la fin du tour et qu'on doit piocher une seconde carte
// Dans player.c - Modification de la fonction playTurn

// CORRECTION: Si ce n'est pas la fin du tour et qu'on doit piocher une seconde carte
if (needSecondCard) {
    printf("\nNow drawing a second card for this turn\n");
    
    // IMPORTANT: Mettre à jour l'état du plateau avant de piocher la seconde carte
    BoardState boardState;
    ResultCode updateResult = getBoardState(&boardState);
    if (updateResult != ALL_GOOD) {
        printf("Error getting updated board state before second card: 0x%x\n", updateResult);
        return updateResult;
    }
    
    // Mettre à jour les cartes visibles dans notre état
    for (int i = 0; i < 5; i++) {
        state->visibleCards[i] = boardState.card[i];
    }
    
    // Réinitialiser la variable MoveData pour la seconde carte
    MoveData secondCardMove;
    MoveResult secondCardResult = {0};
    
    // Pour la seconde carte, on ne peut pas prendre de locomotive visible
    // Soit une carte visible non-locomotive, soit une carte aveugle
    
    // Vérifier s'il y a des cartes visibles non-locomotive
    CardColor cardToDraw = (CardColor)-1;
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] != LOCOMOTIVE && state->visibleCards[i] != NONE) {
            cardToDraw = state->visibleCards[i];
            break;
        }
    }
    
    if (cardToDraw != (CardColor)-1) {
        // Il y a une carte visible non-locomotive disponible
        secondCardMove.action = DRAW_CARD;
        secondCardMove.drawCard = cardToDraw;
        printf("Second card draw this turn - cannot draw a visible locomotive\n");
        printf("Drawing second visible card: ");
        printCardName(cardToDraw);
    } else {
        // Pas de carte visible non-locomotive, on pioche une carte aveugle
        secondCardMove.action = DRAW_BLIND_CARD;
        printf("Second card draw this turn - drawing blind card\n");
    }
    
    // Envoyer l'action
    returnCode = sendMove(&secondCardMove, &secondCardResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Error drawing second card: 0x%x\n", returnCode);
        cleanupMoveResult(&secondCardResult);
        return returnCode;
    }
    
    // Traiter le résultat
    if (secondCardMove.action == DRAW_CARD) {
        addCardToHand(state, secondCardMove.drawCard);
        printf("Successfully drew second visible card\n");
    } else {
        addCardToHand(state, secondCardResult.card);
        printf("Successfully drew second blind card: ");
        printCardName(secondCardResult.card);
    }
    
    // Le tour est maintenant terminé
    cardDrawnThisTurn = 0;
    
    // Nettoyer la mémoire
    cleanupMoveResult(&secondCardResult);
    
    return ALL_GOOD;
}

    // Fin de la fonction
    return ALL_GOOD;
}
