#include "ConsoleMoveProvider.h"
#include "CardValidator.h"

#include <iostream>
#include <algorithm>

unsigned int ConsoleMoveProvider::makeBet(const vector<Card*>& hand, Card* trump, bool isFirstPlayer)
{
    unsigned int bet = 0;

    std::cout << "Enter bet: ";
    std::cin >> bet;

    return bet;
}

Card* ConsoleMoveProvider::playCard(vector<Card*>& hand, Card* trump, const Suit* downSuit)
{
    CardValidator cardValidator;
    vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, downSuit);

    std::cout << "Choose a card to play:" << std::endl;
    for(int i = 0 ; i < legalCards.size() ; i++)
    {
        std::cout << i << ") " << legalCards[i]->toString() << std::endl;
    }

    int choice;
    std::cin >> choice;

    if (choice < 0 || choice >= legalCards.size()) {
        choice = 0;
    }

    Card* card = legalCards[choice];
    hand.erase(std::find(hand.begin(), hand.end(), card));
    return card;
}
