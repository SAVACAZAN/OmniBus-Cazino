#include "SportsBettingGame.h"
#include "engine/ProvablyFair.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

SportsBettingGame::SportsBettingGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("sports", "Sports Betting", engine, parent)
{
    setupUI();
    populateMatches();
}

void SportsBettingGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\xE2\x9A\xBD SPORTS BETTING"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* mainLayout = new QHBoxLayout;

    // Left: matches
    auto* matchBox = new QGroupBox("Matches");
    matchBox->setStyleSheet(
        "QGroupBox { border: 1px solid rgba(250,204,21,0.2); border-radius: 6px; "
        "margin-top: 10px; padding-top: 16px; color: #facc15; font-weight: bold; }");
    auto* matchLayout = new QVBoxLayout(matchBox);
    m_matchList = new QListWidget;
    m_matchList->setStyleSheet(
        "QListWidget { background: #0f1419; border: none; }"
        "QListWidget::item { background: rgba(26,31,46,0.6); color: #e0e0e0; padding: 8px; "
        "margin: 2px 0; border-radius: 4px; }"
        "QListWidget::item:hover { background: rgba(250,204,21,0.1); }");
    matchLayout->addWidget(m_matchList);
    mainLayout->addWidget(matchBox, 2);

    // Right: bet slip
    auto* slipBox = new QGroupBox("Bet Slip");
    slipBox->setStyleSheet(
        "QGroupBox { border: 1px solid rgba(250,204,21,0.2); border-radius: 6px; "
        "margin-top: 10px; padding-top: 16px; color: #facc15; font-weight: bold; }");
    auto* slipLayout = new QVBoxLayout(slipBox);
    m_betSlipList = new QListWidget;
    m_betSlipList->setStyleSheet(
        "QListWidget { background: #0f1419; border: none; }"
        "QListWidget::item { background: rgba(26,31,46,0.6); color: #e0e0e0; padding: 6px; "
        "margin: 1px 0; border-radius: 4px; }");
    slipLayout->addWidget(m_betSlipList);

    m_totalOddsLabel = new QLabel("Total Odds: ---");
    m_totalOddsLabel->setStyleSheet("color: #facc15; font-size: 14px; font-weight: bold;");
    slipLayout->addWidget(m_totalOddsLabel);

    m_clearSlipBtn = new QPushButton("Clear Slip");
    m_clearSlipBtn->setStyleSheet(
        "QPushButton { background: rgba(235,4,4,0.15); color: #eb0404; border: 1px solid rgba(235,4,4,0.3); "
        "padding: 6px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(235,4,4,0.25); }");
    connect(m_clearSlipBtn, &QPushButton::clicked, this, [this]() {
        m_betSlip.clear();
        updateBetSlipDisplay();
    });
    slipLayout->addWidget(m_clearSlipBtn);

    m_placeBetBtn = new QPushButton("PLACE BET");
    m_placeBetBtn->setFixedHeight(40);
    m_placeBetBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #facc15, stop:1 #f59e0b); "
        "color: #0f1419; font-size: 14px; font-weight: bold; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: #fbbf24; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_placeBetBtn, &QPushButton::clicked, this, &SportsBettingGame::placeBets);
    slipLayout->addWidget(m_placeBetBtn);

    mainLayout->addWidget(slipBox, 1);
    layout->addLayout(mainLayout);

    m_resultLabel = new QLabel("");
    m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setWordWrap(true);
    layout->addWidget(m_resultLabel);
}

