#include "RaceBettingGame.h"
#include "engine/ProvablyFair.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPainter>
#include <QRandomGenerator>
#include <algorithm>

RaceBettingGame::RaceBettingGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("race", "Race Betting", engine, parent)
{
    m_raceTimer = new QTimer(this);
    connect(m_raceTimer, &QTimer::timeout, this, &RaceBettingGame::tick);

    m_racers = {
        {"Thunder Bolt", 3.50}, {"Golden Arrow", 4.20}, {"Night Fury", 5.00}, {"Silver Wind", 6.50},
        {"Iron Stride", 8.00}, {"Dark Storm", 10.00}, {"Lucky Star", 12.00}, {"Wild Card", 15.00}
    };

    setupUI();
}

void RaceBettingGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\x8F\x87 RACE BETTING"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Bet type selector
    auto* typeLayout = new QHBoxLayout;
    m_winBtn = new QPushButton("WIN (1st)");
    m_winBtn->setCheckable(true);
    m_winBtn->setChecked(true);
    m_winBtn->setStyleSheet(
        "QPushButton { background: rgba(250,204,21,0.15); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.3); padding: 6px 12px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:checked { background: rgba(250,204,21,0.3); border-color: #facc15; }");
    connect(m_winBtn, &QPushButton::clicked, this, [this]() {
        m_betOnPlace = false;
        m_winBtn->setChecked(true);
        m_placeBtn->setChecked(false);
    });
    typeLayout->addWidget(m_winBtn);

    m_placeBtn = new QPushButton("PLACE (Top 3)");
    m_placeBtn->setCheckable(true);
    m_placeBtn->setStyleSheet(m_winBtn->styleSheet());
    connect(m_placeBtn, &QPushButton::clicked, this, [this]() {
        m_betOnPlace = true;
        m_placeBtn->setChecked(true);
        m_winBtn->setChecked(false);
    });
    typeLayout->addWidget(m_placeBtn);
    layout->addLayout(typeLayout);

    // Racer selection
    auto* racerBox = new QGroupBox("Select Racer");
    racerBox->setStyleSheet(
        "QGroupBox { border: 1px solid rgba(250,204,21,0.2); border-radius: 6px; "
        "margin-top: 10px; padding-top: 16px; color: #facc15; font-weight: bold; }");
    auto* racerLayout = new QVBoxLayout(racerBox);

    for (int i = 0; i < m_racers.size(); ++i) {
        auto* btn = new QPushButton(QString("#%1  %2  (Odds: %3x)")
            .arg(i + 1).arg(m_racers[i].name).arg(m_racers[i].odds, 0, 'f', 2));
        btn->setCheckable(true);
        btn->setStyleSheet(
            "QPushButton { background: rgba(26,31,46,0.6); color: #e0e0e0; text-align: left; "
            "padding: 6px 12px; border: 1px solid rgba(250,204,21,0.1); border-radius: 4px; font-size: 12px; }"
            "QPushButton:hover { background: rgba(250,204,21,0.1); }"
            "QPushButton:checked { background: rgba(250,204,21,0.2); border-color: #facc15; color: #facc15; }");
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            m_selectedRacer = i;
            for (int j = 0; j < m_racerBtns.size(); ++j)
                m_racerBtns[j]->setChecked(j == i);
            m_selectedLabel->setText(QString("Selected: %1 @ %2x")
                .arg(m_racers[i].name).arg(m_racers[i].odds, 0, 'f', 2));
        });
        m_racerBtns.append(btn);
        racerLayout->addWidget(btn);
    }
    layout->addWidget(racerBox);

    m_selectedLabel = new QLabel("Select a racer to bet on");
    m_selectedLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    m_selectedLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_selectedLabel);

    m_resultLabel = new QLabel("");
    m_resultLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setWordWrap(true);
    layout->addWidget(m_resultLabel);

    m_startBtn = new QPushButton("START RACE");
    m_startBtn->setFixedHeight(50);
    m_startBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #facc15, stop:1 #f59e0b); "
        "color: #0f1419; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #fbbf24; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_startBtn, &QPushButton::clicked, this, &RaceBettingGame::startGame);
    layout->addWidget(m_startBtn);
}

void RaceBettingGame::startGame()
{
    if (m_selectedRacer < 0) return;

    // Reset progress
    for (auto& r : m_racers) {
        r.progress = 0.0;
        r.speed = 0.5 + QRandomGenerator::global()->bounded(100) / 200.0;
        r.position = 0;
    }

    m_racing = true;
    m_startBtn->setEnabled(false);
    m_resultLabel->setText("RACING...");
    m_resultLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #facc15;");
    for (auto* btn : m_racerBtns) btn->setEnabled(false);

    m_raceTimer->start(60);
}

