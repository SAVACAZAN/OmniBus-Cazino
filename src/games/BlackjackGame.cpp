#include "BlackjackGame.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

BlackjackGame::BlackjackGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("blackjack", "Blackjack", engine, parent)
{
    setupUI();
}

void BlackjackGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\x83\x8F BLACKJACK"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Dealer section
    auto* dealerBox = new QWidget;
    dealerBox->setStyleSheet("background: rgba(26,31,46,0.6); border-radius: 8px; padding: 12px;");
    auto* dealerLayout = new QVBoxLayout(dealerBox);
    auto* dealerTitle = new QLabel("DEALER");
    dealerTitle->setStyleSheet("color: #facc15; font-size: 12px; font-weight: bold;");
    dealerLayout->addWidget(dealerTitle);
    m_dealerLabel = new QLabel("---");
    m_dealerLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #e0e0e0; letter-spacing: 4px;");
    m_dealerLabel->setAlignment(Qt::AlignCenter);
    m_dealerLabel->setMinimumHeight(60);
    dealerLayout->addWidget(m_dealerLabel);
    m_dealerValueLabel = new QLabel("");
    m_dealerValueLabel->setStyleSheet("color: #aaa; font-size: 14px;");
    m_dealerValueLabel->setAlignment(Qt::AlignCenter);
    dealerLayout->addWidget(m_dealerValueLabel);
    layout->addWidget(dealerBox);

    // Result
    m_resultLabel = new QLabel("");
    m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setMinimumHeight(40);
    layout->addWidget(m_resultLabel);

    // Player section
    auto* playerBox = new QWidget;
    playerBox->setStyleSheet("background: rgba(26,31,46,0.6); border-radius: 8px; padding: 12px;");
    auto* playerLayout = new QVBoxLayout(playerBox);
    auto* playerTitle = new QLabel("YOUR HAND");
    playerTitle->setStyleSheet("color: #facc15; font-size: 12px; font-weight: bold;");
    playerLayout->addWidget(playerTitle);
    m_playerLabel = new QLabel("---");
    m_playerLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #e0e0e0; letter-spacing: 4px;");
    m_playerLabel->setAlignment(Qt::AlignCenter);
    m_playerLabel->setMinimumHeight(60);
    playerLayout->addWidget(m_playerLabel);
    m_playerValueLabel = new QLabel("");
    m_playerValueLabel->setStyleSheet("color: #aaa; font-size: 14px;");
    m_playerValueLabel->setAlignment(Qt::AlignCenter);
    playerLayout->addWidget(m_playerValueLabel);
    layout->addWidget(playerBox);

    // Action buttons
    auto* btnLayout = new QHBoxLayout;

    m_hitBtn = new QPushButton("HIT");
    m_hitBtn->setFixedHeight(45);
    m_hitBtn->setEnabled(false);
    m_hitBtn->setStyleSheet(
        "QPushButton { background: rgba(16,235,4,0.15); color: #10eb04; font-size: 14px; "
        "font-weight: bold; border: 1px solid rgba(16,235,4,0.4); border-radius: 8px; }"
        "QPushButton:hover { background: rgba(16,235,4,0.25); }"
        "QPushButton:disabled { background: #222; color: #555; border-color: #333; }");
    connect(m_hitBtn, &QPushButton::clicked, this, &BlackjackGame::hit);
    btnLayout->addWidget(m_hitBtn);

    m_standBtn = new QPushButton("STAND");
    m_standBtn->setFixedHeight(45);
    m_standBtn->setEnabled(false);
    m_standBtn->setStyleSheet(
        "QPushButton { background: rgba(250,204,21,0.15); color: #facc15; font-size: 14px; "
        "font-weight: bold; border: 1px solid rgba(250,204,21,0.4); border-radius: 8px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.25); }"
        "QPushButton:disabled { background: #222; color: #555; border-color: #333; }");
    connect(m_standBtn, &QPushButton::clicked, this, &BlackjackGame::stand);
    btnLayout->addWidget(m_standBtn);

    m_doubleBtn = new QPushButton("DOUBLE");
    m_doubleBtn->setFixedHeight(45);
    m_doubleBtn->setEnabled(false);
    m_doubleBtn->setStyleSheet(
        "QPushButton { background: rgba(59,130,246,0.15); color: #3b82f6; font-size: 14px; "
        "font-weight: bold; border: 1px solid rgba(59,130,246,0.4); border-radius: 8px; }"
        "QPushButton:hover { background: rgba(59,130,246,0.25); }"
        "QPushButton:disabled { background: #222; color: #555; border-color: #333; }");
    connect(m_doubleBtn, &QPushButton::clicked, this, &BlackjackGame::doubleDown);
    btnLayout->addWidget(m_doubleBtn);

    layout->addLayout(btnLayout);

    m_dealBtn = new QPushButton("DEAL");
    m_dealBtn->setFixedHeight(50);
    m_dealBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #facc15, stop:1 #f59e0b); "
        "color: #0f1419; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #fbbf24; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_dealBtn, &QPushButton::clicked, this, &BlackjackGame::startGame);
    layout->addWidget(m_dealBtn);

    layout->addStretch();
}

