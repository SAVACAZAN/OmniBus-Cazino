#pragma once
#include <QString>
#include <QMap>

struct Player {
    QString id;
    QString name = "Player";
    QMap<QString, double> balances; // "EUR" -> 4821.50, "BTC" -> 0.0142
    int totalBets = 0;
    int totalWins = 0;
    double totalProfit = 0.0;

    double balance(const QString& currency = "EUR") const {
        return balances.value(currency, 0.0);
    }
    double winRate() const {
        return totalBets > 0 ? (double)totalWins / totalBets * 100.0 : 0.0;
    }
};
