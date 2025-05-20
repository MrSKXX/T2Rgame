#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "gamestate.h"
#include "strategy.h"
#include "rules.h"


// Déclaration externe de checkObjectivesPaths
extern void checkObjectivesPaths(GameState* state);

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
    
    // VÉRIFICATION ET CORRECTION DES ROUTES AVANT D'ENVOYER L'ACTION
if (myMove.action == CLAIM_ROUTE) {
    int from = myMove.claimRoute.from;
    int to = myMove.claimRoute.to;
    CardColor color = myMove.claimRoute.color;
    
    // Trouver l'index de la route
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex != -1) {
        CardColor routeColor = state->routes[routeIndex].color;
        CardColor routeSecondColor = state->routes[routeIndex].secondColor;
        
        printf("VÉRIFICATION route %d-%d: couleur choisie %d, couleurs valides: %d", 
               from, to, color, routeColor);
        
        if (routeSecondColor != NONE) {
            printf(" ou %d", routeSecondColor);
        }
        printf("\n");
        
        // Si la route n'est pas grise, vérifier que la couleur est correcte
        if (routeColor != LOCOMOTIVE) {
            bool validColor = (color == routeColor || 
                              (routeSecondColor != NONE && color == routeSecondColor) || 
                              color == LOCOMOTIVE);
            
            if (!validColor) {
                printf("CORRECTION: Couleur invalide! Forçage à la couleur correcte\n");
                
                // Choisir la première couleur valide que nous avons en quantité suffisante
                if (state->nbCardsByColor[routeColor] >= state->routes[routeIndex].length) {
                    myMove.claimRoute.color = routeColor;
                    myMove.claimRoute.nbLocomotives = 0;
                } 
                else if (routeSecondColor != NONE && 
                        state->nbCardsByColor[routeSecondColor] >= state->routes[routeIndex].length) {
                    myMove.claimRoute.color = routeSecondColor;
                    myMove.claimRoute.nbLocomotives = 0;
                }
                else if (routeColor != NONE && 
                         state->nbCardsByColor[routeColor] + state->nbCardsByColor[LOCOMOTIVE] >= state->routes[routeIndex].length) {
                    myMove.claimRoute.color = routeColor;
                    myMove.claimRoute.nbLocomotives = state->routes[routeIndex].length - state->nbCardsByColor[routeColor];
                }
                else if (routeSecondColor != NONE && 
                         state->nbCardsByColor[routeSecondColor] + state->nbCardsByColor[LOCOMOTIVE] >= state->routes[routeIndex].length) {
                    myMove.claimRoute.color = routeSecondColor;
                    myMove.claimRoute.nbLocomotives = state->routes[routeIndex].length - state->nbCardsByColor[routeSecondColor];
                }
                else if (state->nbCardsByColor[LOCOMOTIVE] >= state->routes[routeIndex].length) {
                    myMove.claimRoute.color = LOCOMOTIVE;
                    myMove.claimRoute.nbLocomotives = state->routes[routeIndex].length;
                }
                else {
                    // Pas assez de cartes, utiliser le maximum de ce qu'on a
                    printf("ERREUR: Pas assez de cartes pour prendre cette route!\n");
                    // On pourrait changer à une autre action ici si nécessaire
                    // Par exemple, piocher une carte au lieu de prendre une route
                    myMove.action = DRAW_BLIND_CARD;
                }
            }
        }
    }
}

