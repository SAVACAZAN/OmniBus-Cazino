#include "CrashGame.h"
#include "engine/ProvablyFair.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

CrashGame::CrashGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("crash", "Crash", engine, parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &CrashGame::tick);
    setupUI();
}

void CrashGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\x9A\x80 CRASH"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Big multiplier display
    m_multiplierLabel = new QLabel("1.00x");
    m_multiplierLabel->setStyleSheet("font-size: 64px; font-weight: bold; color: #facc15;");
    m_multiplierLabel->setAlignment(Qt::AlignCenter);
    m_multiplierLabel->setMinimumHeight(150);
    layout->addWidget(m_multiplierLabel);

    // Buttons
    auto* btnLayout = new QHBoxLayout;
    m_startBtn = new QPushButton("START");
    m_startBtn->setFixedHeight(50);
    m_startBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #facc15, stop:1 #f59e0b); "
        "color: #0f1419; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #fbbf24; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_startBtn, &QPushButton::clicked, this, &CrashGame::startGame);
    btnLayout->addWidget(m_startBtn);

    m_cashOutBtn = new QPushButton("CASH OUT");
    m_cashOutBtn->setFixedHeight(50);
    m_cashOutBtn->setEnabled(false);
    m_cashOutBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #10eb04, stop:1 #0a8f02); "
        "color: #0f1419; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #10eb04; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_cashOutBtn, &QPushButton::clicked, this, &CrashGame::cashOut);
    btnLayout->addWidget(m_cashOutBtn);
    layout->addLayout(btnLayout);

    // History
    m_historyLabel = new QLabel("History: ---");
    m_historyLabel->setStyleSheet("color: #aaa; font-size: 12px; padding: 8px;");
    m_historyLabel->setWordWrap(true);
    layout->addWidget(m_historyLabel);

    layout->addStretch();
}

void CrashGame::generateCrashPoint()
{
    QString hash = ProvablyFair::generateHash(
        ProvablyFair::generateServerSeed(), "crash-client",
        m_engine->getPlayer().totalBets);
    double r = ProvablyFair::hashToFloat(hash);
    // House edge 1%: crash = max(1.0, 99 / (r * 100))
    if (r < 0.01) r = 0.01;
    m_crashPoint = qMax(1.0, 0.99 / r);
    if (m_crashPoint > 100.0) m_crashPoint = 100.0; // cap
}

void CrashGame::startGame()
{
    generateCrashPoint();
    m_currentMultiplier = 1.0;
    m_running = true;
    m_cashedOut = false;
    m_tickCount = 0;
    m_startBtn->setEnabled(false);
    m_cashOutBtn->setEnabled(true);
    m_multiplierLabel->setStyleSheet("font-size: 64px; font-weight: bold; color: #10eb04;");
    m_timer->start(50);
}

void CrashGame::tick()
{
    m_tickCount++;
    // Exponential growth: multiplier = e^(0.00006 * tickCount^1.5)
    m_currentMultiplier = qExp(0.00006 * qPow(m_tickCount, 1.5));
    if (m_currentMultiplier < 1.0) m_currentMultiplier = 1.0;

    m_multiplierLabel->setText(QString("%1x").arg(m_currentMultiplier, 0, 'f', 2));
    m_cashOutBtn->setText(QString("CASH OUT @ %1x").arg(m_currentMultiplier, 0, 'f', 2));

    if (m_currentMultiplier >= m_crashPoint) {
        // Crashed!
        m_timer->stop();
        m_running = false;
        m_currentMultiplier = m_crashPoint;
        m_multiplierLabel->setText(QString("CRASHED @ %1x").arg(m_crashPoint, 0, 'f', 2));
        m_multiplierLabel->setStyleSheet("font-size: 48px; font-weight: bold; color: #eb0404;");
        m_cashOutBtn->setEnabled(false);
        m_startBtn->setEnabled(true);

        m_crashHistory.prepend(m_crashPoint);
        if (m_crashHistory.size() > 10) m_crashHistory.removeLast();
        updateHistoryLabel();

        if (!m_cashedOut) {
            auto bets = m_engine->allBets();
            if (!bets.isEmpty())
                m_engine->settleBet(bets.last().id, false);
            emit gameFinished(false, 0);
        }
    }
    update();
}

void CrashGame::cashOut()
{
    if (!m_running || m_cashedOut) return;
    m_cashedOut = true;
    m_cashOutBtn->setEnabled(false);

    m_multiplierLabel->setText(QString("CASHED OUT @ %1x").arg(m_currentMultiplier, 0, 'f', 2));
    m_multiplierLabel->setStyleSheet("font-size: 48px; font-weight: bold; color: #10eb04;");

    auto bets = m_engine->allBets();
    if (!bets.isEmpty()) {
        // Override odds with actual multiplier
        auto& lastBet = bets.last();
        // Manually settle: give back amount * multiplier
        m_engine->getPlayer().balances[lastBet.currency] += lastBet.amount * m_currentMultiplier;
        m_engine->getPlayer().totalWins++;
        m_engine->getPlayer().totalProfit += lastBet.amount * (m_currentMultiplier - 1.0);
        emit m_engine->balanceChanged(lastBet.currency, m_engine->getPlayer().balance(lastBet.currency));
    }
    emit gameFinished(true, m_currentMultiplier);
}

void CrashGame::updateHistoryLabel()
{
    QStringList parts;
    for (double v : m_crashHistory) {
        QString color = v < 2.0 ? "#eb0404" : (v < 5.0 ? "#facc15" : "#10eb04");
        parts << QString("<span style='color:%1'>%2x</span>").arg(color).arg(v, 0, 'f', 2);
    }
    m_historyLabel->setText("History: " + parts.join(" | "));
}

void CrashGame::resetGame()
{
    m_timer->stop();
    m_running = false;
    m_cashedOut = false;
    m_currentMultiplier = 1.0;
    m_multiplierLabel->setText("1.00x");
    m_multiplierLabel->setStyleSheet("font-size: 64px; font-weight: bold; color: #facc15;");
    m_startBtn->setEnabled(true);
    m_cashOutBtn->setEnabled(false);
    m_cashOutBtn->setText("CASH OUT");
    update();
}

void CrashGame::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!m_running && m_tickCount == 0) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw crash curve in the background
    int baseY = height() - 60;
    int baseX = 40;
    int graphW = width() - 80;
    int graphH = height() / 2;

    p.setPen(QPen(QColor(250, 204, 21, 40), 1));
    p.drawLine(baseX, baseY, baseX + graphW, baseY);
    p.drawLine(baseX, baseY, baseX, baseY - graphH);

    if (m_tickCount > 1) {
        QPainterPath path;
        path.moveTo(baseX, baseY);
        int steps = qMin(m_tickCount, graphW);
        for (int i = 0; i <= steps; ++i) {
            double t = (double)i / steps * m_tickCount;
            double mult = qExp(0.00006 * qPow(t, 1.5));
            double y = baseY - (mult - 1.0) / (m_crashPoint) * graphH;
            double x = baseX + (double)i / steps * graphW;
            if (i == 0) path.moveTo(x, qMax((double)baseY - graphH, y));
            else path.lineTo(x, qMax((double)baseY - graphH, y));
        }
        QColor lineColor = m_cashedOut ? QColor("#10eb04") : (m_running ? QColor("#facc15") : QColor("#eb0404"));
        p.setPen(QPen(lineColor, 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
}

