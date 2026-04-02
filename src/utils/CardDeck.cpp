#include "CardDeck.h"
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <algorithm>

CardDeck::CardDeck()
{
    reset();
}

void CardDeck::reset()
{
    m_cards.clear();
    m_cards.reserve(52);
    for (int s = 0; s < 4; ++s) {
        for (int r = 1; r <= 13; ++r) {
            m_cards.append({static_cast<Card::Rank>(r), static_cast<Card::Suit>(s)});
        }
    }
}

void CardDeck::shuffle()
{
    // Fisher-Yates shuffle
    auto* rng = QRandomGenerator::global();
    for (int i = m_cards.size() - 1; i > 0; --i) {
        int j = rng->bounded(i + 1);
        std::swap(m_cards[i], m_cards[j]);
    }
}

void CardDeck::shuffleSeeded(const QByteArray& seed)
{
    // Deterministic Fisher-Yates shuffle using seed-derived PRNG
    // Generate enough random bytes by repeatedly hashing seed + counter
    QByteArray rngBytes;
    int counter = 0;
    while (rngBytes.size() < m_cards.size() * 4) {
        QByteArray input = seed + QByteArray::number(counter++);
        rngBytes.append(QCryptographicHash::hash(input, QCryptographicHash::Sha256));
    }

    int byteIdx = 0;
    for (int i = m_cards.size() - 1; i > 0; --i) {
        quint32 val = 0;
        for (int b = 0; b < 4; ++b) {
            val = (val << 8) | static_cast<quint8>(rngBytes[byteIdx++]);
        }
        int j = static_cast<int>(val % static_cast<quint32>(i + 1));
        std::swap(m_cards[i], m_cards[j]);
    }
}

Card CardDeck::deal()
{
    if (m_cards.isEmpty()) {
        reset();
        shuffle();
    }
    Card c = m_cards.last();
    m_cards.removeLast();
    return c;
}
