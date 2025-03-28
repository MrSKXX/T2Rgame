#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "manual.h"
#include "gamestate.h"
#include "rules.h"

// Variable pour suivre si nous avons déjà pioché une carte ce tour-ci
static int cardDrawnThisTurn = 0;  // 0 = début de tour, 1 = déjà pioché une carte

// Affiche le nom d'une carte couleur (pour éviter le conflit avec player.c)
void printCardNameManual(CardColor card) {
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    if (card >= 0 && card < 10) {
        printf("%s", cardNames[card]);
    } else {
        printf("Unknown card: %d", card);
    }
}

// Obtient le nom d'une carte couleur sous forme de chaîne
const char* getCardColorName(CardColor card) {
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    if (card >= 0 && card < 10) {
        return cardNames[card];
    } else {
        return "Unknown";
    }
}

// Affiche les informations d'un objectif
void printObjectiveDetails(Objective objective) {
    printf("De ville %d à ville %d, score %d\n", 
           objective.from, objective.to, objective.score);
    printf("  De: ");
    printCity(objective.from);
    printf(" à ");
    printCity(objective.to);
    printf("\n");
}

// Affiche l'état actuel du joueur
void printManualGameState(GameState* state) {
    printf("\n=== ÉTAT DU JOUEUR ===\n");
    printf("Cartes en main (%d):\n", state->nbCards);
    
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    
    for (int i = 1; i < 10; i++) {  // Commencer à 1 pour ignorer NONE
        if (state->nbCardsByColor[i] > 0) {
            printf("  %s: %d\n", cardNames[i], state->nbCardsByColor[i]);
        }
    }
    
    printf("Objectifs (%d):\n", state->nbObjectives);
    for (int i = 0; i < state->nbObjectives; i++) {
        printf("  %d. De ville %d (", i+1, state->objectives[i].from);
        printCity(state->objectives[i].from);
        printf(") à ville %d (", state->objectives[i].to);
        printCity(state->objectives[i].to);
        printf("), score %d", state->objectives[i].score);
        
        if (state->cityConnected[state->objectives[i].from][state->objectives[i].to]) {
            printf(" [COMPLÉTÉ]");
        }
        printf("\n");
    }
    
    printf("Nombre de wagons restants: %d\n", state->wagonsLeft);
    printf("Adversaire: %d wagons, %d cartes, %d objectifs\n", 
           state->opponentWagonsLeft, state->opponentCardCount, state->opponentObjectiveCount);
    printf("======================\n\n");
}

// Affiche les routes disponibles
void printAvailableRoutes(GameState* state) {
    printf("\n=== ROUTES DISPONIBLES ===\n");
    
    // Parcourir toutes les routes pour trouver celles disponibles
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {  // Si la route n'a pas de propriétaire
            printf("%d: ", i);
            printf("Ville %d (", state->routes[i].from);
            printCity(state->routes[i].from);
            printf(") à Ville %d (", state->routes[i].to);
            printCity(state->routes[i].to);
            printf("), longueur %d, couleur ", state->routes[i].length);
            printCardNameManual(state->routes[i].color);
            if (state->routes[i].secondColor != NONE) {
                printf(" ou ");
                printCardNameManual(state->routes[i].secondColor);
            }
            printf("\n");
        }
    }
    printf("===========================\n");
}

// Affiche les cartes visibles sur le plateau
void printVisibleCards(CardColor* visibleCards) {
    printf("\n=== CARTES VISIBLES ===\n");
    for (int i = 0; i < 5; i++) {
        if (visibleCards[i] != NONE) {
            printf("Position %d: ", i+1);
            printCardName(visibleCards[i]);
            printf("\n");
        }
    }
    printf("======================\n\n");
}

