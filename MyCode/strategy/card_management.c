/**
 * card_management.c
 * Gestion intelligente des cartes et stratégies de pioche - VERSION CORRIGÉE
 */
#include <stdio.h>
#include <stdlib.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  

// Pioche stratégique de cartes basée sur les besoins actuels
int strategicCardDrawing(GameState* state) {
    if (!state) {
        return -1;
    }
    
    // Définir les tableaux pour l'analyse
    int colorNeeds[10] = {0};
    int totalColorNeeds = 0;
    
    printf("Analyzing card needs for incomplete objectives:\n");
    
    // Déterminer les besoins de couleurs pour objectifs non complétés
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) continue;
        
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        
        if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
            continue;
        }
        
        printf("  Objective %d: From %d to %d\n", i+1, objFrom, objTo);
        
        int path[MAX_CITIES];
        int pathLength = 0;
        // CHANGEMENT: Utiliser findShortestPath pour analyser les besoins en cartes
        int distance = findShortestPath(state, objFrom, objTo, path, &pathLength);
        
        if (distance > 0 && pathLength > 0) {
            for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
                int pathFrom = path[j];
                int pathTo = path[j+1];
                
                if (pathFrom < 0 || pathFrom >= state->nbCities || 
                    pathTo < 0 || pathTo >= state->nbCities) {
                    continue;
                }
                
                for (int k = 0; k < state->nbTracks; k++) {
                    if (((state->routes[k].from == pathFrom && state->routes[k].to == pathTo) ||
                         (state->routes[k].from == pathTo && state->routes[k].to == pathFrom)) &&
                        state->routes[k].owner == 0) {
                        
                        CardColor routeColor = state->routes[k].color;
                        int length = state->routes[k].length;
                        
                        if (routeColor != LOCOMOTIVE) {
                            colorNeeds[routeColor] += length;
                            totalColorNeeds += length;
                        } else {
                            colorNeeds[LOCOMOTIVE] += 1;
                        }
                        
                        break;
                    }
                }
            }
        }
    }
    
    // Afficher les besoins par couleur
    printf("Color needs summary:\n");
    for (int c = 1; c < 10; c++) {
        if (colorNeeds[c] > 0) {
            printf("  %s: %d cards needed\n", 
                  (c < 10) ? (const char*[]){
                      "None", "Purple", "White", "Blue", "Yellow", 
                      "Orange", "Black", "Red", "Green", "Locomotive"
                  }[c] : "Unknown", 
                  colorNeeds[c]);
        }
    }
    
    // Trouver la meilleure carte visible selon les besoins
    int bestCardIndex = -1;
    int bestCardScore = 0;
    
    for (int i = 0; i < 5; i++) {
        CardColor card = state->visibleCards[i];
        if (card == NONE || card < 0 || card >= 10) {
            continue;
        }
        
        int score = 0;
        
        // Les locomotives sont toujours très précieuses
        if (card == LOCOMOTIVE) {
            score = 100;
            score += colorNeeds[LOCOMOTIVE] * 10;
        } 
        // Score des autres cartes basé sur les besoins
        else {
            score = colorNeeds[card] * 5;
            
            // Bonus si nous avons déjà des cartes de cette couleur
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
                            }
                        }
                    }
                }
            }
            
            // Pénalité si nous avons trop de cartes de cette couleur (>8)
            if (state->nbCardsByColor[card] > 8) {
                score -= (state->nbCardsByColor[card] - 8) * 5;
            }
        }
        
        printf("Visible card %d: %s, score %d\n", 
              i+1, 
              (card < 10) ? (const char*[]){
                  "None", "Purple", "White", "Blue", "Yellow", 
                  "Orange", "Black", "Red", "Green", "Locomotive"
              }[card] : "Unknown", 
              score);
        
        if (score > bestCardScore) {
            bestCardScore = score;
            bestCardIndex = i;
        }
    }
    
    // Si aucune carte visible n'est bonne, suggérer une pioche aveugle
    if (bestCardIndex == -1 || bestCardScore < 20) {
        int blindScore = 40;
        
        // Plus de chances de pioche aveugle si besoins diversifiés
        int uniqueNeeds = 0;
        for (int c = 1; c < 10; c++) {
            if (colorNeeds[c] > 0) {
                uniqueNeeds++;
            }
        }
        
        if (uniqueNeeds >= 3) {
            blindScore += 20;
        }
        
        printf("Blind draw score: %d (unique color needs: %d)\n", blindScore, uniqueNeeds);
        
        if (blindScore > bestCardScore) {
            printf("Recommending blind draw (score %d > best visible card score %d)\n", 
                  blindScore, bestCardScore);
            return -1;
        }
    }
    
    if (bestCardIndex >= 0) {
        printf("Recommending visible card %d: %s (score %d)\n", 
              bestCardIndex + 1, 
              (state->visibleCards[bestCardIndex] < 10) ? (const char*[]){
                  "None", "Purple", "White", "Blue", "Yellow", 
                  "Orange", "Black", "Red", "Green", "Locomotive"
              }[state->visibleCards[bestCardIndex]] : "Unknown", 
              bestCardScore);
    } else {
        printf("No good visible card found, recommending blind draw\n");
    }
    
    return bestCardIndex;
}

