#pragma once
#include "ui/GameWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QVector>

struct Match {
    QString id;
    QString teamA;
    QString teamB;
    double odds1;  // team A win
    double oddsX;  // draw
    double odds2;  // team B win
    QString league;
};

struct BetSlipItem {
    QString matchId;
    QString selection; // "1", "X", "2"
    double odds;
    QString description;
};

class SportsBettingGame : public GameWidget {
    Q_OBJECT
public:
    explicit SportsBettingGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

private:
    void setupUI();
    void populateMatches();
    void addToBetSlip(const Match& match, const QString& selection, double odds);
    void placeBets();
    void simulateResults();
    void updateBetSlipDisplay();

    QListWidget* m_matchList;
    QListWidget* m_betSlipList;
    QLabel* m_totalOddsLabel;
    QLabel* m_resultLabel;
    QPushButton* m_placeBetBtn;
    QPushButton* m_clearSlipBtn;

    QVector<Match> m_matches;
    QVector<BetSlipItem> m_betSlip;
};
