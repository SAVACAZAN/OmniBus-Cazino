#include "BetPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

BetPanel::BetPanel(BettingEngine* engine, QWidget* parent)
    : QWidget(parent), m_engine(engine)
{
    setupUI();
    updateBalance();

    connect(m_engine, &BettingEngine::balanceChanged, this, &BetPanel::updateBalance);
}

void BetPanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    setFixedWidth(280);

    // Title
    auto* title = new QLabel("PLACE BET");
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #facc15; padding: 8px;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Balance
    m_balanceLabel = new QLabel("Balance: ---");
    m_balanceLabel->setStyleSheet("font-size: 14px; color: #10eb04; padding: 4px 8px;");
    m_balanceLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_balanceLabel);

    // Currency selector
    auto* currLabel = new QLabel("Currency:");
    currLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    layout->addWidget(currLabel);

    m_currencyCombo = new QComboBox;
    m_currencyCombo->addItems({"EUR", "BTC", "ETH", "LCX"});
    m_currencyCombo->setStyleSheet(
        "QComboBox { background: #1a1f2e; color: #e0e0e0; border: 1px solid rgba(250,204,21,0.3); "
        "padding: 6px; border-radius: 4px; }"
        "QComboBox QAbstractItemView { background: #1a1f2e; color: #e0e0e0; "
        "selection-background-color: rgba(250,204,21,0.2); }");
    connect(m_currencyCombo, &QComboBox::currentTextChanged, this, &BetPanel::updateBalance);
    layout->addWidget(m_currencyCombo);

    // Bet amount
    auto* amtLabel = new QLabel("Bet Amount:");
    amtLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    layout->addWidget(amtLabel);

    m_amountSpin = new QDoubleSpinBox;
    m_amountSpin->setRange(m_engine->minBet(), m_engine->maxBet());
    m_amountSpin->setDecimals(8);
    m_amountSpin->setValue(10.0);
    m_amountSpin->setStyleSheet(
        "QDoubleSpinBox { background: #1a1f2e; color: #e0e0e0; border: 1px solid rgba(250,204,21,0.3); "
        "padding: 8px; border-radius: 4px; font-size: 16px; font-weight: bold; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 20px; }");
    layout->addWidget(m_amountSpin);

    // Preset buttons
    auto* presetLayout = new QHBoxLayout;
    m_minBtn = new QPushButton("Min");
    m_halfBtn = new QPushButton(QString::fromUtf8("\xC2\xBD")); // "1/2"
    m_doubleBtn = new QPushButton("x2");
    m_maxBtn = new QPushButton("Max");

    QString presetStyle =
        "QPushButton { background: rgba(250,204,21,0.1); color: #facc15; border: 1px solid rgba(250,204,21,0.3); "
        "padding: 6px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(250,204,21,0.2); }";

    for (auto* btn : {m_minBtn, m_halfBtn, m_doubleBtn, m_maxBtn}) {
        btn->setStyleSheet(presetStyle);
        presetLayout->addWidget(btn);
    }
    layout->addLayout(presetLayout);

    connect(m_minBtn, &QPushButton::clicked, this, [this]() {
        m_amountSpin->setValue(m_engine->minBet());
    });
    connect(m_halfBtn, &QPushButton::clicked, this, [this]() {
        m_amountSpin->setValue(m_amountSpin->value() / 2.0);
    });
    connect(m_doubleBtn, &QPushButton::clicked, this, [this]() {
        m_amountSpin->setValue(m_amountSpin->value() * 2.0);
    });
    connect(m_maxBtn, &QPushButton::clicked, this, [this]() {
        double bal = m_engine->getPlayer().balance(m_currencyCombo->currentText());
        double maxBet = qMin(bal, m_engine->maxBet());
        m_amountSpin->setValue(maxBet);
    });

    layout->addSpacing(10);

    // Place Bet button
    m_betButton = new QPushButton("PLACE BET");
    m_betButton->setFixedHeight(50);
    m_betButton->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 #facc15, stop:1 #f59e0b); color: #0f1419; font-size: 18px; "
        "font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 #fbbf24, stop:1 #f59e0b); }"
        "QPushButton:pressed { background: #d97706; }"
        "QPushButton:disabled { background: #444; color: #888; }");
    connect(m_betButton, &QPushButton::clicked, this, [this]() {
        emit betRequested(m_amountSpin->value(), m_currencyCombo->currentText());
    });
    layout->addWidget(m_betButton);

    layout->addStretch();

    // Stats section
    auto* statsBox = new QGroupBox("Session Stats");
    statsBox->setStyleSheet(
        "QGroupBox { border: 1px solid rgba(250,204,21,0.2); border-radius: 6px; "
        "margin-top: 10px; padding-top: 16px; color: #facc15; font-weight: bold; }");
    auto* statsLayout = new QVBoxLayout(statsBox);
    auto* betsLabel = new QLabel("Total Bets: 0 | Wins: 0");
    betsLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    betsLabel->setObjectName("statsLabel");
    statsLayout->addWidget(betsLabel);
    layout->addWidget(statsBox);

    connect(m_engine, &BettingEngine::betSettled, this, [this, betsLabel](const Bet&) {
        auto& p = m_engine->getPlayer();
        betsLabel->setText(QString("Bets: %1 | Wins: %2 | WR: %3%")
            .arg(p.totalBets).arg(p.totalWins).arg(p.winRate(), 0, 'f', 1));
    });
}

double BetPanel::betAmount() const
{
    return m_amountSpin->value();
}

QString BetPanel::currency() const
{
    return m_currencyCombo->currentText();
}

void BetPanel::setEnabled(bool enabled)
{
    m_betButton->setEnabled(enabled);
    m_amountSpin->setEnabled(enabled);
    m_currencyCombo->setEnabled(enabled);
}

void BetPanel::updateBalance()
{
    QString cur = m_currencyCombo->currentText();
    double bal = m_engine->getPlayer().balance(cur);
    if (cur == "BTC" || cur == "ETH")
        m_balanceLabel->setText(QString("Balance: %1 %2").arg(bal, 0, 'f', 8).arg(cur));
    else
        m_balanceLabel->setText(QString("Balance: %1 %2").arg(bal, 0, 'f', 2).arg(cur));
}