// Fonction pour prendre une route en mode manuel
MoveData claimRouteManual(GameState* state) {
    MoveData move;
    move.action = CLAIM_ROUTE;
    
    printf("\n=== PRISE DE ROUTE MANUELLE ===\n");
    
    // Afficher les cartes en main du joueur
    printf("Cartes en main:\n");
    for (int i = 1; i < 10; i++) {
        if (state->nbCardsByColor[i] > 0) {
            printf("  %s: %d\n", getCardColorName(i), state->nbCardsByColor[i]);
        }
    }
    
    // Afficher les routes disponibles
    printAvailableRoutes(state);
    
    int choiceMethod;
    printf("\nComment souhaitez-vous sélectionner la route?\n");
    printf("1. Par index (numéro de la route)\n");
    printf("2. Par villes (numéro des villes de départ et d'arrivée)\n");
    printf("Votre choix: ");
    scanf("%d", &choiceMethod);
    
    int routeIndex = -1;
    
    if (choiceMethod == 1) {
        // Sélection par index
        printf("\nEntrez l'index de la route à prendre: ");
        scanf("%d", &routeIndex);
        
        // Vérifier que l'index est valide
        if (routeIndex < 0 || routeIndex >= state->nbTracks || state->routes[routeIndex].owner != 0) {
            printf("ERREUR: Route invalide ou déjà prise.\n");
            move.action = DRAW_BLIND_CARD;  // Action par défaut en cas d'erreur
            return move;
        }
    } else {
        // Sélection par villes
        int fromCity, toCity;
        printf("\nEntrez le numéro de la ville de départ: ");
        scanf("%d", &fromCity);
        printf("Entrez le numéro de la ville d'arrivée: ");
        scanf("%d", &toCity);
        
        // Rechercher la route correspondante
        for (int i = 0; i < state->nbTracks; i++) {
            if (((state->routes[i].from == fromCity && state->routes[i].to == toCity) || 
                (state->routes[i].from == toCity && state->routes[i].to == fromCity)) && 
                state->routes[i].owner == 0) {
                routeIndex = i;
                break;
            }
        }
        
        if (routeIndex == -1) {
            printf("ERREUR: Route entre ville %d et ville %d non trouvée ou déjà prise.\n", fromCity, toCity);
            move.action = DRAW_BLIND_CARD;  // Action par défaut en cas d'erreur
            return move;
        }
    }
    
    // Récupérer les informations de la route
    move.claimRoute.from = state->routes[routeIndex].from;
    move.claimRoute.to = state->routes[routeIndex].to;
    int routeLength = state->routes[routeIndex].length;
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;
    
    // Afficher les détails de la route
    printf("\nDétails de la route:\n");
    printf("  De ville %d (", move.claimRoute.from);
    printCity(move.claimRoute.from);
    printf(") à ville %d (", move.claimRoute.to);
    printCity(move.claimRoute.to);
    printf(")\n  Longueur: %d\n", routeLength);
    printf("  Couleur requise: ");
    printCardNameManual(routeColor);
    if (routeSecondColor != NONE) {
        printf(" ou ");
        printCardNameManual(routeSecondColor);
    }
    printf("\n");
    
    // Choisir quelle couleur utiliser
    printf("\nCouleurs disponibles:\n");
    for (int i = 1; i < 10; i++) {
        if (state->nbCardsByColor[i] > 0) {
            printf("%d: %s (%d cartes)\n", i, getCardColorName(i), state->nbCardsByColor[i]);
        }
    }
    
    printf("\nEntrez la couleur à utiliser (numéro): ");
    int colorChoice;
    scanf("%d", &colorChoice);
    
    if (colorChoice < 1 || colorChoice > 9 || state->nbCardsByColor[colorChoice] == 0) {
        printf("ERREUR: Couleur invalide ou pas assez de cartes.\n");
        move.action = DRAW_BLIND_CARD;  // Action par défaut en cas d'erreur
        return move;
    }
    
    move.claimRoute.color = (CardColor)colorChoice;
    
    // Demander le nombre de locomotives à utiliser
    printf("\nLongueur de la route: %d\n", routeLength);
    printf("Couleur choisie: %s (Vous avez %d cartes)\n", 
           getCardColorName(move.claimRoute.color), 
           state->nbCardsByColor[move.claimRoute.color]);
    printf("Locomotives disponibles: %d cartes\n", state->nbCardsByColor[LOCOMOTIVE]);
    
    int maxLocomotives = (routeLength < state->nbCardsByColor[LOCOMOTIVE]) ? 
                          routeLength : state->nbCardsByColor[LOCOMOTIVE];
    
    printf("Nombre de locomotives à utiliser (max %d): ", maxLocomotives);
    scanf("%d", &move.claimRoute.nbLocomotives);
    
    if (move.claimRoute.nbLocomotives < 0 || move.claimRoute.nbLocomotives > state->nbCardsByColor[LOCOMOTIVE]) {
        printf("ERREUR: Nombre de locomotives invalide.\n");
        move.action = DRAW_BLIND_CARD;  // Action par défaut en cas d'erreur
        return move;
    }
    
    if (move.claimRoute.nbLocomotives > routeLength) {
        printf("ERREUR: Trop de locomotives pour cette route (longueur %d).\n", routeLength);
        move.action = DRAW_BLIND_CARD;  // Action par défaut en cas d'erreur
        return move;
    }
    
    // Vérifier si on a assez de cartes pour prendre la route
    int colorCards = state->nbCardsByColor[move.claimRoute.color];
    int requiredColorCards = routeLength - move.claimRoute.nbLocomotives;
    
    printf("Vérification des cartes: %d cartes %s requises + %d locomotives = %d cartes au total\n",
           requiredColorCards, getCardColorName(move.claimRoute.color), 
           move.claimRoute.nbLocomotives, routeLength);
    
    if (colorCards < requiredColorCards) {
        printf("ERREUR: Pas assez de cartes %s. Besoin de %d, vous en avez %d.\n", 
               getCardColorName(move.claimRoute.color), requiredColorCards, colorCards);
        move.action = DRAW_BLIND_CARD;  // Action par défaut en cas d'erreur
        return move;
    }
    
    // Vérifier si la couleur est valide pour cette route
    if (routeColor != move.claimRoute.color && routeSecondColor != move.claimRoute.color && 
        move.claimRoute.color != LOCOMOTIVE) {
        printf("ERREUR: La couleur %s n'est pas valide pour cette route.\n", 
               getCardColorName(move.claimRoute.color));
        move.action = DRAW_BLIND_CARD;  // Action par défaut en cas d'erreur
        return move;
    }
    
    // Confirmation finale
    printf("\nRécapitulatif de la route à prendre:\n");
    printf("De ville %d (", move.claimRoute.from);
    printCity(move.claimRoute.from);
    printf(") à ville %d (", move.claimRoute.to);
    printCity(move.claimRoute.to);
    printf(")\nCouleur: %s\n", getCardColorName(move.claimRoute.color));
    printf("Nombre de locomotives: %d\n", move.claimRoute.nbLocomotives);
    printf("Longueur de la route: %d\n", routeLength);
    printf("Cartes à utiliser: %d %s + %d locomotives\n", 
           routeLength - move.claimRoute.nbLocomotives,
           getCardColorName(move.claimRoute.color), 
           move.claimRoute.nbLocomotives);
    
    printf("\nConfirmer? (1: Oui, 0: Non): ");
    int confirm;
    scanf("%d", &confirm);
    
    if (!confirm) {
        printf("Opération annulée.\n");
        move.action = DRAW_BLIND_CARD;  // Action par défaut en cas d'annulation
    } else {
        // Stocker l'index de la route pour référence future
        // (nous l'utiliserons pour la mise à jour dans playManualTurn)
        printf("Route confirmée: Index %d, Longueur %d\n", routeIndex, routeLength);
    }
    
    // Vider le buffer d'entrée
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    
    return move;
}

