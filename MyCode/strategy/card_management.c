/**
 * card_management.c
 * Gestion des cartes et stratégies de pioche 
 */
#include <stdio.h>
#include <stdlib.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  

// Pioche stratégique de cartes basée sur les besoins actuels
int strategicCardDrawing(GameState* state) {
    if (!state) {
        printf("ERROR: NULL state in strategicCardDrawing\n");
        return -1;
    }
    
    // Toujours prendre une locomotive visible
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            return i;
        }
    }
    
    // Analyser nos besoins urgents
    int urgentNeeds[10] = {0};
    int totalNeeds[10] = {0};
    
    // Pour chaque objectif non complété
    for (int obj = 0; obj < state->nbObjectives; obj++) {
        if (isObjectiveCompleted(state, state->objectives[obj])) {
            continue;
        }
        
        int objFrom = state->objectives[obj].from;
        int objTo = state->objectives[obj].to;
        
        if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
            continue;
        }
        
        int path[MAX_CITIES];
        int pathLength = 0;
        
        if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
            // Analyser chaque segment du chemin
            for (int i = 0; i < pathLength - 1; i++) {
                int cityA = path[i];
                int cityB = path[i + 1];
                
                // Trouver la route correspondante
                for (int r = 0; r < state->nbTracks; r++) {
                    if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                         (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                        state->routes[r].owner == 0) {
                        
                        CardColor routeColor = state->routes[r].color;
                        int routeLength = state->routes[r].length;
                        
                        if (routeColor == LOCOMOTIVE) {
                            // Route grise - n'importe quelle couleur
                            totalNeeds[LOCOMOTIVE] += 1;
                            for (int c = 1; c < 9; c++) {
                                totalNeeds[c] += routeLength / 8;
                            }
                        } else {
                            // Route colorée spécifique
                            totalNeeds[routeColor] += routeLength;
                            
                            // Urgent si on peut presque la prendre
                            int available = state->nbCardsByColor[routeColor] + 
                                           state->nbCardsByColor[LOCOMOTIVE];
                            
                            if (available >= routeLength - 2 && available < routeLength) {
                                urgentNeeds[routeColor] += (routeLength - available);
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Chercher les besoins urgents dans les cartes visibles
    int bestUrgentCard = -1;
    int maxUrgency = 0;
    
    for (int i = 0; i < 5; i++) {
        CardColor card = state->visibleCards[i];
        if (card != NONE && card != LOCOMOTIVE && urgentNeeds[card] > maxUrgency) {
            maxUrgency = urgentNeeds[card];
            bestUrgentCard = i;
        }
    }
    
    if (bestUrgentCard >= 0) {
        return bestUrgentCard;
    }
    
    // Chercher les couleurs dont nous avons le plus besoin
    int bestNeededCard = -1;
    int maxNeed = 0;
    
    for (int i = 0; i < 5; i++) {
        CardColor card = state->visibleCards[i];
        if (card != NONE && card != LOCOMOTIVE) {
            int score = totalNeeds[card];
            
            // Bonus synergie si on a déjà quelques cartes
            if (state->nbCardsByColor[card] > 0 && state->nbCardsByColor[card] <= 5) {
                score += state->nbCardsByColor[card] * 2;
            }
            
            // Pénalité si trop de cartes de cette couleur
            if (state->nbCardsByColor[card] > 8) {
                score -= 5;
            }
            
            if (score > maxNeed) {
                maxNeed = score;
                bestNeededCard = i;
            }
        }
    }
    
    if (bestNeededCard >= 0 && maxNeed > 3) {
        return bestNeededCard;
    }
    
    // Diversification - prendre une couleur qu'on n'a pas
    if (state->nbCards > 6) {
        for (int i = 0; i < 5; i++) {
            CardColor card = state->visibleCards[i];
            if (card != NONE && card != LOCOMOTIVE && 
                state->nbCardsByColor[card] == 0 && totalNeeds[card] > 0) {
                return i;
            }
        }
    }
    
    // Prendre n'importe quelle carte utile
    for (int i = 0; i < 5; i++) {
        CardColor card = state->visibleCards[i];
        if (card != NONE && card != LOCOMOTIVE && totalNeeds[card] > 0) {
            return i;
        }
    }
    
    return -1; // Pioche aveugle recommandée
}

// Détermine la couleur optimale pour prendre une route
CardColor determineOptimalColor(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERROR: Invalid route index %d\n", routeIndex);
        return RED;
    }
    
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor secondColor = state->routes[routeIndex].secondColor;
    int length = state->routes[routeIndex].length;
    
    // Route colorée (non grise)
    if (routeColor != LOCOMOTIVE) {
        // Option 1: Couleur principale pure
        if (state->nbCardsByColor[routeColor] >= length) {
            return routeColor;
        }
        
        // Option 2: Couleur secondaire pure (si existe)
        if (secondColor != NONE && state->nbCardsByColor[secondColor] >= length) {
            return secondColor;
        }
        
        // Option 3: Couleur principale + locomotives
        if (state->nbCardsByColor[routeColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
            return routeColor;
        }
        
        // Option 4: Couleur secondaire + locomotives
        if (secondColor != NONE && 
            state->nbCardsByColor[secondColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
            return secondColor;
        }
        
        // Option 5: Locomotives seules
        if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
            return LOCOMOTIVE;
        }
        
        return NONE; // Pas assez de cartes
    }
    
    // Route grise - trouver la meilleure option
    else {
        CardColor bestColor = NONE;
        int bestScore = -1;
        
        // Évaluer chaque couleur standard (1-8)
        for (int c = PURPLE; c <= GREEN; c++) {
            int colorCards = state->nbCardsByColor[c];
            int totalAvailable = colorCards + state->nbCardsByColor[LOCOMOTIVE];
            
            if (totalAvailable >= length) {
                int score = 0;
                
                // Score de base : préférer les cartes colorées
                if (colorCards >= length) {
                    score = 1000; // Parfait - que des cartes colorées
                } else {
                    score = 500 - (length - colorCards) * 10; // Pénalité par locomotive
                }
                
                // Bonus si on a exactement ce qu'il faut
                if (colorCards == length) {
                    score += 100;
                }
                
                // Bonus si on a beaucoup de cartes de cette couleur
                if (colorCards > length + 2) {
                    score += 50;
                }
                
                if (score > bestScore) {
                    bestScore = score;
                    bestColor = c;
                }
            }
        }
        
        // Évaluer locomotives seules
        if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
            int locoScore = 200;
            
            // Pénalité pour routes courtes (gaspillage)
            if (length <= 2) {
                locoScore -= 100;
            }
            
            if (locoScore > bestScore) {
                bestScore = locoScore;
                bestColor = LOCOMOTIVE;
            }
        }
        
        if (bestColor != NONE) {
            return bestColor;
        }
        
        return NONE; // Aucune option viable
    }
}

// Analyse les besoins en cartes pour les objectifs
void analyzeCardNeeds(GameState* state, int colorNeeds[10]) {
    // Initialiser le tableau
    for (int i = 0; i < 10; i++) {
        colorNeeds[i] = 0;
    }
    
    // Pour chaque objectif non complété
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        
        if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
            continue;
        }
        
        int path[MAX_CITIES];
        int pathLength = 0;
        if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
            // Pour chaque segment du chemin
            for (int j = 0; j < pathLength - 1; j++) {
                int cityA = path[j];
                int cityB = path[j+1];
                
                if (cityA < 0 || cityA >= state->nbCities || cityB < 0 || cityB >= state->nbCities) {
                    continue;
                }
                
                // Trouver la route correspondante
                for (int r = 0; r < state->nbTracks; r++) {
                    if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                         (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                        state->routes[r].owner == 0) {
                        
                        CardColor routeColor = state->routes[r].color;
                        int length = state->routes[r].length;
                        
                        // Si c'est une route grise, toutes les couleurs sont possibles
                        if (routeColor == LOCOMOTIVE) {
                            // Ajouter un besoin général pour toutes les couleurs
                            for (int c = 1; c < 9; c++) {
                                colorNeeds[c] += length / 8; // Répartir le besoin
                            }
                            colorNeeds[LOCOMOTIVE] += 1;
                        } else {
                            // Calculer combien il nous manque de cartes de cette couleur
                            int have = state->nbCardsByColor[routeColor];
                            int needed = length;
                            if (have < needed) {
                                colorNeeds[routeColor] += (needed - have);
                            }
                        }
                        
                        break;
                    }
                }
            }
        }
    }
}

