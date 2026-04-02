#include "PokerGame.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>
#include <QMap>

PokerGame::PokerGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("poker", "Poker", engine, parent)
{
    setupUI();
}

void PokerGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\x83\x8F TEXAS HOLD'EM"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    m_phaseLabel = new QLabel("Place a bet to deal");
    m_phaseLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    m_phaseLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_phaseLabel);

    // AI hand
    auto* aiBox = new QWidget;
    aiBox->setStyleSheet("background: rgba(26,31,46,0.5); border-radius: 8px; padding: 8px;");
    auto* aiLay = new QVBoxLayout(aiBox);
    auto* aiTitle = new QLabel("AI OPPONENT");
    aiTitle->setStyleSheet("color: #eb0404; font-size: 11px; font-weight: bold;");
    aiLay->addWidget(aiTitle);
    m_aiLabel = new QLabel("[??] [??]");
    m_aiLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #888; letter-spacing: 4px;");
    m_aiLabel->setAlignment(Qt::AlignCenter);
    aiLay->addWidget(m_aiLabel);
    layout->addWidget(aiBox);

    // Community cards
    m_communityLabel = new QLabel("--- Community Cards ---");
    m_communityLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #facc15; "
        "background: rgba(26,31,46,0.8); border-radius: 8px; padding: 16px; letter-spacing: 6px;");
    m_communityLabel->setAlignment(Qt::AlignCenter);
    m_communityLabel->setMinimumHeight(80);
    layout->addWidget(m_communityLabel);

    // Pot
    m_potLabel = new QLabel("Pot: 0");
    m_potLabel->setStyleSheet("color: #facc15; font-size: 16px; font-weight: bold;");
    m_potLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_potLabel);

    // Player hand
    auto* playerBox = new QWidget;
    playerBox->setStyleSheet("background: rgba(26,31,46,0.5); border-radius: 8px; padding: 8px;");
    auto* pLay = new QVBoxLayout(playerBox);
    auto* pTitle = new QLabel("YOUR HAND");
    pTitle->setStyleSheet("color: #10eb04; font-size: 11px; font-weight: bold;");
    pLay->addWidget(pTitle);
    m_playerLabel = new QLabel("---");
    m_playerLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #e0e0e0; letter-spacing: 4px;");
    m_playerLabel->setAlignment(Qt::AlignCenter);
    pLay->addWidget(m_playerLabel);
    layout->addWidget(playerBox);

    // Result
    m_resultLabel = new QLabel("");
    m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_resultLabel);

    // Action buttons
    auto* btnLayout = new QHBoxLayout;
    QString btnStyle = "QPushButton { background: rgba(250,204,21,0.1); color: #facc15; "
        "font-size: 13px; font-weight: bold; border: 1px solid rgba(250,204,21,0.3); "
        "border-radius: 6px; padding: 8px 4px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.2); }"
        "QPushButton:disabled { background: #222; color: #555; border-color: #333; }";

    m_foldBtn = new QPushButton("FOLD");
    m_foldBtn->setStyleSheet(btnStyle);
    m_foldBtn->setEnabled(false);
    connect(m_foldBtn, &QPushButton::clicked, this, [this]() {
        m_resultLabel->setText("You folded. AI wins.");
        m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #eb0404;");
        m_gameActive = false;
        m_foldBtn->setEnabled(false);
        m_checkBtn->setEnabled(false);
        m_callBtn->setEnabled(false);
        m_raiseBtn->setEnabled(false);
        m_dealBtn->setEnabled(true);
        auto bets = m_engine->allBets();
        if (!bets.isEmpty()) m_engine->settleBet(bets.last().id, false);
        emit gameFinished(false, 0);
    });
    btnLayout->addWidget(m_foldBtn);

    m_checkBtn = new QPushButton("CHECK");
    m_checkBtn->setStyleSheet(btnStyle);
    m_checkBtn->setEnabled(false);
    connect(m_checkBtn, &QPushButton::clicked, this, [this]() {
        // Advance phase
        if (m_phase == 0) dealFlop();
        else if (m_phase == 1) dealTurn();
        else if (m_phase == 2) dealRiver();
        else if (m_phase == 3) showdown();
    });
    btnLayout->addWidget(m_checkBtn);

    m_callBtn = new QPushButton("CALL");
    m_callBtn->setStyleSheet(btnStyle);
    m_callBtn->setEnabled(false);
    connect(m_callBtn, &QPushButton::clicked, this, [this]() {
        // Same as check for simplified version
        if (m_phase == 0) dealFlop();
        else if (m_phase == 1) dealTurn();
        else if (m_phase == 2) dealRiver();
        else if (m_phase == 3) showdown();
    });
    btnLayout->addWidget(m_callBtn);

    m_raiseBtn = new QPushButton("RAISE");
    m_raiseBtn->setStyleSheet(btnStyle);
    m_raiseBtn->setEnabled(false);
    connect(m_raiseBtn, &QPushButton::clicked, this, [this]() {
        // Add to pot
        m_pot += m_currentBetAmount * 0.5;
        m_potLabel->setText(QString("Pot: %1 %2").arg(m_pot, 0, 'f', 2).arg(m_betCurrency));
        // Advance
        if (m_phase == 0) dealFlop();
        else if (m_phase == 1) dealTurn();
        else if (m_phase == 2) dealRiver();
        else if (m_phase == 3) showdown();
    });
    btnLayout->addWidget(m_raiseBtn);
    layout->addLayout(btnLayout);

    m_dealBtn = new QPushButton("DEAL");
    m_dealBtn->setFixedHeight(45);
    m_dealBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #facc15, stop:1 #f59e0b); "
        "color: #0f1419; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #fbbf24; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_dealBtn, &QPushButton::clicked, this, &PokerGame::startGame);
    layout->addWidget(m_dealBtn);

    layout->addStretch();
}

