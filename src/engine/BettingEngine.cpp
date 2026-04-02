#include "BettingEngine.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

BettingEngine::BettingEngine(QObject* parent)
    : QObject(parent)
{
}

Bet BettingEngine::placeBet(const QString& gameId, double amount, const QString& currency, double odds)
{
    Bet bet;
    bet.id = QString("BET-%1").arg(m_nextBetId++, 6, 10, QChar('0'));
    bet.gameId = gameId;
    bet.playerId = m_player.id;
    bet.amount = amount;
    bet.currency = currency;
    bet.odds = odds;
    bet.timestamp = QDateTime::currentDateTime();
    bet.provablyFairHash = ProvablyFair::generateServerSeed();

    // Deduct from balance
    m_player.balances[currency] -= amount;
    m_player.totalBets++;

    m_bets.append(bet);
    emit balanceChanged(currency, m_player.balances[currency]);
    emit betPlaced(bet);
    return bet;
}

bool BettingEngine::settleBet(const QString& betId, bool won)
{
    for (int i = m_bets.size() - 1; i >= 0; --i) {
        if (m_bets[i].id == betId) {
            m_bets[i].won = won;
            if (won) {
                m_bets[i].payout = m_bets[i].amount * m_bets[i].odds;
                m_player.balances[m_bets[i].currency] += m_bets[i].payout;
                m_player.totalWins++;
                m_player.totalProfit += (m_bets[i].payout - m_bets[i].amount);
            } else {
                m_bets[i].payout = 0.0;
                m_player.totalProfit -= m_bets[i].amount;
            }
            emit balanceChanged(m_bets[i].currency, m_player.balances[m_bets[i].currency]);
            emit betSettled(m_bets[i]);
            return true;
        }
    }
    return false;
}

void BettingEngine::updateBalance(const QString& currency, double delta)
{
    m_player.balances[currency] += delta;
    emit balanceChanged(currency, m_player.balances[currency]);
}

void BettingEngine::setBalance(const QString& currency, double value)
{
    m_player.balances[currency] = value;
    emit balanceChanged(currency, m_player.balances[currency]);
}

QVector<Bet> BettingEngine::betHistory(int count) const
{
    int start = qMax(0, m_bets.size() - count);
    return m_bets.mid(start);
}

void BettingEngine::loadConfig(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    if (root.contains("player")) {
        QJsonObject p = root["player"].toObject();
        m_player.name = p["name"].toString("Player");
        m_player.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject bals = p["balances"].toObject();
        for (auto it = bals.begin(); it != bals.end(); ++it)
            m_player.balances[it.key()] = it.value().toDouble();
    }
    m_houseEdge = root["house_edge"].toDouble(0.01);
    m_minBet = root["min_bet"].toDouble(0.01);
    m_maxBet = root["max_bet"].toDouble(10000);
}

void BettingEngine::saveState(const QString& path)
{
    QJsonObject root;
    QJsonObject player;
    player["name"] = m_player.name;
    player["id"] = m_player.id;
    player["totalBets"] = m_player.totalBets;
    player["totalWins"] = m_player.totalWins;
    player["totalProfit"] = m_player.totalProfit;
    QJsonObject bals;
    for (auto it = m_player.balances.begin(); it != m_player.balances.end(); ++it)
        bals[it.key()] = it.value();
    player["balances"] = bals;
    root["player"] = player;

    QJsonArray betsArr;
    for (const auto& b : m_bets) {
        QJsonObject bo;
        bo["id"] = b.id;
        bo["gameId"] = b.gameId;
        bo["amount"] = b.amount;
        bo["currency"] = b.currency;
        bo["odds"] = b.odds;
        bo["payout"] = b.payout;
        bo["won"] = b.won;
        bo["timestamp"] = b.timestamp.toString(Qt::ISODate);
        betsArr.append(bo);
    }
    root["bets"] = betsArr;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}

void BettingEngine::loadState(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    if (root.contains("player")) {
        QJsonObject p = root["player"].toObject();
        m_player.name = p["name"].toString("Player");
        m_player.id = p["id"].toString();
        m_player.totalBets = p["totalBets"].toInt();
        m_player.totalWins = p["totalWins"].toInt();
        m_player.totalProfit = p["totalProfit"].toDouble();
        QJsonObject bals = p["balances"].toObject();
        for (auto it = bals.begin(); it != bals.end(); ++it)
            m_player.balances[it.key()] = it.value().toDouble();
    }
}
