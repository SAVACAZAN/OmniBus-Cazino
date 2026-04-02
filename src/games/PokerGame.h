#pragma once
#include "ui/GameWidget.h"
#include "utils/CardDeck.h"
#include <QLabel>
#include <QPushButton>
#include <QVector>

class PokerGame : public GameWidget {
    Q_OBJECT
public:
    explicit PokerGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

    enum HandRank {
        HighCard, OnePair, TwoPair, ThreeOfAKind, Straight,
        Flush, FullHouse, FourOfAKind, StraightFlush, RoyalFlush
    };

private:
    void setupUI();
    void dealPreflop();
    void dealFlop();
    void dealTurn();
    void dealRiver();
    void showdown();
    HandRank evaluateHand(const QVector<Card>& cards) const;
    QString rankName(HandRank rank) const;
    int handScore(const QVector<Card>& allCards) const;
    void updateDisplay();
    void aiAction();

    QLabel* m_communityLabel;
    QLabel* m_playerLabel;
    QLabel* m_aiLabel;
    QLabel* m_potLabel;
    QLabel* m_resultLabel;
    QLabel* m_phaseLabel;
    QPushButton* m_foldBtn;
    QPushButton* m_checkBtn;
    QPushButton* m_callBtn;
    QPushButton* m_raiseBtn;
    QPushButton* m_dealBtn;

    CardDeck m_deck;
    QVector<Card> m_playerCards;
    QVector<Card> m_aiCards;
    QVector<Card> m_community;
    double m_pot = 0.0;
    double m_currentBetAmount = 0.0;
    int m_phase = 0; // 0=preflop, 1=flop, 2=turn, 3=river, 4=showdown
    bool m_gameActive = false;
    QString m_betCurrency;
};