// APRÈS LA VÉRIFICATION, ENVOI DU COUP AU SERVEUR
// Envoie l'action
// Dernière vérification de sécurité pour les actions CLAIM_ROUTE
if (myMove.action == CLAIM_ROUTE) {
    // Vérification de validité de la couleur
    if (myMove.claimRoute.color < 1 || myMove.claimRoute.color > 9) {
        printf("ERREUR CRITIQUE FINALE: Couleur invalide %d, correction à 6 (BLACK)\n", myMove.claimRoute.color);
        myMove.claimRoute.color = 6;  // BLACK est généralement 6
    }
}
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
// Joue un tour normal
ResultCode playTurn(GameState* state, StrategyType strategy) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult = {0};
    BoardState boardState;
    
    // Variable statique pour suivre si une carte a déjà été piochée ce tour-ci
    static int cardDrawnThisTurn = 0;
    
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
        printf("Second card draw this turn\n");
        
        // Pour la seconde carte, on ne peut pas prendre de locomotive visible
        // Vérifier s'il y a des cartes visibles non-locomotive
        CardColor cardToDraw = (CardColor)-1;
        for (int i = 0; i < 5; i++) {
            if (state->visibleCards[i] != LOCOMOTIVE && state->visibleCards[i] != NONE) {
                cardToDraw = state->visibleCards[i];
                break;
            }
        }
        
        if (cardToDraw != (CardColor)-1) {
            myMove.action = DRAW_CARD;
            myMove.drawCard = cardToDraw;
            printf("Drawing second visible card: %d\n", cardToDraw);
        } else {
            myMove.action = DRAW_BLIND_CARD;
            printf("Drawing second blind card\n");
        }
        
        cardDrawnThisTurn = 0;
    } else {
        // Début de tour normal - toutes les options sont disponibles
        if (!decideNextMove(state, strategy, &myMove)) {
            printf("No specific move decided, drawing blind card\n");
            myMove.action = DRAW_BLIND_CARD;
        }
    }
    
    // VÉRIFICATION DE SÉCURITÉ POUR LA VALIDITÉ DE LA COULEUR
    if (myMove.action == CLAIM_ROUTE) {
        CardColor color = myMove.claimRoute.color;
        
        // Vérifier que la couleur est dans la plage valide (1-9)
        if (color < PURPLE || color > LOCOMOTIVE) {
            printf("ERREUR CRITIQUE: Couleur invalide détectée: %d, correction à GREEN (8)\n", color);
            myMove.claimRoute.color = GREEN; // Utiliser GREEN comme couleur par défaut
        }
        
        // Vérification supplémentaire: s'assurer que nous avons assez de cartes de cette couleur
        int routeIndex = findRouteIndex(state, myMove.claimRoute.from, myMove.claimRoute.to);
        if (routeIndex >= 0) {
            int length = state->routes[routeIndex].length;
            CardColor routeColor = state->routes[routeIndex].color;
            
            // Pour les routes colorées, vérifier que la couleur est valide
            if (routeColor != LOCOMOTIVE && color != routeColor && 
                color != state->routes[routeIndex].secondColor && color != LOCOMOTIVE) {
                
                printf("CORRECTION: Couleur incorrecte pour route %d->%d (couleur choisie: %d, route: %d)\n", 
                      myMove.claimRoute.from, myMove.claimRoute.to, color, routeColor);
                
                // Adapter la couleur à celle de la route
                if (state->nbCardsByColor[routeColor] >= length) {
                    myMove.claimRoute.color = routeColor;
                    myMove.claimRoute.nbLocomotives = 0;
                } else if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
                    myMove.claimRoute.color = LOCOMOTIVE;
                    myMove.claimRoute.nbLocomotives = length;
                } else if (state->nbCardsByColor[routeColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
                    myMove.claimRoute.color = routeColor;
                    myMove.claimRoute.nbLocomotives = length - state->nbCardsByColor[routeColor];
                } else {
                    // Si on ne peut pas prendre la route, changer d'action
                    printf("ERREUR CRITIQUE: Pas assez de cartes pour cette route! Piochage à la place.\n");
                    myMove.action = DRAW_BLIND_CARD;
                }
            }
        }
    }
    
    // Affiche l'action choisie
    printf("Action: ");
    switch (myMove.action) {
        case CLAIM_ROUTE:
            printf("Claim route %d -> %d (color %d, locos %d)\n", 
                   myMove.claimRoute.from, myMove.claimRoute.to, 
                   myMove.claimRoute.color, myMove.claimRoute.nbLocomotives);
            break;
        case DRAW_CARD:
            printf("Draw visible card %d\n", myMove.drawCard);
            break;
        case DRAW_BLIND_CARD:
            printf("Draw blind card\n");
            break;
        case DRAW_OBJECTIVES:
            printf("Draw objectives\n");
            break;
        default:
            printf("Unknown action %d\n", myMove.action);
    }
    // VÉRIFICATION ULTIME: s'assurer que l'action est cohérente
