#include "BinaryOptionsGame.h"
#include "engine/ProvablyFair.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QtMath>

BinaryOptionsGame::BinaryOptionsGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("binary", "Binary Options", engine, parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &BinaryOptionsGame::tick);

    // Initialize with mock BTC price
    double basePrice = 64250.0;
    for (int i = 0; i < 20; ++i) {
        basePrice += (QRandomGenerator::global()->bounded(200) - 100);
        m_prices.append(basePrice);
    }

    setupUI();
}

void BinaryOptionsGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\x93\x8A BINARY OPTIONS"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Price display
    m_priceLabel = new QLabel(QString("BTC/USD: $%1").arg(m_prices.last(), 0, 'f', 2));
    m_priceLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: #facc15;");
    m_priceLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_priceLabel);

    // Chart area will be painted
    auto* chartArea = new QWidget;
    chartArea->setMinimumHeight(180);
    chartArea->setStyleSheet("background: transparent;");
    layout->addWidget(chartArea, 1);

    // Timer and direction
    auto* infoLayout = new QHBoxLayout;
    m_timerLabel = new QLabel("Timer: 30s");
    m_timerLabel->setStyleSheet("color: #aaa; font-size: 16px; font-weight: bold;");
    m_directionLabel = new QLabel("");
    m_directionLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    infoLayout->addWidget(m_timerLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_directionLabel);
    layout->addLayout(infoLayout);

    m_resultLabel = new QLabel("Predict: will the price go UP or DOWN?");
    m_resultLabel->setStyleSheet("color: #aaa; font-size: 14px;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_resultLabel);

    // UP/DOWN buttons
    auto* btnLayout = new QHBoxLayout;
    m_upBtn = new QPushButton(QString::fromUtf8("\xE2\xAC\x86 UP"));
    m_upBtn->setFixedHeight(55);
    m_upBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #10eb04, stop:1 #0a8f02); "
        "color: #0f1419; font-size: 18px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #10eb04; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_upBtn, &QPushButton::clicked, this, [this]() { predict(true); });
    btnLayout->addWidget(m_upBtn);

    m_downBtn = new QPushButton(QString::fromUtf8("\xE2\xAC\x87 DOWN"));
    m_downBtn->setFixedHeight(55);
    m_downBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #eb0404, stop:1 #8f0202); "
        "color: white; font-size: 18px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #eb0404; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_downBtn, &QPushButton::clicked, this, [this]() { predict(false); });
    btnLayout->addWidget(m_downBtn);
    layout->addLayout(btnLayout);

    auto* payoutInfo = new QLabel("Payout: 85% | Duration: 30 seconds");
    payoutInfo->setStyleSheet("color: #666; font-size: 11px;");
    payoutInfo->setAlignment(Qt::AlignCenter);
    layout->addWidget(payoutInfo);

    layout->addStretch();
}

void BinaryOptionsGame::predict(bool up)
{
    m_predictedUp = up;
    m_entryPrice = m_prices.last();
    m_active = true;
    m_countdown = 30;

    m_upBtn->setEnabled(false);
    m_downBtn->setEnabled(false);

    m_directionLabel->setText(up ? "Prediction: UP" : "Prediction: DOWN");
    m_directionLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;")
        .arg(up ? "#10eb04" : "#eb0404"));

    m_resultLabel->setText(QString("Entry: $%1 | Waiting...").arg(m_entryPrice, 0, 'f', 2));
    m_resultLabel->setStyleSheet("color: #facc15; font-size: 14px;");

    m_timer->start(1000);
}

void BinaryOptionsGame::tick()
{
    m_countdown--;
    m_timerLabel->setText(QString("Timer: %1s").arg(m_countdown));

    generatePrice();
    m_priceLabel->setText(QString("BTC/USD: $%1").arg(m_prices.last(), 0, 'f', 2));

    double diff = m_prices.last() - m_entryPrice;
    bool currentlyUp = diff > 0;
    m_priceLabel->setStyleSheet(QString("font-size: 32px; font-weight: bold; color: %1;")
        .arg(currentlyUp ? "#10eb04" : "#eb0404"));

    update();

    if (m_countdown <= 0) {
        m_timer->stop();
        resolve();
    }
}