// Évalue une carte visible selon les besoins
int evaluateVisibleCard(GameState* state, CardColor card, int colorNeeds[10]) {
    if (card == NONE || card < 1 || card > 9) {
        return 0;
    }
    
    int score = 0;
    
    // Les locomotives sont toujours précieuses
    if (card == LOCOMOTIVE) {
        score = 100;
        score += colorNeeds[LOCOMOTIVE] * 10;
        
        // Bonus si nous avons peu de locomotives
        if (state->nbCardsByColor[LOCOMOTIVE] < 3) {
            score += 50;
        }
    } 
    // Score des autres cartes basé sur les besoins
    else {
        score = colorNeeds[card] * 5;
        
        // Bonus synergie si nous avons déjà des cartes de cette couleur
        if (state->nbCardsByColor[card] > 0) {
            score += state->nbCardsByColor[card] * 3;
            
            // Bonus si nous sommes proches de compléter une route
            for (int r = 0; r < state->nbTracks; r++) {
                if (state->routes[r].owner == 0) {
                    CardColor routeColor = state->routes[r].color;
                    int length = state->routes[r].length;
                    
                    if (routeColor == card || routeColor == LOCOMOTIVE) {
                        int cardsNeeded = length - state->nbCardsByColor[card];
                        if (cardsNeeded > 0 && cardsNeeded <= 2) {
                            score += (3 - cardsNeeded) * 15;
                            
                            // Bonus pour les routes longues
                            if (length >= 4) {
                                score += length * 10;
                            }
                        }
                    }
                }
            }
        }
        
        // Pénalité si nous avons trop de cartes de cette couleur
        if (state->nbCardsByColor[card] > 8) {
            score -= (state->nbCardsByColor[card] - 8) * 5;
        }
        
        // Bonus pour la diversification
        int totalCards = 0;
        for (int i = 1; i < 10; i++) {
            totalCards += state->nbCardsByColor[i];
        }
        
        if (totalCards > 5 && state->nbCardsByColor[card] == 0) {
            score += 20;
        }
    }
    
    return score;
}

