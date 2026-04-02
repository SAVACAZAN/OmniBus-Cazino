#include "SlotsGame.h"
#include "engine/ProvablyFair.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QRandomGenerator>

const QStringList SlotsGame::s_symbols = {"BTC", "ETH", "LCX", "SOL", "XRP", "WILD", "SCATTER"};

SlotsGame::SlotsGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("slots", "Slots", engine, parent)
{
    m_spinTimer = new QTimer(this);
    connect(m_spinTimer, &QTimer::timeout, this, [this]() {
        bool allStopped = true;
        for (int r = 0; r < REELS; ++r) {
            if (m_spinCountdown[r] > 0) {
                m_spinCountdown[r]--;
                // Randomize reel while spinning
                for (int row = 0; row < ROWS; ++row) {
                    m_grid[r][row] = s_symbols[QRandomGenerator::global()->bounded(s_symbols.size())];
                }
                allStopped = false;
            }
        }
        update();
        if (allStopped) {
            m_spinTimer->stop();
            m_spinning = false;
            // Final result via provably fair
            for (int r = 0; r < REELS; ++r) {
                for (int row = 0; row < ROWS; ++row) {
                    QString hash = ProvablyFair::generateHash(
                        ProvablyFair::generateServerSeed(), "slots",
                        r * ROWS + row + m_engine->getPlayer().totalBets * 100);
                    int idx = ProvablyFair::hashToNumber(hash, 0, s_symbols.size());
                    m_grid[r][row] = s_symbols[idx];
                }
            }
            update();
            checkWin();
            m_spinBtn->setEnabled(true);
        }
    });
    setupUI();
    resetGame();
}

void SlotsGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\x8E\xB0 CRYPTO SLOTS"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Grid will be painted
    auto* gridWidget = new QWidget;
    gridWidget->setMinimumSize(400, 200);
    gridWidget->setStyleSheet("background: rgba(26,31,46,0.8); border-radius: 8px;");
    layout->addWidget(gridWidget, 1);

    m_resultLabel = new QLabel("");
    m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_resultLabel);

    m_payoutLabel = new QLabel("Match 3+ symbols on a line to win!");
    m_payoutLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    m_payoutLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_payoutLabel);

    m_spinBtn = new QPushButton("SPIN");
    m_spinBtn->setFixedHeight(55);
    m_spinBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #facc15, stop:1 #f59e0b); "
        "color: #0f1419; font-size: 20px; font-weight: bold; border: none; border-radius: 10px; }"
        "QPushButton:hover { background: #fbbf24; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_spinBtn, &QPushButton::clicked, this, &SlotsGame::spin);
    layout->addWidget(m_spinBtn);

    layout->addStretch();
}

void SlotsGame::spin()
{
    m_spinning = true;
    m_spinBtn->setEnabled(false);
    m_resultLabel->setText("");
    m_payoutLabel->setText("Spinning...");

    // Stagger reel stops
    for (int r = 0; r < REELS; ++r) {
        m_spinCountdown[r] = 10 + r * 4; // Each reel stops later
    }
    m_spinTimer->start(80);
}

void SlotsGame::checkWin()
{
    double totalPayout = 0.0;
    QStringList winLines;

    // Check each row (horizontal paylines)
    for (int row = 0; row < ROWS; ++row) {
        // Count consecutive matching from left (with WILD substitution)
        QString firstSym = m_grid[0][row];
        if (firstSym == "SCATTER") continue; // scatter doesn't count on lines
        int matchCount = 1;
        for (int r = 1; r < REELS; ++r) {
            QString sym = m_grid[r][row];
            if (sym == firstSym || sym == "WILD" || firstSym == "WILD") {
                matchCount++;
                if (firstSym == "WILD" && sym != "WILD") firstSym = sym;
            } else break;
        }
        if (matchCount >= 3) {
            double mult = symbolMultiplier(firstSym) * (matchCount - 2);
            totalPayout += mult;
            winLines << QString("Row %1: %2x%3 = %4x").arg(row + 1).arg(firstSym).arg(matchCount).arg(mult, 0, 'f', 1);
        }
    }

    // Count scatters
    int scatterCount = 0;
    for (int r = 0; r < REELS; ++r)
        for (int row = 0; row < ROWS; ++row)
            if (m_grid[r][row] == "SCATTER") scatterCount++;

    if (scatterCount >= 3) {
        m_freeSpins += scatterCount * 3;
        winLines << QString("%1 SCATTERS = %2 Free Spins!").arg(scatterCount).arg(scatterCount * 3);
        totalPayout += 2.0;
    }

    if (totalPayout > 0) {
        m_resultLabel->setText(QString("WIN! %1x payout").arg(totalPayout, 0, 'f', 1));
        m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #10eb04;");

        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, true);
    } else {
        m_resultLabel->setText("No win");
        m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #eb0404;");
        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, false);
    }
    m_payoutLabel->setText(winLines.isEmpty() ? "Try again!" : winLines.join(" | "));
    emit gameFinished(totalPayout > 0, totalPayout);
}

double SlotsGame::symbolMultiplier(const QString& symbol) const
{
    if (symbol == "BTC") return 10.0;
    if (symbol == "ETH") return 7.0;
    if (symbol == "SOL") return 5.0;
    if (symbol == "LCX") return 4.0;
    if (symbol == "XRP") return 3.0;
    if (symbol == "WILD") return 15.0;
    return 1.0;
}

void SlotsGame::startGame() { spin(); }
void SlotsGame::resetGame()
{
    m_spinning = false;
    m_freeSpins = 0;
    for (int r = 0; r < REELS; ++r)
        for (int row = 0; row < ROWS; ++row)
            m_grid[r][row] = s_symbols[QRandomGenerator::global()->bounded(s_symbols.size())];
    m_resultLabel->setText("");
    m_payoutLabel->setText("Match 3+ symbols on a line to win!");
    m_spinBtn->setEnabled(true);
    update();
}

void SlotsGame::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw the slot grid
    int gridTop = 60;
    int cellW = (width() - 40) / REELS;
    int cellH = 50;
    int gridLeft = 20;

    for (int r = 0; r < REELS; ++r) {
        for (int row = 0; row < ROWS; ++row) {
            QRect cell(gridLeft + r * cellW, gridTop + row * cellH, cellW - 4, cellH - 4);

            // Background
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(26, 31, 46, 200));
            p.drawRoundedRect(cell, 6, 6);

            // Border
            p.setPen(QPen(QColor(250, 204, 21, 60), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(cell, 6, 6);

            // Symbol text
            QString sym = m_grid[r][row];
            QColor textColor = "#e0e0e0";
            if (sym == "BTC") textColor = QColor("#f7931a");
            else if (sym == "ETH") textColor = QColor("#627eea");
            else if (sym == "LCX") textColor = QColor("#facc15");
            else if (sym == "SOL") textColor = QColor("#9945ff");
            else if (sym == "XRP") textColor = QColor("#00aae4");
            else if (sym == "WILD") textColor = QColor("#10eb04");
            else if (sym == "SCATTER") textColor = QColor("#eb0404");

            QFont f("Segoe UI", 12, QFont::Bold);
            p.setFont(f);
            p.setPen(textColor);
            p.drawText(cell, Qt::AlignCenter, sym);
        }
    }
}