void BinaryOptionsGame::generatePrice()
{
    double last = m_prices.last();
    // Random walk with slight drift
    double change = (QRandomGenerator::global()->bounded(200) - 100) * 0.5;
    // Add momentum
    if (m_prices.size() >= 2) {
        double momentum = (m_prices.last() - m_prices[m_prices.size() - 2]) * 0.3;
        change += momentum;
    }
    m_prices.append(last + change);
    if (m_prices.size() > m_maxPrices) m_prices.removeFirst();
}

void BinaryOptionsGame::resolve()
{
    m_active = false;
    double exitPrice = m_prices.last();
    bool wentUp = exitPrice > m_entryPrice;
    bool won = (m_predictedUp == wentUp);

    if (won) {
        m_resultLabel->setText(QString("WIN! Entry: $%1 Exit: $%2 (+85%)")
            .arg(m_entryPrice, 0, 'f', 2).arg(exitPrice, 0, 'f', 2));
        m_resultLabel->setStyleSheet("color: #10eb04; font-size: 14px; font-weight: bold;");
    } else {
        m_resultLabel->setText(QString("LOSS. Entry: $%1 Exit: $%2")
            .arg(m_entryPrice, 0, 'f', 2).arg(exitPrice, 0, 'f', 2));
        m_resultLabel->setStyleSheet("color: #eb0404; font-size: 14px; font-weight: bold;");
    }

    auto bets = m_engine->allBets();
    if (!bets.isEmpty())
        m_engine->settleBet(bets.last().id, won);

    m_upBtn->setEnabled(true);
    m_downBtn->setEnabled(true);
    m_timerLabel->setText("Timer: 30s");
    emit gameFinished(won, won ? 1.85 : 0);
}

void BinaryOptionsGame::startGame() {}
void BinaryOptionsGame::resetGame()
{
    m_timer->stop();
    m_active = false;
    m_countdown = 30;
    m_upBtn->setEnabled(true);
    m_downBtn->setEnabled(true);
    m_timerLabel->setText("Timer: 30s");
    m_directionLabel->setText("");
    m_resultLabel->setText("Predict: will the price go UP or DOWN?");
    m_resultLabel->setStyleSheet("color: #aaa; font-size: 14px;");
    m_priceLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: #facc15;");
    update();
}

void BinaryOptionsGame::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (m_prices.size() < 2) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Chart area
    int chartTop = 100;
    int chartBottom = height() - 200;
    int chartLeft = 20;
    int chartRight = width() - 20;
    int chartH = chartBottom - chartTop;
    int chartW = chartRight - chartLeft;

    if (chartH < 30 || chartW < 30) return;

    // Find min/max
    double minP = m_prices[0], maxP = m_prices[0];
    for (double v : m_prices) {
        if (v < minP) minP = v;
        if (v > maxP) maxP = v;
    }
    double range = maxP - minP;
    if (range < 1.0) range = 1.0;

    // Draw grid
    p.setPen(QPen(QColor(250, 204, 21, 20), 1));
    for (int i = 0; i <= 4; ++i) {
        int y = chartTop + chartH * i / 4;
        p.drawLine(chartLeft, y, chartRight, y);
    }

    // Draw entry price line
    if (m_active) {
        double entryY = chartBottom - (m_entryPrice - minP) / range * chartH;
        p.setPen(QPen(QColor(250, 204, 21, 100), 1, Qt::DashLine));
        p.drawLine(chartLeft, (int)entryY, chartRight, (int)entryY);
    }

    // Draw price line
    QPainterPath path;
    for (int i = 0; i < m_prices.size(); ++i) {
        double x = chartLeft + (double)i / (m_prices.size() - 1) * chartW;
        double y = chartBottom - (m_prices[i] - minP) / range * chartH;
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }

    bool goingUp = m_prices.last() > m_prices[m_prices.size() - 2];
    p.setPen(QPen(goingUp ? QColor("#10eb04") : QColor("#eb0404"), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Draw current price dot
    double lastX = chartRight;
    double lastY = chartBottom - (m_prices.last() - minP) / range * chartH;
    p.setPen(Qt::NoPen);
    p.setBrush(goingUp ? QColor("#10eb04") : QColor("#eb0404"));
    p.drawEllipse(QPointF(lastX, lastY), 5, 5);
}