void SportsBettingGame::populateMatches()
{
    m_matches = {
        {"M1", "Barcelona", "Real Madrid", 2.10, 3.40, 3.20, "La Liga"},
        {"M2", "Man City", "Liverpool", 1.85, 3.60, 3.90, "Premier League"},
        {"M3", "Bayern Munich", "Dortmund", 1.60, 4.00, 4.80, "Bundesliga"},
        {"M4", "PSG", "Marseille", 1.45, 4.20, 6.00, "Ligue 1"},
        {"M5", "Juventus", "AC Milan", 2.30, 3.20, 2.90, "Serie A"},
        {"M6", "Ajax", "PSV", 2.00, 3.50, 3.40, "Eredivisie"},
        {"M7", "Porto", "Benfica", 2.20, 3.30, 3.10, "Liga Portugal"},
        {"M8", "Celtic", "Rangers", 1.90, 3.50, 3.70, "Scottish Prem"},
    };

    m_matchList->clear();
    for (const auto& m : m_matches) {
        auto* widget = new QWidget;
        auto* lay = new QVBoxLayout(widget);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        auto* header = new QLabel(QString("%1 | %2 vs %3").arg(m.league, m.teamA, m.teamB));
        header->setStyleSheet("color: #e0e0e0; font-weight: bold; font-size: 12px;");
        lay->addWidget(header);

        auto* oddsLayout = new QHBoxLayout;
        auto makeOddsBtn = [this, &m](const QString& label, const QString& sel, double odds) {
            auto* btn = new QPushButton(QString("%1\n%2").arg(label).arg(odds, 0, 'f', 2));
            btn->setFixedHeight(40);
            btn->setStyleSheet(
                "QPushButton { background: rgba(250,204,21,0.08); color: #facc15; "
                "border: 1px solid rgba(250,204,21,0.25); border-radius: 4px; font-size: 11px; font-weight: bold; }"
                "QPushButton:hover { background: rgba(250,204,21,0.2); border-color: #facc15; }");
            connect(btn, &QPushButton::clicked, this, [this, m, sel, odds]() {
                addToBetSlip(m, sel, odds);
            });
            return btn;
        };

        oddsLayout->addWidget(makeOddsBtn("1", "1", m.odds1));
        oddsLayout->addWidget(makeOddsBtn("X", "X", m.oddsX));
        oddsLayout->addWidget(makeOddsBtn("2", "2", m.odds2));
        lay->addLayout(oddsLayout);

        auto* item = new QListWidgetItem;
        item->setSizeHint(widget->sizeHint());
        m_matchList->addItem(item);
        m_matchList->setItemWidget(item, widget);
    }
}

void SportsBettingGame::addToBetSlip(const Match& match, const QString& selection, double odds)
{
    // Check if already in slip
    for (const auto& s : m_betSlip) {
        if (s.matchId == match.id) return;
    }

    QString desc;
    if (selection == "1") desc = match.teamA + " to win";
    else if (selection == "X") desc = "Draw";
    else desc = match.teamB + " to win";

    m_betSlip.append({match.id, selection, odds,
        QString("%1 vs %2: %3 @ %4").arg(match.teamA, match.teamB, desc).arg(odds, 0, 'f', 2)});
    updateBetSlipDisplay();
}

void SportsBettingGame::updateBetSlipDisplay()
{
    m_betSlipList->clear();
    double totalOdds = 1.0;
    for (const auto& item : m_betSlip) {
        m_betSlipList->addItem(item.description);
        totalOdds *= item.odds;
    }
    m_totalOddsLabel->setText(m_betSlip.isEmpty()
        ? "Total Odds: ---"
        : QString("Accumulator Odds: %1x").arg(totalOdds, 0, 'f', 2));
}

void SportsBettingGame::placeBets()
{
    if (m_betSlip.isEmpty()) return;
    simulateResults();
}

void SportsBettingGame::simulateResults()
{
    int wins = 0;
    QStringList results;

    for (const auto& slip : m_betSlip) {
        QString hash = ProvablyFair::generateHash(
            ProvablyFair::generateServerSeed(), slip.matchId,
            m_engine->getPlayer().totalBets);
        int result = ProvablyFair::hashToNumber(hash, 0, 100);

        // Simulate based on odds (lower odds = higher chance)
        double winProb = 1.0 / slip.odds * 0.95; // slightly less than fair
        bool won = (result / 100.0) < winProb;

        if (won) {
            wins++;
            results << QString("%1: WIN").arg(slip.description);
        } else {
            results << QString("%1: LOSE").arg(slip.description);
        }
    }

    bool allWon = (wins == m_betSlip.size());

    if (allWon) {
        double totalOdds = 1.0;
        for (const auto& s : m_betSlip) totalOdds *= s.odds;
        m_resultLabel->setText(QString("ALL %1 BETS WON! Payout: %2x").arg(wins).arg(totalOdds, 0, 'f', 2));
        m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #10eb04;");

        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, true);
        emit gameFinished(true, 0);
    } else {
        m_resultLabel->setText(QString("%1/%2 correct. Accumulator lost.").arg(wins).arg(m_betSlip.size()));
        m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #eb0404;");

        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, false);
        emit gameFinished(false, 0);
    }

    m_betSlip.clear();
    updateBetSlipDisplay();
}

void SportsBettingGame::startGame() {}
void SportsBettingGame::resetGame()
{
    m_betSlip.clear();
    updateBetSlipDisplay();
    m_resultLabel->setText("");
}
