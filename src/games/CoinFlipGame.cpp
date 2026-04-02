#include "CoinFlipGame.h"
#include "engine/ProvablyFair.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>

CoinFlipGame::CoinFlipGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("coinflip", "Coin Flip", engine, parent)
{
    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        m_animFrame++;
        if (m_animFrame >= 12) {
            m_animTimer->stop();
            m_flipping = false;

            QString hash = ProvablyFair::generateHash(
                ProvablyFair::generateServerSeed(), "coinflip",
                m_engine->getPlayer().totalBets);
            m_lastResult = ProvablyFair::hashToNumber(hash, 0, 100) >= 50;
            bool won = (m_lastChoice == m_lastResult);

            m_resultLabel->setText(m_lastResult ? "HEADS!" : "TAILS!");
            m_resultLabel->setStyleSheet(QString("font-size: 48px; font-weight: bold; color: %1;")
                .arg(won ? "#10eb04" : "#eb0404"));

            if (won) {
                m_streak++;
                m_doubleOrNothingBtn->setEnabled(true);
            } else {
                m_streak = 0;
                m_doubleOrNothingBtn->setEnabled(false);
            }
            m_streakLabel->setText(QString("Streak: %1").arg(m_streak));

            auto bets = m_engine->allBets();
            if (!bets.isEmpty())
                m_engine->settleBet(bets.last().id, won);

            m_headsBtn->setEnabled(true);
            m_tailsBtn->setEnabled(true);
            emit gameFinished(won, won ? 2.0 : 0.0);
        }
        update();
    });
    setupUI();
}

void CoinFlipGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\xAA\x99 COIN FLIP"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    m_resultLabel = new QLabel("Pick a side!");
    m_resultLabel->setStyleSheet("font-size: 36px; font-weight: bold; color: #facc15;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setMinimumHeight(150);
    layout->addWidget(m_resultLabel);

    m_streakLabel = new QLabel("Streak: 0");
    m_streakLabel->setStyleSheet("font-size: 14px; color: #facc15;");
    m_streakLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_streakLabel);

    auto* btnLayout = new QHBoxLayout;
    m_headsBtn = new QPushButton("HEADS");
    m_headsBtn->setFixedHeight(60);
    m_headsBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #10eb04, stop:1 #0a8f02); "
        "color: #0f1419; font-size: 18px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #10eb04; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_headsBtn, &QPushButton::clicked, this, [this]() { flip(true); });
    btnLayout->addWidget(m_headsBtn);

    m_tailsBtn = new QPushButton("TAILS");
    m_tailsBtn->setFixedHeight(60);
    m_tailsBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #eb0404, stop:1 #8f0202); "
        "color: white; font-size: 18px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #eb0404; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_tailsBtn, &QPushButton::clicked, this, [this]() { flip(false); });
    btnLayout->addWidget(m_tailsBtn);
    layout->addLayout(btnLayout);

    m_doubleOrNothingBtn = new QPushButton("DOUBLE OR NOTHING");
    m_doubleOrNothingBtn->setFixedHeight(45);
    m_doubleOrNothingBtn->setEnabled(false);
    m_doubleOrNothingBtn->setStyleSheet(
        "QPushButton { background: rgba(250,204,21,0.15); color: #facc15; "
        "font-size: 14px; font-weight: bold; border: 1px solid rgba(250,204,21,0.4); border-radius: 8px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.25); }"
        "QPushButton:disabled { background: #222; color: #555; border-color: #333; }");
    connect(m_doubleOrNothingBtn, &QPushButton::clicked, this, [this]() {
        flip(m_lastChoice);
    });
    layout->addWidget(m_doubleOrNothingBtn);

    layout->addStretch();
}

void CoinFlipGame::flip(bool heads)
{
    m_lastChoice = heads;
    m_flipping = true;
    m_animFrame = 0;
    m_headsBtn->setEnabled(false);
    m_tailsBtn->setEnabled(false);
    m_doubleOrNothingBtn->setEnabled(false);
    m_resultLabel->setText("Flipping...");
    m_resultLabel->setStyleSheet("font-size: 36px; font-weight: bold; color: #facc15;");
    m_animTimer->start(80);
}

void CoinFlipGame::startGame() {}
void CoinFlipGame::resetGame()
{
    m_streak = 0;
    m_flipping = false;
    m_resultLabel->setText("Pick a side!");
    m_resultLabel->setStyleSheet("font-size: 36px; font-weight: bold; color: #facc15;");
    m_streakLabel->setText("Streak: 0");
    m_headsBtn->setEnabled(true);
    m_tailsBtn->setEnabled(true);
    m_doubleOrNothingBtn->setEnabled(false);
    update();
}

void CoinFlipGame::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!m_flipping) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Animated coin
    QRect coinRect(width() / 2 - 50, 80, 100, 100);
    double scaleY = qAbs(qCos(m_animFrame * 0.5));
    int h = static_cast<int>(100 * scaleY);
    coinRect.setTop(80 + (100 - h) / 2);
    coinRect.setHeight(h);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#facc15"));
    p.drawEllipse(coinRect);

    if (scaleY > 0.3) {
        QFont f("Segoe UI", 20, QFont::Bold);
        p.setFont(f);
        p.setPen(QColor("#0f1419"));
        p.drawText(coinRect, Qt::AlignCenter, m_animFrame % 2 == 0 ? "H" : "T");
    }
}