void BlackjackGame::startGame()
{
    m_deck.reset();
    m_deck.shuffle();
    m_playerHand.clear();
    m_dealerHand.clear();
    m_gameActive = true;
    m_dealerRevealed = false;
    m_resultLabel->setText("");

    dealInitial();
}

void BlackjackGame::dealInitial()
{
    m_playerHand.append(m_deck.deal());
    m_dealerHand.append(m_deck.deal());
    m_playerHand.append(m_deck.deal());
    m_dealerHand.append(m_deck.deal());

    updateDisplay();

    // Check for blackjack
    int pv = handValue(m_playerHand);
    if (pv == 21) {
        m_dealerRevealed = true;
        updateDisplay();
        int dv = handValue(m_dealerHand);
        if (dv == 21) {
            m_resultLabel->setText("PUSH - Both Blackjack!");
            m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #facc15;");
            // Return bet
            auto bets = m_engine->allBets();
            if (!bets.isEmpty()) {
                m_engine->updateBalance(bets.last().currency, bets.last().amount);
            }
        } else {
            m_resultLabel->setText("BLACKJACK! 3:2 Payout!");
            m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #10eb04;");
            auto bets = m_engine->allBets();
            if (!bets.isEmpty()) {
                // BJ pays 3:2 = return bet + 1.5x bet
                m_engine->updateBalance(bets.last().currency, bets.last().amount * 2.5);
                m_engine->getPlayer().totalWins++;
            }
        }
        m_gameActive = false;
        m_hitBtn->setEnabled(false);
        m_standBtn->setEnabled(false);
        m_doubleBtn->setEnabled(false);
        m_dealBtn->setEnabled(true);
        emit gameFinished(true, 2.5);
        return;
    }

    m_hitBtn->setEnabled(true);
    m_standBtn->setEnabled(true);
    m_doubleBtn->setEnabled(m_playerHand.size() == 2);
    m_dealBtn->setEnabled(false);
}

void BlackjackGame::hit()
{
    m_playerHand.append(m_deck.deal());
    m_doubleBtn->setEnabled(false);
    updateDisplay();

    int pv = handValue(m_playerHand);
    if (pv > 21) {
        m_gameActive = false;
        m_dealerRevealed = true;
        updateDisplay();
        m_resultLabel->setText("BUST! You lose.");
        m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #eb0404;");
        m_hitBtn->setEnabled(false);
        m_standBtn->setEnabled(false);
        m_dealBtn->setEnabled(true);

        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, false);
        emit gameFinished(false, 0);
    } else if (pv == 21) {
        stand();
    }
}

void BlackjackGame::stand()
{
    m_hitBtn->setEnabled(false);
    m_standBtn->setEnabled(false);
    m_doubleBtn->setEnabled(false);
    m_dealerRevealed = true;
    dealerPlay();
}