if (myMove.action == CLAIM_ROUTE) {
    // Vérifier que from/to sont valides
    if (myMove.claimRoute.from < 0 || myMove.claimRoute.from >= state->nbCities || 
        myMove.claimRoute.to < 0 || myMove.claimRoute.to >= state->nbCities) {
        printf("ERREUR FATALE: Tentative de prendre une route avec villes invalides: %d -> %d\n", 
              myMove.claimRoute.from, myMove.claimRoute.to);
        myMove.action = DRAW_BLIND_CARD;
    }
    
    // Vérifier que la route existe et est disponible
    else {
        bool routeFound = false;
        for (int i = 0; i < state->nbTracks; i++) {
            if ((state->routes[i].from == myMove.claimRoute.from && 
                 state->routes[i].to == myMove.claimRoute.to) ||
                (state->routes[i].from == myMove.claimRoute.to && 
                 state->routes[i].to == myMove.claimRoute.from)) {
                
                routeFound = true;
                
                // Vérifier que la route est libre
                if (state->routes[i].owner != 0) {
                    printf("ERREUR FATALE: Tentative de prendre une route déjà prise: %d -> %d\n", 
                          myMove.claimRoute.from, myMove.claimRoute.to);
                    myMove.action = DRAW_BLIND_CARD;
                }
                break;
            }
        }
        
        if (!routeFound) {
            printf("ERREUR FATALE: Route inexistante: %d -> %d\n", 
                  myMove.claimRoute.from, myMove.claimRoute.to);
            myMove.action = DRAW_BLIND_CARD;
        }
    }
    
    // Vérifier que la couleur est valide
    if (myMove.claimRoute.color < PURPLE || myMove.claimRoute.color > LOCOMOTIVE) {
        printf("ERREUR FATALE: Couleur invalide: %d\n", myMove.claimRoute.color);
        myMove.action = DRAW_BLIND_CARD;
    }
}
    // Envoie l'action
    returnCode = sendMove(&myMove, &myMoveResult);
    
    // Vérifier si c'est la fin du jeu
    if (returnCode == SERVER_ERROR || returnCode == PARAM_ERROR) {
        printf("Game end or error: 0x%x\n", returnCode);
        
        // Détecter la fin de partie dans le message
        if (myMoveResult.message && (
            strstr(myMoveResult.message, "Total score") || 
            strstr(myMoveResult.message, "winner") ||
            strstr(myMoveResult.message, "Final Score"))) {
            
            printf("\n==================================================\n");
            printf("           RÉSULTAT FINAL DÉTECTÉ                 \n");
            printf("==================================================\n");
            printf("%s\n", myMoveResult.message);
            printf("==================================================\n\n");
            
            // Noter que le jeu est terminé
            state->lastTurn = 2;  // 2 = jeu terminé complètement
        }
        
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    if (returnCode != ALL_GOOD) {
        printf("Error sending move: 0x%x\n", returnCode);
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    // Traite le résultat de l'action
    switch (myMove.action) {
        case CLAIM_ROUTE:
            if (myMoveResult.state == NORMAL_MOVE) {
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
                printf("Route claimed successfully\n");
            } else {
                printf("WARNING: CLAIM_ROUTE not confirmed by server, state: %d\n", myMoveResult.state);
            }
            cardDrawnThisTurn = 0;
            break;
            
        case DRAW_CARD:
            // Ajoute la carte piochée à notre main
            addCardToHand(state, myMove.drawCard);
            printf("Card drawn successfully\n");
            
            // Si c'est une locomotive visible, le tour est terminé
            if (myMove.drawCard == LOCOMOTIVE) {
                printf("Drew a locomotive - turn ends\n");
                cardDrawnThisTurn = 0;
            } else {
                // Sinon, on peut piocher une seconde carte
                if (cardDrawnThisTurn == 0) {
                    printf("Drew non-locomotive card - can draw second card\n");
                    cardDrawnThisTurn = myMoveResult.replay ? 1 : 0;
                } else {
                    printf("Drew second card - turn ends\n");
                    cardDrawnThisTurn = 0;
                }
            }
            break;
            
        case DRAW_BLIND_CARD:
            // Ajoute la carte piochée à notre main
            addCardToHand(state, myMoveResult.card);
            printf("Drew blind card: %d\n", myMoveResult.card);
            
            // Gestion du tour en fonction de la réponse du serveur
            if (myMoveResult.card == LOCOMOTIVE && !myMoveResult.replay) {
                printf("Blind card is a locomotive that doesn't allow second draw\n");
                cardDrawnThisTurn = 0;
            } else {
                if (cardDrawnThisTurn == 0 && myMoveResult.replay) {
                    printf("Can draw a second card\n");
                    cardDrawnThisTurn = 1;
                } else {
                    printf("Turn ends\n");
                    cardDrawnThisTurn = 0;
                }
            }
            break;
            
        case DRAW_OBJECTIVES:
            printf("Received objectives to choose from\n");
            
            // Choisit quels objectifs garder
            bool chooseObjectives[3] = {true, true, true};
            chooseObjectivesStrategy(state, myMoveResult.objectives, chooseObjectives);
            
            // Prépare la réponse
            MoveData chooseMove;
            MoveResult chooseMoveResult = {0};
            
            chooseMove.action = CHOOSE_OBJECTIVES;
            chooseMove.chooseObjectives[0] = chooseObjectives[0];
            chooseMove.chooseObjectives[1] = chooseObjectives[1];
            chooseMove.chooseObjectives[2] = chooseObjectives[2];
            
            // Compte combien d'objectifs nous gardons
            int objectivesToKeep = 0;
            Objective chosenObjectives[3];
            int idx = 0;
            for (int i = 0; i < 3; i++) {
                if (chooseObjectives[i]) {
                    objectivesToKeep++;
                    chosenObjectives[idx++] = myMoveResult.objectives[i];
                }
            }
            
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
            
            printf("Chose %d objectives\n", objectivesToKeep);
            cleanupMoveResult(&chooseMoveResult);
            
            cardDrawnThisTurn = 0;
            break;
    }
    
    // Libère la mémoire si ce n'est pas déjà fait
    if (myMove.action != DRAW_OBJECTIVES) {
        cleanupMoveResult(&myMoveResult);
    }
    
    // Met à jour la connectivité des villes après notre action
    updateCityConnectivity(state);
    
    // Si on doit piocher une seconde carte
    if (cardDrawnThisTurn == 1) {
        printf("\nNow drawing second card\n");
        
        // Mettre à jour l'état du plateau avant de piocher
        ResultCode updateResult = getBoardState(&boardState);
        if (updateResult != ALL_GOOD) {
            printf("Error getting board state for second card: 0x%x\n", updateResult);
            return updateResult;
        }
        
        // Mettre à jour les cartes visibles
        for (int i = 0; i < 5; i++) {
            state->visibleCards[i] = boardState.card[i];
        }
        
        // Préparer la seconde pioche
        MoveData secondCardMove;
        MoveResult secondCardResult = {0};
        
        // Pour la seconde carte, pas de locomotive visible
        CardColor cardToDraw = (CardColor)-1;
        for (int i = 0; i < 5; i++) {
            if (state->visibleCards[i] != LOCOMOTIVE && state->visibleCards[i] != NONE) {
                cardToDraw = state->visibleCards[i];
                break;
            }
        }
        
        if (cardToDraw != (CardColor)-1) {
            secondCardMove.action = DRAW_CARD;
            secondCardMove.drawCard = cardToDraw;
            printf("Drawing second visible card: %d\n", cardToDraw);
        } else {
            secondCardMove.action = DRAW_BLIND_CARD;
            printf("Drawing second blind card\n");
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
            printf("Second visible card drawn\n");
        } else {
            addCardToHand(state, secondCardResult.card);
            printf("Second blind card drawn: %d\n", secondCardResult.card);
        }
        
        cardDrawnThisTurn = 0;
        cleanupMoveResult(&secondCardResult);
    }
    
    return ALL_GOOD;
}