// Fonction pour obtenir un coup manuel de l'utilisateur
MoveData getManualMove(GameState* state) {
    MoveData move;
    int choice;
    
    // Si nous avons déjà pioché une carte (2ème carte du tour)
    if (cardDrawnThisTurn == 1) {
        printf("\n=== DEUXIÈME CARTE ===\n");
        printf("1. Piocher une carte aveugle\n");
        printf("2. Piocher une carte visible (sauf locomotive)\n");
        
        printf("Votre choix: ");
        if (scanf("%d", &choice) != 1 || (choice != 1 && choice != 2)) {
            choice = 1;
        }
        
        // Vider le buffer d'entrée
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        
        if (choice == 1) {
            move.action = DRAW_BLIND_CARD;
            cardDrawnThisTurn = 0;  // Réinitialiser pour le prochain tour
            return move;
        } else {
            // Afficher les cartes visibles non-locomotive
            printf("Cartes visibles:\n");
            int validCards = 0;
            for (int i = 0; i < 5; i++) {
                if (state->visibleCards[i] != NONE && state->visibleCards[i] != LOCOMOTIVE) {
                    printf("%d: ", i+1);
                    printCardNameManual(state->visibleCards[i]);
                    printf("\n");
                    validCards++;
                }
            }
            
            if (validCards == 0) {
                printf("Aucune carte visible non-locomotive disponible. Pioche aveugle par défaut.\n");
                move.action = DRAW_BLIND_CARD;
                cardDrawnThisTurn = 0;
                return move;
            }
            
            printf("Quelle carte piocher (1-5): ");
            int cardIndex;
            if (scanf("%d", &cardIndex) != 1 || cardIndex < 1 || cardIndex > 5 || 
                state->visibleCards[cardIndex-1] == NONE || 
                state->visibleCards[cardIndex-1] == LOCOMOTIVE) {
                move.action = DRAW_BLIND_CARD;
            } else {
                move.action = DRAW_CARD;
                move.drawCard = state->visibleCards[cardIndex-1];
            }
            
            // Vider le buffer d'entrée
            while ((c = getchar()) != '\n' && c != EOF);
            
            cardDrawnThisTurn = 0;  // Réinitialiser pour le prochain tour
            return move;
        }
    }
    
    // Début de tour - Choix libre
    printf("\n=== MODE MANUEL ===\n");
    printf("1. Piocher des cartes\n");
    printf("2. Prendre une route\n");
    printf("3. Piocher des objectifs\n");
    
    printf("Votre choix: ");
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > 3) {
        choice = 1;
    }
    
    // Vider le buffer d'entrée
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    
    switch (choice) {
        case 1: // Piocher des cartes
            printf("Comment piocher la première carte?\n");
            printf("1. Carte aveugle\n");
            printf("2. Carte visible\n");
            
            printf("Votre choix: ");
            int pickChoice;
            if (scanf("%d", &pickChoice) != 1 || (pickChoice != 1 && pickChoice != 2)) {
                pickChoice = 1;
            }
            
            // Vider le buffer d'entrée
            while ((c = getchar()) != '\n' && c != EOF);
            
            if (pickChoice == 1) {
                move.action = DRAW_BLIND_CARD;
                cardDrawnThisTurn = 1;  // Pour piocher une deuxième carte au prochain tour
                return move;
            } else {
                // Afficher les cartes visibles
                printf("Cartes visibles:\n");
                for (int i = 0; i < 5; i++) {
                    if (state->visibleCards[i] != NONE) {
                        printf("%d: ", i+1);
                        printCardName(state->visibleCards[i]);
                        printf("\n");
                    }
                }
                
                printf("Quelle carte piocher (1-5): ");
                int cardIndex;
                if (scanf("%d", &cardIndex) != 1 || cardIndex < 1 || cardIndex > 5 || 
                    state->visibleCards[cardIndex-1] == NONE) {
                    printf("Choix invalide, pioche aveugle par défaut.\n");
                    move.action = DRAW_BLIND_CARD;
                    cardDrawnThisTurn = 1;
                } else {
                    move.action = DRAW_CARD;
                    move.drawCard = state->visibleCards[cardIndex-1];
                    
                    // Si c'est une locomotive, on ne peut pas piocher de deuxième carte
                    if (state->visibleCards[cardIndex-1] == LOCOMOTIVE) {
                        printf("Locomotive piochée, pas de deuxième carte.\n");
                        cardDrawnThisTurn = 0;
                    } else {
                        cardDrawnThisTurn = 1;
                    }
                }
                
                // Vider le buffer d'entrée
                while ((c = getchar()) != '\n' && c != EOF);
                
                return move;
            }
            break;
            
        case 2: // Prendre une route
            return claimRouteManual(state);
            
        case 3: // Piocher des objectifs
            move.action = DRAW_OBJECTIVES;
            cardDrawnThisTurn = 0;
            return move;
            
        default:
            printf("Choix invalide, pioche d'une carte aveugle par défaut.\n");
            move.action = DRAW_BLIND_CARD;
            cardDrawnThisTurn = 1;
    }
    
    return move;
}