// Calcule l'efficacité d'utilisation d'une couleur pour une route
int calculateCardEfficiency(GameState* state, CardColor color, int routeLength) {
    if (color < 1 || color > 9 || routeLength <= 0) {
        return 0;
    }
    
    int efficiency = 0;
    
    // Efficacité de base selon la disponibilité
    int available = state->nbCardsByColor[color];
    int locomotives = state->nbCardsByColor[LOCOMOTIVE];
    
    if (color == LOCOMOTIVE) {
        if (locomotives >= routeLength) {
            efficiency = 100;
        } else {
            efficiency = (locomotives * 100) / routeLength;
        }
    } else {
        if (available >= routeLength) {
            efficiency = 150; // Bonus pour avoir toutes les cartes nécessaires
        } else if (available + locomotives >= routeLength) {
            int locosNeeded = routeLength - available;
            efficiency = 100 - (locosNeeded * 10); // Pénalité par locomotive utilisée
        } else {
            efficiency = 0; // Pas assez de cartes
        }
    }
    
    // Pénalité pour gaspiller des locomotives sur des routes courtes
    if (color == LOCOMOTIVE && routeLength <= 2) {
        efficiency -= 50;
    }
    
    // Bonus pour utiliser efficacement nos ressources
    if (color != LOCOMOTIVE && available > 0) {
        float utilizationRate = (float)routeLength / available;
        if (utilizationRate > 0.5 && utilizationRate <= 1.0) {
            efficiency += 25; // Bon taux d'utilisation
        }
    }
    
    return efficiency;
}