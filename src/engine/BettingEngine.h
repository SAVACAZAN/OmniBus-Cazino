#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include "models/Bet.h"
#include "models/Player.h"
#include "engine/ProvablyFair.h"

class BettingEngine : public QObject {
    Q_OBJECT
public:
    explicit BettingEngine(QObject* parent = nullptr);

    Bet placeBet(const QString& gameId, double amount, const QString& currency, double odds);
    bool settleBet(const QString& betId, bool won);

    Player& getPlayer() { return m_player; }
    const Player& getPlayer() const { return m_player; }
    void updateBalance(const QString& currency, double delta);
    void setBalance(const QString& currency, double value);

    QVector<Bet> betHistory(int count = 50) const;
    const QVector<Bet>& allBets() const { return m_bets; }

    double minBet() const { return m_minBet; }
    double maxBet() const { return m_maxBet; }
    double houseEdge() const { return m_houseEdge; }

    void saveState(const QString& path = "casino_state.json");
    void loadState(const QString& path = "casino_state.json");
    void loadConfig(const QString& path = "config.json");

    ProvablyFair& provablyFair() { return m_pf; }

signals:
    void balanceChanged(const QString& currency, double newBalance);
    void betPlaced(const Bet& bet);
    void betSettled(const Bet& bet);

private:
    Player m_player;
    QVector<Bet> m_bets;
    ProvablyFair m_pf;
    double m_minBet = 0.01;
    double m_maxBet = 10000.0;
    double m_houseEdge = 0.01;
    int m_nextBetId = 1;
};
