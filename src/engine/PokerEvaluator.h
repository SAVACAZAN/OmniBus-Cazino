#pragma once
#include <QString>
#include <QVector>
#include <QPair>
#include "utils/CardDeck.h"

// Wrapper around OMPEval for professional hand evaluation
// Falls back to our manual evaluator if OMPEval not available

namespace PokerEval {

// Hand ranking (same as our existing enum but more detailed)
enum HandRank {
    HighCard = 0,
    OnePair = 1,
    TwoPair = 2,
    ThreeOfAKind = 3,
    Straight = 4,
    Flush = 5,
    FullHouse = 6,
    FourOfAKind = 7,
    StraightFlush = 8,
    RoyalFlush = 9
};

struct HandResult {
    HandRank rank;
    int score;          // numeric score for comparison (higher = better)
    QString rankName;   // "Two Pair", "Full House", etc.
    QString description; // "Two Pair, Aces and Kings"
    QVector<int> kickers; // kicker cards for tiebreaking
};

// Evaluate best 5-card hand from any number of cards (typically 7 = 2 hole + 5 community)
HandResult evaluate(const QVector<Card>& cards);

// Compare two hands — returns >0 if hand1 wins, <0 if hand2 wins, 0 if tie
int compare(const QVector<Card>& hand1, const QVector<Card>& hand2);

// Calculate win equity (percentage chance of winning) using Monte Carlo simulation
// hand = 2 hole cards, community = 0-5 community cards, numOpponents = how many opponents
// numSimulations = how many random deals to simulate (more = more accurate, slower)
double calculateEquity(const QVector<Card>& hand, const QVector<Card>& community,
                        int numOpponents = 1, int numSimulations = 10000);

// Get hand rank name
QString rankToString(HandRank rank);

// Convert our Card to OMPEval format
// Our Card: rank 1-13 (Ace=1, King=13), suit 0-3
// OMPEval: uses its own encoding

} // namespace PokerEval
