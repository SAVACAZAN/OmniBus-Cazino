#pragma once
#include "ui/GameWidget.h"
#include "utils/CardDeck.h"
#include <QLabel>
#include <QPushButton>
#include <QVector>

class BlackjackGame : public GameWidget {
    Q_OBJECT
public:
    explicit BlackjackGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

private:
    void setupUI();
    void dealInitial();
    void hit();
    void stand();
    void doubleDown();
    void dealerPlay();
    void evaluateHands();
    int handValue(const QVector<Card>& hand) const;
    QString handToString(const QVector<Card>& hand, bool hideSecond = false) const;
    void updateDisplay();

    QLabel* m_dealerLabel;
    QLabel* m_dealerValueLabel;
    QLabel* m_playerLabel;
    QLabel* m_playerValueLabel;
    QLabel* m_resultLabel;
    QPushButton* m_hitBtn;
    QPushButton* m_standBtn;
    QPushButton* m_doubleBtn;
    QPushButton* m_dealBtn;

    CardDeck m_deck;
    QVector<Card> m_playerHand;
    QVector<Card> m_dealerHand;
    bool m_gameActive = false;
    bool m_dealerRevealed = false;
};
