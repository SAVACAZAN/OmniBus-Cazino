#include "DiceGame.h"
#include "engine/ProvablyFair.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QRandomGenerator>

DiceGame::DiceGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("dice", "Dice", engine, parent)
{
    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        m_animFrame++;
        if (m_animFrame >= 15) {
            m_animTimer->stop();
            m_rolling = false;
            // Show final result
            QString hash = ProvablyFair::generateHash(
                ProvablyFair::generateServerSeed(), "client", m_engine->getPlayer().totalBets);
            m_lastRoll = ProvablyFair::hashToNumber(hash, 0, 100);
            bool won = m_lastOver ? (m_lastRoll > m_target) : (m_lastRoll < m_target);
            double multiplier = m_lastOver
                ? 99.0 / (100.0 - m_target)
                : 99.0 / m_target;
            m_resultLabel->setText(QString("Roll: %1 — %2!")
                .arg(m_lastRoll)
                .arg(won ? "WIN" : "LOSE"));
            m_resultLabel->setStyleSheet(won
                ? "font-size: 24px; font-weight: bold; color: #10eb04;"
                : "font-size: 24px; font-weight: bold; color: #eb0404;");

            // Settle the bet
            auto bets = m_engine->allBets();
            if (!bets.isEmpty())
                m_engine->settleBet(bets.last().id, won);

            m_rollOverBtn->setEnabled(true);
            m_rollUnderBtn->setEnabled(true);
            emit gameFinished(won, won ? multiplier : 0);
        }
        update();
    });
    setupUI();
}

void DiceGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // Title
    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\x8E\xB2 DICE"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Result display area (painted)
    m_resultLabel = new QLabel("Set target and roll!");
    m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #aaa;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setMinimumHeight(120);
    layout->addWidget(m_resultLabel);

    // Target slider
    auto* sliderLayout = new QHBoxLayout;
    auto* sliderLabel = new QLabel("Target:");
    sliderLabel->setStyleSheet("color: #aaa;");
    sliderLayout->addWidget(sliderLabel);

    m_targetSlider = new QSlider(Qt::Horizontal);
    m_targetSlider->setRange(2, 98);
    m_targetSlider->setValue(50);
    m_targetSlider->setStyleSheet(
        "QSlider::groove:horizontal { background: #1a1f2e; height: 8px; border-radius: 4px; }"
        "QSlider::handle:horizontal { background: #facc15; width: 20px; margin: -6px 0; border-radius: 10px; }"
        "QSlider::sub-page:horizontal { background: rgba(250,204,21,0.3); border-radius: 4px; }");
    sliderLayout->addWidget(m_targetSlider);

    m_targetLabel = new QLabel("50");
    m_targetLabel->setStyleSheet("color: #facc15; font-size: 18px; font-weight: bold; min-width: 40px;");
    sliderLayout->addWidget(m_targetLabel);
    layout->addLayout(sliderLayout);

    connect(m_targetSlider, &QSlider::valueChanged, this, [this](int val) {
        m_target = val;
        m_targetLabel->setText(QString::number(val));
        updateMultiplier();
    });

    // Info row
    auto* infoLayout = new QHBoxLayout;
    m_multiplierLabel = new QLabel("Multiplier: 1.98x");
    m_multiplierLabel->setStyleSheet("color: #facc15; font-size: 14px; font-weight: bold;");
    m_winChanceLabel = new QLabel("Win Chance: 50%");
    m_winChanceLabel->setStyleSheet("color: #aaa; font-size: 14px;");
    infoLayout->addWidget(m_multiplierLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_winChanceLabel);
    layout->addLayout(infoLayout);

    // Roll buttons
    auto* btnLayout = new QHBoxLayout;
    m_rollOverBtn = new QPushButton("ROLL OVER");
    m_rollOverBtn->setFixedHeight(50);
    m_rollOverBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #10eb04, stop:1 #0a8f02); "
        "color: #0f1419; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #10eb04; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_rollOverBtn, &QPushButton::clicked, this, [this]() { roll(true); });
    btnLayout->addWidget(m_rollOverBtn);

    m_rollUnderBtn = new QPushButton("ROLL UNDER");
    m_rollUnderBtn->setFixedHeight(50);
    m_rollUnderBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #eb0404, stop:1 #8f0202); "
        "color: white; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #eb0404; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_rollUnderBtn, &QPushButton::clicked, this, [this]() { roll(false); });
    btnLayout->addWidget(m_rollUnderBtn);

    layout->addLayout(btnLayout);
    layout->addStretch();

    updateMultiplier();
}

void DiceGame::updateMultiplier()
{
    double overMult = 99.0 / (100.0 - m_target);
    double underMult = 99.0 / m_target;
    double overChance = (100.0 - m_target);
    double underChance = m_target;

    m_multiplierLabel->setText(QString("Over: %1x | Under: %2x")
        .arg(overMult, 0, 'f', 2).arg(underMult, 0, 'f', 2));
    m_winChanceLabel->setText(QString("Over: %1% | Under: %2%")
        .arg(overChance, 0, 'f', 1).arg(underChance, 0, 'f', 1));
}

void DiceGame::roll(bool over)
{
    m_lastOver = over;
    m_rolling = true;
    m_animFrame = 0;
    m_lastRoll = -1;

    double multiplier = over ? 99.0 / (100.0 - m_target) : 99.0 / m_target;

    // The bet panel handles the bet request - we just use the last bet
    m_resultLabel->setText("Rolling...");
    m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #facc15;");
    m_rollOverBtn->setEnabled(false);
    m_rollUnderBtn->setEnabled(false);

    m_animTimer->start(80);
}

void DiceGame::startGame() { /* triggered by bet panel */ }
void DiceGame::resetGame()
{
    m_lastRoll = -1;
    m_rolling = false;
    m_resultLabel->setText("Set target and roll!");
    m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #aaa;");
    m_rollOverBtn->setEnabled(true);
    m_rollUnderBtn->setEnabled(true);
    update();
}

void DiceGame::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!m_rolling && m_lastRoll < 0) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw animated number in center
    int displayNum = m_rolling
        ? QRandomGenerator::global()->bounded(100)
        : m_lastRoll;

    QRect center(width() / 2 - 60, 60, 120, 80);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(26, 31, 46, 200));
    p.drawRoundedRect(center, 12, 12);

    QFont f("Segoe UI", 36, QFont::Bold);
    p.setFont(f);
    p.setPen(m_rolling ? QColor("#facc15") : (m_lastRoll > m_target == m_lastOver ? QColor("#10eb04") : QColor("#eb0404")));
    p.drawText(center, Qt::AlignCenter, QString::number(displayNum));
}
