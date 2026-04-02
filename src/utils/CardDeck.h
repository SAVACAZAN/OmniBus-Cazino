#pragma once
#include <QString>
#include <QVector>

struct Card {
    enum Suit { Hearts, Diamonds, Clubs, Spades };
    enum Rank { Ace = 1, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King };

    Rank rank;
    Suit suit;

    int value() const {
        if (rank >= Ten) return 10;
        return static_cast<int>(rank);
    }

    int blackjackValue(bool aceHigh = true) const {
        if (rank == Ace) return aceHigh ? 11 : 1;
        if (rank >= Ten) return 10;
        return static_cast<int>(rank);
    }

    QString toString() const {
        static const char* ranks[] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
        static const char* suits[] = {"\xe2\x99\xa5", "\xe2\x99\xa6", "\xe2\x99\xa3", "\xe2\x99\xa0"};
        return QString("%1%2").arg(ranks[rank]).arg(QString::fromUtf8(suits[suit]));
    }

    bool isRed() const { return suit == Hearts || suit == Diamonds; }
};

class CardDeck {
public:
    CardDeck();
    void reset();
    void shuffle();
    void shuffleSeeded(const QByteArray& seed);  // deterministic shuffle from seed
    Card deal();
    int remaining() const { return m_cards.size(); }
    bool empty() const { return m_cards.isEmpty(); }
    const QVector<Card>& cards() const { return m_cards; }

private:
    QVector<Card> m_cards;
};