// Détermine la couleur optimale pour prendre une route
CardColor determineOptimalColor(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERREUR: Index de route invalide dans determineOptimalColor: %d\n", routeIndex);
        return RED; // Valeur par défaut sécurisée
    }
    
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor secondColor = state->routes[routeIndex].secondColor;
    int length = state->routes[routeIndex].length;
    
    printf("DÉTERMINATION COULEUR pour route %d->%d: Longueur %d, Couleur %d, SecondColor %d\n", 
           state->routes[routeIndex].from, state->routes[routeIndex].to, 
           length, routeColor, secondColor);
    
    // Si c'est une route colorée (non grise)
    if (routeColor != LOCOMOTIVE) {
        // Vérifier si nous avons assez de cartes de la couleur principale
        if (state->nbCardsByColor[routeColor] >= length) {
            printf("  CHOIX: Couleur principale %d (assez de cartes)\n", routeColor);
            return routeColor; // Assez de cartes de cette couleur
        }
        
        // S'il y a une couleur alternative
        if (secondColor != NONE && state->nbCardsByColor[secondColor] >= length) {
            printf("  CHOIX: Couleur secondaire %d (assez de cartes)\n", secondColor);
            return secondColor;
        }
        
        // Si on peut compléter avec des locomotives
        if (state->nbCardsByColor[routeColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
            printf("  CHOIX: Couleur principale %d avec locomotives\n", routeColor);
            return routeColor;
        }
        
        if (secondColor != NONE && 
            state->nbCardsByColor[secondColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
            printf("  CHOIX: Couleur secondaire %d avec locomotives\n", secondColor);
            return secondColor;
        }
        
        // Dernier recours: utiliser uniquement des locomotives
        if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
            printf("  CHOIX: Toutes locomotives\n");
            return LOCOMOTIVE;
        }
        
        printf("  ERREUR: Pas assez de cartes! Retourne NONE\n");
        return NONE; // Pas assez de cartes
    }
    
    // C'est une route grise, trouver la couleur la plus efficace
    CardColor bestColor = NONE;
    int bestColorCount = 0;
    
    for (int c = PURPLE; c <= GREEN; c++) { // 1-8 = couleurs, exclure LOCOMOTIVE
        int count = state->nbCardsByColor[c];
        printf("  Évaluation couleur %d: %d cartes\n", c, count);
        
        if (count > bestColorCount) {
            bestColorCount = count;
            bestColor = c;
        }
    }
    
    // Si la meilleure couleur suffit
    if (bestColor != NONE && bestColorCount >= length) {
        printf("  CHOIX (gris): Couleur %d (assez de cartes: %d)\n", bestColor, bestColorCount);
        return bestColor;
    }
    
    // Compléter avec des locomotives
    if (bestColor != NONE && bestColorCount + state->nbCardsByColor[LOCOMOTIVE] >= length) {
        printf("  CHOIX (gris): Couleur %d avec locomotives\n", bestColor);
        return bestColor;
    }
    
    // Si uniquement des locomotives suffisent
    if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
        printf("  CHOIX (gris): Toutes locomotives\n");
        return LOCOMOTIVE;
    }
    
    // Fallback: chercher n'importe quelle couleur dont on a des cartes
    for (int c = PURPLE; c <= GREEN; c++) {
        if (state->nbCardsByColor[c] > 0) {
            printf("  CHOIX FALLBACK: Couleur %d (quelques cartes)\n", c);
            return c;
        }
    }
    
    // Dernier recours vraiment désespéré
    printf("  CHOIX DÉSESPÉRÉ: RED par défaut\n");
    return RED; // Pour éviter de retourner NONE
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
        
        // CHANGEMENT: Utiliser findShortestPath pour analyser les besoins
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
        
        // Bonus supplémentaire si nous avons peu de locomotives
        if (state->nbCardsByColor[LOCOMOTIVE] < 3) {
            score += 50;
        }
    } 
    // Score des autres cartes basé sur les besoins
    else {
        score = colorNeeds[card] * 5;
        
        // Bonus si nous avons déjà des cartes de cette couleur (synergie)
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
                            
                            // Bonus supplémentaire pour les routes longues
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
        
        // Bonus pour les couleurs rares dans notre main
        int totalCards = 0;
        for (int i = 1; i < 10; i++) {
            totalCards += state->nbCardsByColor[i];
        }
        
        if (totalCards > 5 && state->nbCardsByColor[card] == 0) {
            score += 20; // Bonus pour la diversification
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
            efficiency = 100 - (locosNeeded * 10); // Pénalité pour chaque locomotive utilisée
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