// Fonction pour choisir des objectifs en mode manuel
void chooseObjectivesManual(GameState* state, Objective* objectives, bool* choices) {
    printf("\n=== CHOIX DES OBJECTIFS ===\n");
    printf("Vous devez choisir au moins 1 objectif parmi les 3 suivants:\n");
    
    for (int i = 0; i < 3; i++) {
        printf("%d: ", i+1);
        printf("De ville %d (", objectives[i].from);
        printCity(objectives[i].from);
        printf(") à ville %d (", objectives[i].to);
        printCity(objectives[i].to);
        printf("), score %d\n", objectives[i].score);
    }
    
    // Initialiser les choix
    for (int i = 0; i < 3; i++) {
        choices[i] = false;
    }
    
    printf("Entrez les numéros des objectifs à garder (séparés par des espaces): ");
    int choice;
    bool hasChosen = false;
    
    // Lire les choix
    for (int i = 0; i < 3; i++) {
        if (scanf("%d", &choice) != 1) {
            break;
        }
        
        if (choice >= 1 && choice <= 3) {
            choices[choice-1] = true;
            hasChosen = true;
        }
    }
    
    // Si aucun choix, sélectionner le premier par défaut
    if (!hasChosen) {
        printf("Vous devez choisir au moins un objectif. Premier objectif sélectionné par défaut.\n");
        choices[0] = true;
    }
    
    printf("Objectifs choisis: ");
    for (int i = 0; i < 3; i++) {
        if (choices[i]) {
            printf("%d ", i+1);
        }
    }
    printf("\n");
    
    // Vider le buffer d'entrée
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Fonction pour jouer un tour en mode manuel
ResultCode playManualTurn(GameState* state) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult;
    BoardState boardState;
    
    // Récupère l'état du plateau (cartes visibles)
    returnCode = getBoardState(&boardState);
    if (returnCode != ALL_GOOD) {
        printf("Erreur lors de la récupération de l'état du plateau: 0x%x\n", returnCode);
        return returnCode;
    }
    
    // Met à jour les cartes visibles dans notre état
    for (int i = 0; i < 5; i++) {
        state->visibleCards[i] = boardState.card[i];
    }
    
    // Afficher l'état actuel du jeu
    printManualGameState(state);
    
    // Afficher les cartes visibles
    printVisibleCards(state->visibleCards);
    
    // Demander à l'utilisateur quelle action effectuer
    myMove = getManualMove(state);
    
    // Cas spécial pour la pioche d'objectifs
    if (myMove.action == DRAW_OBJECTIVES) {
        returnCode = sendMove(&myMove, &myMoveResult);
        
        if (returnCode != ALL_GOOD) {
            printf("Erreur lors de la pioche d'objectifs: 0x%x\n", returnCode);
            if (myMoveResult.message) {
                printf("Message du serveur: %s\n", myMoveResult.message);
            }
            cleanupMoveResult(&myMoveResult);
            return returnCode;
        }
        
        printf("Objectifs piochés:\n");
        for (int i = 0; i < 3; i++) {
            printf("  Objectif %d: ", i+1);
            printObjectiveDetails(myMoveResult.objectives[i]);
        }
        
        // Choisir quels objectifs garder
        bool chooseObjectives[3];
        chooseObjectivesManual(state, myMoveResult.objectives, chooseObjectives);
        
        // Préparer la réponse pour le choix des objectifs
        MoveData chooseMove;
        MoveResult chooseMoveResult;
        
        chooseMove.action = CHOOSE_OBJECTIVES;
        chooseMove.chooseObjectives[0] = chooseObjectives[0];
        chooseMove.chooseObjectives[1] = chooseObjectives[1];
        chooseMove.chooseObjectives[2] = chooseObjectives[2];
        
        // Libérer la mémoire du premier résultat
        cleanupMoveResult(&myMoveResult);
        
        // Envoyer le choix des objectifs
        returnCode = sendMove(&chooseMove, &chooseMoveResult);
        
        if (returnCode != ALL_GOOD) {
            printf("Erreur lors du choix des objectifs: 0x%x\n", returnCode);
            if (chooseMoveResult.message) {
                printf("Message du serveur: %s\n", chooseMoveResult.message);
            }
            cleanupMoveResult(&chooseMoveResult);
            return returnCode;
        }
        
        // Calculer combien d'objectifs nous gardons
        int objectivesToKeep = 0;
        for (int i = 0; i < 3; i++) {
            if (chooseObjectives[i]) {
                objectivesToKeep++;
            }
        }
        
        // Créer un tableau des objectifs choisis
        Objective chosenObjectives[3];
        int idx = 0;
        for (int i = 0; i < 3; i++) {
            if (chooseObjectives[i]) {
                chosenObjectives[idx++] = myMoveResult.objectives[i];
            }
        }
        
        // Ajouter les objectifs à notre état
        addObjectives(state, chosenObjectives, objectivesToKeep);
        
        printf("Objectifs choisis avec succès!\n");
        cleanupMoveResult(&chooseMoveResult);
        
        return ALL_GOOD;
    }
    
    // Pour les autres types d'actions
    returnCode = sendMove(&myMove, &myMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Erreur lors de l'envoi du coup: 0x%x\n", returnCode);
        if (myMoveResult.message) {
            printf("Message du serveur: %s\n", myMoveResult.message);
        }
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    // Traiter le résultat en fonction du type d'action
    switch (myMove.action) {
        case CLAIM_ROUTE:
    // Utilisez exactement la même séquence que l'IA
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
    
    printf("Route prise avec succès!\n");
    break;
            
        case DRAW_CARD:
            // Ajouter la carte visible à notre main
            addCardToHand(state, myMove.drawCard);
            printf("Carte visible piochée: ");
            printCardNameManual(myMove.drawCard);
            printf("\n");
            break;
            
        case DRAW_BLIND_CARD:
            // Ajouter la carte piochée à notre main
            addCardToHand(state, myMoveResult.card);
            printf("Carte aveugle piochée: ");
            printCardNameManual(myMoveResult.card);
            printf("\n");
            break;
    }
    
    // Mettre à jour la connectivité des villes
    updateCityConnectivity(state);
    
    // Libérer la mémoire
    cleanupMoveResult(&myMoveResult);
    
    return ALL_GOOD;
}

// Fonction pour jouer le premier tour en mode manuel
ResultCode playManualFirstTurn(GameState* state) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult;
    
    printf("Premier tour: pioche des objectifs\n");
    
    myMove.action = DRAW_OBJECTIVES;
    
    // Envoyer la requête
    returnCode = sendMove(&myMove, &myMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Erreur lors de la pioche d'objectifs: 0x%x\n", returnCode);
        if (myMoveResult.message) {
            printf("Message du serveur: %s\n", myMoveResult.message);
        }
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    printf("Objectifs reçus:\n");
    for (int i = 0; i < 3; i++) {
        printf("  Objectif %d: ", i+1);
        printObjectiveDetails(myMoveResult.objectives[i]);
    }
    
    // Choisir quels objectifs garder
    bool chooseObjectives[3];
    chooseObjectivesManual(state, myMoveResult.objectives, chooseObjectives);
    
    // Préparer la réponse pour le choix des objectifs
    MoveData chooseMove;
    MoveResult chooseMoveResult;
    
    chooseMove.action = CHOOSE_OBJECTIVES;
    chooseMove.chooseObjectives[0] = chooseObjectives[0];
    chooseMove.chooseObjectives[1] = chooseObjectives[1];
    chooseMove.chooseObjectives[2] = chooseObjectives[2];
    
    // Calculer combien d'objectifs nous gardons
    int objectivesToKeep = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            objectivesToKeep++;
        }
    }
    
    // Créer un tableau des objectifs choisis
    Objective chosenObjectives[3];
    int idx = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            chosenObjectives[idx++] = myMoveResult.objectives[i];
        }
    }
    
    // Libérer la mémoire du premier résultat
    cleanupMoveResult(&myMoveResult);
    
    // Envoyer le choix des objectifs
    returnCode = sendMove(&chooseMove, &chooseMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Erreur lors du choix des objectifs: 0x%x\n", returnCode);
        if (chooseMoveResult.message) {
            printf("Message du serveur: %s\n", chooseMoveResult.message);
        }
        cleanupMoveResult(&chooseMoveResult);
        return returnCode;
    }
    
    // Ajouter les objectifs à notre état
    addObjectives(state, chosenObjectives, objectivesToKeep);
    
    printf("Objectifs choisis avec succès!\n");
    cleanupMoveResult(&chooseMoveResult);
    
    return ALL_GOOD;
}

// Fonction pour déterminer si un message indique la fin de la partie
int isGameOverMessage(const char* message) {
    if (!message) return 0;
    return strstr(message, "winner") != NULL;
}