#pragma once
#include <QString>
#include <QDateTime>

struct Bet {
    QString id;
    QString gameId;
    QString playerId;
    double amount = 0.0;
    QString currency = "EUR";
    double odds = 1.0;
    double payout = 0.0;
    bool won = false;
    QDateTime timestamp;
    QString provablyFairHash;
};
