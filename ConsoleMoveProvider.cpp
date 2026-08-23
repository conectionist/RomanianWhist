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

Card* ConsoleMoveProvider::playCard(vector<Card*>& hand, Card* trump, const Suit* leadSuit)
{
    CardValidator cardValidator;
    vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, leadSuit);

    std::cout << "Your hand:" << std::endl;
    for(Card* card : hand)
        std::cout << "  " << card->toString() << std::endl;

    std::cout << "Legal plays:" << std::endl;
    for(int i = 0 ; i < static_cast<int>(legalCards.size()) ; i++)
        std::cout << i << ") " << legalCards[i]->toString() << std::endl;

    int choice;
    while(true)
    {
        std::cin >> choice;
        if(choice >= 0 && choice < static_cast<int>(legalCards.size()))
            break;
        std::cout << "Invalid choice. Enter a number between 0 and "
                  << (legalCards.size() - 1) << ": ";
    }

    Card* card = legalCards[choice];
    hand.erase(std::find(hand.begin(), hand.end(), card));
    return card;
}
