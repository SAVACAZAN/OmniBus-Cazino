#include "PokerEvaluator.h"

// Include OMPEval — hand evaluator only
#include "omp/HandEvaluator.h"

#include <algorithm>
#include <map>
#include <QRandomGenerator>

namespace PokerEval {

// Convert our Card (rank 1-13, suit 0-3) to OMPEval card index (0-51)
// OMPEval: card = rank * 4 + suit, where rank: 0=deuce, 12=ace, suit: 0-3
static unsigned ourCardToOmp(const Card& c) {
    // Our ranks: 1=Ace, 2-10, 11=Jack, 12=Queen, 13=King
    // OMPEval: 0=Deuce, 1=Trey, ..., 8=Ten, 9=Jack, 10=Queen, 11=King, 12=Ace
    int ompRank;
    if (c.rank == 1)  // Ace
        ompRank = 12;
    else
        ompRank = c.rank - 2;  // 2→0, 3→1, ..., K→11

    // Our suits: 0=Hearts, 1=Diamonds, 2=Clubs, 3=Spades
    // OMPEval suits: just 0-3, doesn't matter which is which
    int ompSuit = c.suit;

    return ompRank * 4 + ompSuit;
}

// Convert card vector to OMPEval hand
static omp::Hand cardsToOmpHand(const QVector<Card>& cards) {
    omp::Hand h = omp::Hand::empty();
    for (const auto& c : cards) {
        h += omp::Hand(ourCardToOmp(c));
    }
    return h;
}

// Get HandRank from OMPEval score
static HandRank scoreToRank(uint16_t score) {
    // OMPEval scores: higher = better
    // Score ranges per hand type (from OMPEval source):
    // High Card:      1 - 1277
    // One Pair:    1278 - 4137
    // Two Pair:    4138 - 4995
    // Three Kind:  4996 - 5853
    // Straight:    5854 - 5863
    // Flush:       5864 - 7140
    // Full House:  7141 - 7296
    // Four Kind:   7297 - 7452
    // Str Flush:   7453 - 7461
    // Royal Flush: 7462

    if (score >= 7462) return RoyalFlush;
    if (score >= 7453) return StraightFlush;
    if (score >= 7297) return FourOfAKind;
    if (score >= 7141) return FullHouse;
    if (score >= 5864) return Flush;
    if (score >= 5854) return Straight;
    if (score >= 4996) return ThreeOfAKind;
    if (score >= 4138) return TwoPair;
    if (score >= 1278) return OnePair;
    return HighCard;
}

HandResult evaluate(const QVector<Card>& cards) {
    HandResult result;

    if (cards.size() < 5) {
        // Not enough cards — do basic evaluation
        std::map<int, int> rankCount;
        for (const auto& c : cards) rankCount[static_cast<int>(c.rank)]++;
        bool hasPair = false;
        for (auto& kv : rankCount) if (kv.second >= 2) hasPair = true;

        result.rank = hasPair ? OnePair : HighCard;
        result.score = hasPair ? 1278 : 1;
        result.rankName = rankToString(result.rank);
        return result;
    }

    // Use OMPEval for 5+ cards
    omp::HandEvaluator eval;
    omp::Hand hand = cardsToOmpHand(cards);
    uint16_t score = eval.evaluate(hand);

    result.rank = scoreToRank(score);
    result.score = score;
    result.rankName = rankToString(result.rank);

    return result;
}

int compare(const QVector<Card>& hand1, const QVector<Card>& hand2) {
    auto r1 = evaluate(hand1);
    auto r2 = evaluate(hand2);
    return r1.score - r2.score;
}

double calculateEquity(const QVector<Card>& hand, const QVector<Card>& community,
                        int numOpponents, int numSimulations) {
    // Simple Monte Carlo equity calculator using OMPEval's HandEvaluator
    // Deal random remaining cards and count wins
    Q_UNUSED(numOpponents);

    if (hand.size() < 2) return 50.0;

    omp::HandEvaluator eval;
    int wins = 0, total = 0;

    // Cards already used
    QVector<unsigned> usedCards;
    for (const auto& c : hand) usedCards.append(ourCardToOmp(c));
    for (const auto& c : community) usedCards.append(ourCardToOmp(c));

    // Build remaining deck
    QVector<unsigned> deck;
    for (unsigned i = 0; i < 52; ++i) {
        if (!usedCards.contains(i)) deck.append(i);
    }

    QRandomGenerator rng = QRandomGenerator::securelySeeded();

    for (int sim = 0; sim < numSimulations; ++sim) {
        // Shuffle remaining deck
        QVector<unsigned> shuffled = deck;
        for (int i = shuffled.size() - 1; i > 0; --i) {
            int j = rng.bounded(i + 1);
            std::swap(shuffled[i], shuffled[j]);
        }

        int idx = 0;

        // Complete community to 5 cards
        omp::Hand heroHand = omp::Hand::empty();
        for (const auto& c : hand) heroHand += omp::Hand(ourCardToOmp(c));
        for (const auto& c : community) heroHand += omp::Hand(ourCardToOmp(c));
        int needed = 5 - community.size();
        for (int i = 0; i < needed && idx < shuffled.size(); ++i)
            heroHand += omp::Hand(shuffled[idx++]);

        // Deal opponent cards
        omp::Hand oppHand = omp::Hand::empty();
        if (idx + 1 < shuffled.size()) {
            oppHand += omp::Hand(shuffled[idx++]);
            oppHand += omp::Hand(shuffled[idx++]);
        }
        // Add same community
        for (const auto& c : community) oppHand += omp::Hand(ourCardToOmp(c));
        int oppNeeded = 5 - community.size();
        // Reuse same community completion
        idx = 0;
        for (int i = 0; i < oppNeeded && idx < shuffled.size(); ++i)
            oppHand += omp::Hand(shuffled[idx++]);

        uint16_t heroScore = eval.evaluate(heroHand);
        uint16_t oppScore = eval.evaluate(oppHand);

        if (heroScore > oppScore) wins++;
        total++;
    }

    return total > 0 ? (double)wins / total * 100.0 : 50.0;
}

QString rankToString(HandRank rank) {
    switch (rank) {
        case HighCard:      return "High Card";
        case OnePair:       return "One Pair";
        case TwoPair:       return "Two Pair";
        case ThreeOfAKind:  return "Three of a Kind";
        case Straight:      return "Straight";
        case Flush:         return "Flush";
        case FullHouse:     return "Full House";
        case FourOfAKind:   return "Four of a Kind";
        case StraightFlush: return "Straight Flush";
        case RoyalFlush:    return "Royal Flush";
    }
    return "Unknown";
}

} // namespace PokerEval