void PokerGame::startGame()
{
    resetGame();
    m_gameActive = true;
    m_phase = 0;
    m_currentBetAmount = 10.0; // default ante
    m_betCurrency = "EUR";
    m_pot = m_currentBetAmount * 2; // player + AI ante
    m_potLabel->setText(QString("Pot: %1 %2").arg(m_pot, 0, 'f', 2).arg(m_betCurrency));
    dealPreflop();
}

void PokerGame::dealPreflop()
{
    m_deck.reset();
    m_deck.shuffle();
    m_playerCards.clear();
    m_aiCards.clear();
    m_community.clear();

    m_playerCards.append(m_deck.deal());
    m_aiCards.append(m_deck.deal());
    m_playerCards.append(m_deck.deal());
    m_aiCards.append(m_deck.deal());

    m_phaseLabel->setText("PRE-FLOP");
    m_foldBtn->setEnabled(true);
    m_checkBtn->setEnabled(true);
    m_callBtn->setEnabled(true);
    m_raiseBtn->setEnabled(true);
    m_dealBtn->setEnabled(false);
    updateDisplay();
}

void PokerGame::dealFlop()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    m_community.append(m_deck.deal());
    m_community.append(m_deck.deal());
    m_phase = 1;
    m_phaseLabel->setText("FLOP");
    updateDisplay();
}

void PokerGame::dealTurn()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    m_phase = 2;
    m_phaseLabel->setText("TURN");
    updateDisplay();
}

void PokerGame::dealRiver()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    m_phase = 3;
    m_phaseLabel->setText("RIVER");
    updateDisplay();
}

void PokerGame::showdown()
{
    m_phase = 4;
    m_phaseLabel->setText("SHOWDOWN");

    // Reveal AI cards
    QStringList aiStr;
    for (const auto& c : m_aiCards) aiStr << c.toString();
    m_aiLabel->setText(aiStr.join("  "));
    m_aiLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #e0e0e0; letter-spacing: 4px;");

    // Evaluate
    QVector<Card> playerAll = m_playerCards + m_community;
    QVector<Card> aiAll = m_aiCards + m_community;

    int pScore = handScore(playerAll);
    int aScore = handScore(aiAll);

    HandRank pRank = evaluateHand(playerAll);
    HandRank aRank = evaluateHand(aiAll);

    bool won = pScore > aScore;
    bool tie = pScore == aScore;

    if (tie) {
        m_resultLabel->setText(QString("TIE! Both: %1").arg(rankName(pRank)));
        m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #facc15;");
        // Return half pot
        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->updateBalance(bets.last().currency, m_pot / 2.0);
    } else if (won) {
        m_resultLabel->setText(QString("YOU WIN with %1!").arg(rankName(pRank)));
        m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #10eb04;");
        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, true);
    } else {
        m_resultLabel->setText(QString("AI wins with %1").arg(rankName(aRank)));
        m_resultLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #eb0404;");
        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, false);
    }

    m_gameActive = false;
    m_foldBtn->setEnabled(false);
    m_checkBtn->setEnabled(false);
    m_callBtn->setEnabled(false);
    m_raiseBtn->setEnabled(false);
    m_dealBtn->setEnabled(true);
    emit gameFinished(won, won ? m_pot : 0);
}