void RaceBettingGame::tick()
{
    bool anyFinished = false;

    for (auto& r : m_racers) {
        if (r.progress >= 100.0) continue;
        // Random speed variation
        double accel = (QRandomGenerator::global()->bounded(100) - 40) * 0.02;
        r.speed = qBound(0.2, r.speed + accel, 2.5);
        r.progress += r.speed;
        if (r.progress >= 100.0) {
            r.progress = 100.0;
            anyFinished = true;
        }
    }

    update();

    // Check if all finished or leader is at 100
    int finished = 0;
    for (const auto& r : m_racers)
        if (r.progress >= 100.0) finished++;

    if (finished >= 3 || anyFinished) {
        // Wait for top 3
        if (finished >= 3) {
            m_raceTimer->stop();
            finishRace();
        }
    }
}

void RaceBettingGame::finishRace()
{
    m_racing = false;

    // Sort by progress (descending)
    QVector<int> order;
    for (int i = 0; i < m_racers.size(); ++i) order.append(i);
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        return m_racers[a].progress > m_racers[b].progress;
    });

    for (int pos = 0; pos < order.size(); ++pos)
        m_racers[order[pos]].position = pos + 1;

    int winnerIdx = order[0];
    bool won = false;

    if (m_betOnPlace) {
        // Top 3
        won = (m_racers[m_selectedRacer].position <= 3);
    } else {
        won = (m_selectedRacer == winnerIdx);
    }

    QString resultText = QString("1st: %1 | 2nd: %2 | 3rd: %3")
        .arg(m_racers[order[0]].name, m_racers[order[1]].name, m_racers[order[2]].name);

    if (won) {
        double payout = m_betOnPlace
            ? m_racers[m_selectedRacer].odds / 3.0
            : m_racers[m_selectedRacer].odds;
        m_resultLabel->setText(QString("WIN! %1\nPayout: %2x").arg(resultText).arg(payout, 0, 'f', 2));
        m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #10eb04;");
    } else {
        m_resultLabel->setText(QString("LOSE. %1\nYour pick: #%2")
            .arg(resultText).arg(m_racers[m_selectedRacer].position));
        m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #eb0404;");
    }

    auto bets = m_engine->allBets();
    if (!bets.isEmpty())
        m_engine->settleBet(bets.last().id, won);

    m_startBtn->setEnabled(true);
    for (auto* btn : m_racerBtns) btn->setEnabled(true);
    emit gameFinished(won, 0);
}

void RaceBettingGame::resetGame()
{
    m_raceTimer->stop();
    m_racing = false;
    m_selectedRacer = -1;
    for (auto& r : m_racers) { r.progress = 0; r.speed = 0; r.position = 0; }
    for (auto* btn : m_racerBtns) { btn->setChecked(false); btn->setEnabled(true); }
    m_resultLabel->setText("");
    m_selectedLabel->setText("Select a racer to bet on");
    m_startBtn->setEnabled(true);
    update();
}

void RaceBettingGame::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!m_racing && m_racers[0].progress == 0) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw race tracks at bottom of widget
    int trackTop = height() - 180;
    int trackH = 18;
    int trackLeft = 60;
    int trackWidth = width() - 80;

    QStringList colors = {"#facc15", "#10eb04", "#3b82f6", "#eb0404",
                          "#9945ff", "#00aae4", "#f7931a", "#e0e0e0"};

    for (int i = 0; i < m_racers.size(); ++i) {
        int y = trackTop + i * (trackH + 4);

        // Track label
        QFont f("Segoe UI", 8);
        p.setFont(f);
        p.setPen(QColor("#aaa"));
        p.drawText(QRect(0, y, trackLeft - 4, trackH), Qt::AlignRight | Qt::AlignVCenter,
                   QString("#%1").arg(i + 1));

        // Track background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(26, 31, 46, 180));
        p.drawRoundedRect(trackLeft, y, trackWidth, trackH, 4, 4);

        // Progress bar
        int progW = static_cast<int>(m_racers[i].progress / 100.0 * trackWidth);
        QColor c(colors[i % colors.size()]);
        c.setAlpha(i == m_selectedRacer ? 220 : 120);
        p.setBrush(c);
        p.drawRoundedRect(trackLeft, y, progW, trackH, 4, 4);

        // Racer marker
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(colors[i % colors.size()]));
        p.drawEllipse(trackLeft + progW - 6, y + 3, 12, 12);
    }
}