void BlackjackGame::doubleDown()
{
    // Double the bet amount
    auto bets = m_engine->allBets();
    if (!bets.isEmpty()) {
        auto& last = bets.last();
        m_engine->updateBalance(last.currency, -last.amount); // deduct another bet
    }
    m_playerHand.append(m_deck.deal());
    updateDisplay();

    int pv = handValue(m_playerHand);
    if (pv > 21) {
        m_gameActive = false;
        m_dealerRevealed = true;
        updateDisplay();
        m_resultLabel->setText("BUST on Double! You lose.");
        m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #eb0404;");
        m_hitBtn->setEnabled(false);
        m_standBtn->setEnabled(false);
        m_doubleBtn->setEnabled(false);
        m_dealBtn->setEnabled(true);
        auto allBets = m_engine->allBets();
        if (!allBets.isEmpty())
            m_engine->settleBet(allBets.last().id, false);
        emit gameFinished(false, 0);
    } else {
        stand();
    }
}

void BlackjackGame::dealerPlay()
{
    // Dealer hits on soft 16, stands on 17+
    while (handValue(m_dealerHand) < 17) {
        m_dealerHand.append(m_deck.deal());
    }
    // Check soft 17 (ace counted as 11)
    // If dealer has exactly 17 with an ace counted as 11, it's soft 17 - some rules hit
    updateDisplay();
    evaluateHands();
}

void BlackjackGame::evaluateHands()
{
    int pv = handValue(m_playerHand);
    int dv = handValue(m_dealerHand);

    bool won = false;
    bool push = false;

    if (dv > 21) {
        m_resultLabel->setText(QString("Dealer BUSTS (%1)! You win!").arg(dv));
        m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #10eb04;");
        won = true;
    } else if (pv > dv) {
        m_resultLabel->setText(QString("You win! %1 vs %2").arg(pv).arg(dv));
        m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #10eb04;");
        won = true;
    } else if (pv == dv) {
        m_resultLabel->setText("PUSH - Tie!");
        m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #facc15;");
        push = true;
    } else {
        m_resultLabel->setText(QString("Dealer wins. %1 vs %2").arg(pv).arg(dv));
        m_resultLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #eb0404;");
    }

    m_gameActive = false;
    m_dealBtn->setEnabled(true);

    auto bets = m_engine->allBets();
    if (!bets.isEmpty()) {
        if (push) {
            // Return bet
            m_engine->updateBalance(bets.last().currency, bets.last().amount);
        } else {
            m_engine->settleBet(bets.last().id, won);
        }
    }
    emit gameFinished(won, won ? 2.0 : 0.0);
}

int BlackjackGame::handValue(const QVector<Card>& hand) const
{
    int value = 0;
    int aces = 0;
    for (const auto& c : hand) {
        if (c.rank == Card::Ace) {
            aces++;
            value += 11;
        } else {
            value += c.blackjackValue(false);
        }
    }
    while (value > 21 && aces > 0) {
        value -= 10;
        aces--;
    }
    return value;
}

QString BlackjackGame::handToString(const QVector<Card>& hand, bool hideSecond) const
{
    QStringList cards;
    for (int i = 0; i < hand.size(); ++i) {
        if (i == 1 && hideSecond)
            cards << "[??]";
        else
            cards << hand[i].toString();
    }
    return cards.join("  ");
}

void BlackjackGame::updateDisplay()
{
    m_dealerLabel->setText(handToString(m_dealerHand, !m_dealerRevealed));
    m_playerLabel->setText(handToString(m_playerHand));

    m_playerValueLabel->setText(QString("Value: %1").arg(handValue(m_playerHand)));

    if (m_dealerRevealed)
        m_dealerValueLabel->setText(QString("Value: %1").arg(handValue(m_dealerHand)));
    else {
        int firstCard = m_dealerHand.isEmpty() ? 0 : m_dealerHand[0].blackjackValue();
        m_dealerValueLabel->setText(QString("Showing: %1").arg(firstCard));
    }
}

void BlackjackGame::resetGame()
{
    m_playerHand.clear();
    m_dealerHand.clear();
    m_gameActive = false;
    m_dealerRevealed = false;
    m_dealerLabel->setText("---");
    m_playerLabel->setText("---");
    m_dealerValueLabel->setText("");
    m_playerValueLabel->setText("");
    m_resultLabel->setText("");
    m_hitBtn->setEnabled(false);
    m_standBtn->setEnabled(false);
    m_doubleBtn->setEnabled(false);
    m_dealBtn->setEnabled(true);
}