PokerGame::HandRank PokerGame::evaluateHand(const QVector<Card>& cards) const
{
    if (cards.size() < 5) return HighCard;

    // Count ranks and suits
    QMap<int, int> rankCount;
    QMap<int, int> suitCount;
    QVector<int> ranks;
    for (const auto& c : cards) {
        rankCount[c.rank]++;
        suitCount[c.suit]++;
        ranks.append(c.rank);
    }
    std::sort(ranks.begin(), ranks.end());

    // Flush check
    bool hasFlush = false;
    int flushSuit = -1;
    for (auto it = suitCount.begin(); it != suitCount.end(); ++it) {
        if (it.value() >= 5) { hasFlush = true; flushSuit = it.key(); break; }
    }

    // Straight check
    bool hasStraight = false;
    QVector<int> uniqueRanks;
    for (int r : ranks) if (uniqueRanks.isEmpty() || uniqueRanks.last() != r) uniqueRanks.append(r);
    // Add ace as 14
    if (uniqueRanks.contains(1)) uniqueRanks.append(14);
    for (int i = 0; i <= (int)uniqueRanks.size() - 5; ++i) {
        if (uniqueRanks[i+4] - uniqueRanks[i] == 4) hasStraight = true;
    }

    // Count pairs/trips/quads
    int pairs = 0, trips = 0, quads = 0;
    for (auto it = rankCount.begin(); it != rankCount.end(); ++it) {
        if (it.value() == 2) pairs++;
        else if (it.value() == 3) trips++;
        else if (it.value() == 4) quads++;
    }

    if (hasFlush && hasStraight) {
        // Check for royal flush (10,J,Q,K,A of same suit)
        if (uniqueRanks.contains(14) && uniqueRanks.contains(13) &&
            uniqueRanks.contains(12) && uniqueRanks.contains(11) &&
            uniqueRanks.contains(10))
            return RoyalFlush;
        return StraightFlush;
    }
    if (quads > 0) return FourOfAKind;
    if (trips > 0 && pairs > 0) return FullHouse;
    if (hasFlush) return Flush;
    if (hasStraight) return Straight;
    if (trips > 0) return ThreeOfAKind;
    if (pairs >= 2) return TwoPair;
    if (pairs == 1) return OnePair;
    return HighCard;
}

int PokerGame::handScore(const QVector<Card>& allCards) const
{
    HandRank rank = evaluateHand(allCards);
    int score = rank * 1000;
    // Tiebreaker: highest card
    int maxRank = 0;
    for (const auto& c : allCards) {
        int r = (c.rank == Card::Ace) ? 14 : static_cast<int>(c.rank);
        if (r > maxRank) maxRank = r;
    }
    return score + maxRank;
}

QString PokerGame::rankName(HandRank rank) const
{
    switch (rank) {
        case HighCard: return "High Card";
        case OnePair: return "One Pair";
        case TwoPair: return "Two Pair";
        case ThreeOfAKind: return "Three of a Kind";
        case Straight: return "Straight";
        case Flush: return "Flush";
        case FullHouse: return "Full House";
        case FourOfAKind: return "Four of a Kind";
        case StraightFlush: return "Straight Flush";
        case RoyalFlush: return "Royal Flush";
    }
    return "Unknown";
}

void PokerGame::updateDisplay()
{
    QStringList pStr;
    for (const auto& c : m_playerCards) pStr << c.toString();
    m_playerLabel->setText(pStr.join("  "));

    if (m_phase < 4) {
        m_aiLabel->setText("[??]  [??]");
        m_aiLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #888; letter-spacing: 4px;");
    }

    QStringList cStr;
    for (const auto& c : m_community) cStr << c.toString();
    while (cStr.size() < 5) cStr << "[--]";
    m_communityLabel->setText(cStr.join("  "));
}

void PokerGame::resetGame()
{
    m_playerCards.clear();
    m_aiCards.clear();
    m_community.clear();
    m_gameActive = false;
    m_phase = 0;
    m_pot = 0;
    m_playerLabel->setText("---");
    m_aiLabel->setText("[??]  [??]");
    m_aiLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #888; letter-spacing: 4px;");
    m_communityLabel->setText("--- Community Cards ---");
    m_potLabel->setText("Pot: 0");
    m_resultLabel->setText("");
    m_phaseLabel->setText("Place a bet to deal");
    m_foldBtn->setEnabled(false);
    m_checkBtn->setEnabled(false);
    m_callBtn->setEnabled(false);
    m_raiseBtn->setEnabled(false);
    m_dealBtn->setEnabled(true);
}
