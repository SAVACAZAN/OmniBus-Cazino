#include "PokerRoom.h"
#include "network/PokerClient.h"
#include "server/PokerServer.h"
#include "engine/AccountSystem.h"
#include "p2p/GameCode.h"
#include "utils/SoundEngine.h"
#include <QPainterPath>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QTimer>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHostAddress>
#include <QNetworkInterface>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Constructor ───────────────────────────────────────────────────────────────

PokerRoom::PokerRoom(BettingEngine* engine, QWidget* parent)
    : GameWidget("poker_room", "Poker Room", engine, parent)
{
    setMouseTracking(true);
    setMinimumSize(900, 600);
    setFocusPolicy(Qt::StrongFocus);

    // Initialize 9 seats
    m_players.resize(9);
    m_seatHitRects.resize(9);
    QStringList names = {"You", "Viktor", "Elena", "Carlos", "Yuki", "Marcel", "Anya", "Diego", "Fatima"};
    QStringList emojis = {
        QString::fromUtf8("\xF0\x9F\x98\x8E"),  // sunglasses
        QString::fromUtf8("\xF0\x9F\xA4\xA0"),  // cowboy
        QString::fromUtf8("\xF0\x9F\x91\xA9"),  // woman
        QString::fromUtf8("\xF0\x9F\xA7\x94"),  // beard
        QString::fromUtf8("\xF0\x9F\x91\xA7"),  // girl
        QString::fromUtf8("\xF0\x9F\x91\xB4"),  // old man
        QString::fromUtf8("\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\xA6\xB0"), // red hair
        QString::fromUtf8("\xF0\x9F\xA7\x91"),  // person
        QString::fromUtf8("\xF0\x9F\x91\xB3")   // turban
    };

    for (int i = 0; i < 9; ++i) {
        m_players[i].name = names[i];
        m_players[i].emoji = emojis[i];
        m_players[i].seatIndex = i;
        m_players[i].isHero = (i == 0);
        m_players[i].isAI = (i != 0);
        m_players[i].stack = (i == 0) ? 1000 : (500 + QRandomGenerator::global()->bounded(1500));
        // Empty seats: 6 and 8
        if (i == 6 || i == 8) {
            m_players[i].seated = false;
        }
    }
    m_heroSeat = 0;

    // AI timer - processes one AI action at a time
    m_aiTimer = new QTimer(this);
    m_aiTimer->setSingleShot(true);
    connect(m_aiTimer, &QTimer::timeout, this, &PokerRoom::processAIActions);

    // Deal timer for phase transitions
    m_dealTimer = new QTimer(this);
    m_dealTimer->setSingleShot(true);
    connect(m_dealTimer, &QTimer::timeout, this, &PokerRoom::nextPhase);

    m_raiseAmount = m_bigBlind * 2;
    m_minRaise = m_bigBlind;
    m_lastRaiseAmount = m_bigBlind;

    // Hero turn timer for offline mode (30 second countdown)
    m_heroTimer = new QTimer(this);
    m_heroTimer->setInterval(1000);
    connect(m_heroTimer, &QTimer::timeout, this, [this]() {
        if (!m_heroTurn || m_online) {
            m_heroTimer->stop();
            return;
        }
        m_heroTimerSeconds--;
        if (m_heroTimerSeconds <= 10)
            SoundEngine::instance().playTimer();
        if (m_heroTimerSeconds <= 0) {
            m_heroTimer->stop();
            heroFold(); // auto-fold on timeout
        }
        update();
    });

    // Load card PNG images from assets/cards/
    loadCardImages();

    // Create embedded QLineEdit for multiplayer join code input (hidden by default)
    m_joinCodeInput = new QLineEdit(this);
    m_joinCodeInput->setPlaceholderText("Game code or IP:port ...");
    m_joinCodeInput->setFixedSize(260, 36);
    m_joinCodeInput->setStyleSheet(
        "QLineEdit { background: rgba(20,25,35,0.95); color: #facc15; "
        "border: 2px solid rgba(250,204,21,0.4); border-radius: 8px; "
        "padding: 4px 12px; font-size: 14px; font-weight: bold; }"
        "QLineEdit:focus { border-color: #facc15; }");
    m_joinCodeInput->hide();
}

// ─── Game Flow ─────────────────────────────────────────────────────────────────

QString PokerRoom::generateSeed()
{
    // Generate 32 random bytes for seed
    QByteArray randomBytes(32, 0);
    for (int i = 0; i < 32; ++i) {
        randomBytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    // Convert to hex string as the seed
    return QString::fromLatin1(randomBytes.toHex());
}

void PokerRoom::logAction(const QString& action)
{
    QString phaseName = phaseFromInt(m_phase).toUpper();
    m_currentHandHistory.actions.append(QString("%1: %2").arg(phaseName, action));
}

void PokerRoom::startGame()
{
    // In online mode, server controls when game starts
    if (m_online) return;

    // Show buy-in screen if first time or out of chips
    if (m_needBuyIn) {
        update(); // Will draw buy-in screen in paintEvent
        return;
    }

    // Check if hero has chips
    if (m_players[m_heroSeat].stack <= 0) {
        m_needBuyIn = true; // Go back to buy-in screen
        update();
        return;
    }
    m_heroOutOfChips = false;

    resetGame();
    m_gameActive = true;
    m_phase = 0;
    m_resultText.clear();
    m_showOfflineSeed = false;
    m_lastRaiseAmount = m_bigBlind;

    // Increment hand number
    m_offlineHandNumber++;
    m_currentHandHistory = OfflineHandHistory();
    m_currentHandHistory.handNumber = m_offlineHandNumber;
    m_currentHandHistory.timestamp = QDateTime::currentDateTime();

    // Generate provably fair seed
    m_offlineSeed = generateSeed();
    m_offlineSeedHash = QString::fromLatin1(
        QCryptographicHash::hash(m_offlineSeed.toUtf8(), QCryptographicHash::Sha256).toHex());
    m_currentHandHistory.seed = m_offlineSeed;
    m_currentHandHistory.seedHash = m_offlineSeedHash;

    // Rotate dealer
    m_dealerSeat = (m_dealerSeat + 1) % 9;
    while (!m_players[m_dealerSeat].seated)
        m_dealerSeat = (m_dealerSeat + 1) % 9;

    // Post blinds
    int sbIdx = nextActivePlayer(m_dealerSeat);
    int bbIdx = nextActivePlayer(sbIdx);

    double sbAmount = qMin(m_smallBlind, m_players[sbIdx].stack);
    m_players[sbIdx].currentBet = sbAmount;
    m_players[sbIdx].stack -= sbAmount;
    m_players[sbIdx].hasBet = true;
    m_players[sbIdx].lastAction = QString("SB %1").arg(sbAmount, 0, 'f', 0);
    if (m_players[sbIdx].stack <= 0) m_players[sbIdx].allIn = true;

    double bbAmount = qMin(m_bigBlind, m_players[bbIdx].stack);
    m_players[bbIdx].currentBet = bbAmount;
    m_players[bbIdx].stack -= bbAmount;
    m_players[bbIdx].hasBet = true;
    m_players[bbIdx].lastAction = QString("BB %1").arg(bbAmount, 0, 'f', 0);
    if (m_players[bbIdx].stack <= 0) m_players[bbIdx].allIn = true;

    m_pot = sbAmount + bbAmount;
    m_currentBet = m_bigBlind;

    logAction(QString("%1 posts SB %2").arg(m_players[sbIdx].name).arg(sbAmount, 0, 'f', 0));
    logAction(QString("%1 posts BB %2").arg(m_players[bbIdx].name).arg(bbAmount, 0, 'f', 0));

    dealPreflop();

    // First to act is UTG (after BB)
    m_activePlayerIdx = nextActivePlayer(bbIdx);
    m_actionCount = 0;

    if (m_players[m_activePlayerIdx].isHero) {
        m_heroTurn = true;
        m_showActions = true;
        // Start hero timer
        m_heroTimerSeconds = 30;
        m_heroTimer->start();
    } else {
        m_heroTurn = false;
        m_showActions = false;
        m_aiTimer->start(800);
    }

    update();
}

void PokerRoom::resetGame()
{
    m_deck.reset();
    m_deck.shuffle();
    m_community.clear();
    m_pot = 0;
    m_currentBet = 0;
    m_phase = -1;
    m_heroTurn = false;
    m_showActions = false;
    m_gameActive = false;
    m_activePlayerIdx = -1;
    m_actionCount = 0;
    m_resultText.clear();
    m_raiseAmount = m_bigBlind * 2;
    m_lastRaiseAmount = m_bigBlind;
    m_sidePots.clear();
    m_showOfflineSeed = false;

    for (auto& p : m_players) {
        p.holeCards.clear();
        p.folded = false;
        p.allIn = false;
        p.hasBet = false;
        p.currentBet = 0;
        p.lastAction.clear();
    }

    m_aiTimer->stop();
    m_dealTimer->stop();
    if (m_heroTimer) m_heroTimer->stop();
    m_heroTimerSeconds = 0;
    update();
}

void PokerRoom::dealPreflop()
{
    m_deck.reset();

    // Use seeded shuffle for provably fair offline mode
    if (!m_online && !m_offlineSeed.isEmpty()) {
        m_deck.shuffleSeeded(m_offlineSeed.toUtf8());
    } else {
        m_deck.shuffle();
    }

    // Store full deck order for verification
    if (!m_online) {
        m_offlineDeckOrder = m_deck.cards();
        m_currentHandHistory.deckOrder = m_offlineDeckOrder;
    }

    m_community.clear();

    for (auto& p : m_players) {
        p.holeCards.clear();
        // Don't reset folded/allIn here -- already done in resetGame
        // Keep allIn state from blind posting
    }

    // Deal 2 cards to each seated player
    for (int round = 0; round < 2; ++round) {
        for (auto& p : m_players) {
            if (p.seated)
                p.holeCards.append(m_deck.deal());
        }
    }

    // Sound: card deal
    SoundEngine::instance().playCardDeal();

    // Store hero and AI cards in history
    if (!m_online) {
        m_currentHandHistory.playerCards = m_players[m_heroSeat].holeCards;
        // Store first active AI's cards
        for (const auto& p : m_players) {
            if (p.isAI && p.seated && !p.holeCards.isEmpty()) {
                m_currentHandHistory.aiCards = p.holeCards;
                break;
            }
        }
    }
}

void PokerRoom::dealFlop()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    m_community.append(m_deck.deal());
    m_community.append(m_deck.deal());
    SoundEngine::instance().playCardFlip();
}

void PokerRoom::dealTurn()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    SoundEngine::instance().playCardFlip();
}

void PokerRoom::dealRiver()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    SoundEngine::instance().playCardFlip();
}

void PokerRoom::collectBets()
{
    for (auto& p : m_players) {
        p.currentBet = 0;
        p.hasBet = false;
        p.lastAction.clear();
    }
    m_currentBet = 0;
    m_actionCount = 0;
}

void PokerRoom::nextPhase()
{
    collectBets();

    // Count active (non-folded, seated) players
    int active = 0;
    int lastActive = -1;
    int activeNonAllIn = 0;
    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].seated && !m_players[i].folded) {
            active++;
            lastActive = i;
            if (!m_players[i].allIn) activeNonAllIn++;
        }
    }

    if (active <= 1) {
        // Everyone folded, award pot
        if (lastActive >= 0) {
            awardPot(lastActive);
        }
        return;
    }

    // If all remaining players are all-in (or only 1 left who isn't), run out the board
    if (activeNonAllIn <= 1) {
        // Deal remaining community cards without betting rounds
        while (m_phase < 3) {
            m_phase++;
            if (m_phase == 1) dealFlop();
            else if (m_phase == 2) dealTurn();
            else if (m_phase == 3) dealRiver();
        }
        showdown();
        return;
    }

    m_phase++;
    m_lastRaiseAmount = m_bigBlind; // reset min raise for new street

    if (m_phase == 1) dealFlop();
    else if (m_phase == 2) dealTurn();
    else if (m_phase == 3) dealRiver();
    else if (m_phase >= 4) {
        showdown();
        return;
    }

    // First to act after flop is first active player after dealer
    m_activePlayerIdx = nextActivePlayer(m_dealerSeat);
    m_actionCount = 0;

    if (m_players[m_activePlayerIdx].isHero && !m_players[m_activePlayerIdx].folded && !m_players[m_activePlayerIdx].allIn) {
        m_heroTurn = true;
        m_showActions = true;
        m_heroTimerSeconds = 30;
        m_heroTimer->start();
    } else {
        m_heroTurn = false;
        m_showActions = false;
        if (!m_players[m_activePlayerIdx].folded && !m_players[m_activePlayerIdx].allIn)
            m_aiTimer->start(800);
        else
            advanceAction();
    }

    update();
}

void PokerRoom::calculateSidePots()
{
    m_sidePots.clear();

    // Collect all-in amounts to determine side pot thresholds
    QVector<double> allInAmounts;
    for (const auto& p : m_players) {
        if (p.seated && !p.folded && p.allIn) {
            // The total amount they contributed this hand (currentBet might be reset,
            // so we track via stack=0 meaning they put everything in)
            // For simplicity, we use currentBet which is their bet in the current round
            // In a full implementation we'd track cumulative bets. For now, use a simplified approach.
        }
    }

    // Simplified side pot: if any player is all-in for less than others,
    // create main pot everyone is eligible for, and side pot for the rest.
    // For this implementation, just award to best hand (already handled in awardSidePots).
    // The main pot amount is m_pot and we split by eligibility if needed.

    // Gather total bets per player across the hand (approximate from stack changes)
    // Since we don't track cumulative bets perfectly, we'll use a simplified approach:
    // Just create one main pot (all eligible) and note: full side pot implementation
    // would require tracking per-street cumulative contributions.
    SidePot mainPot;
    mainPot.amount = m_pot;
    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].seated && !m_players[i].folded) {
            mainPot.eligible.append(i);
        }
    }
    m_sidePots.append(mainPot);
}

void PokerRoom::awardSidePots()
{
    calculateSidePots();

    for (const auto& pot : m_sidePots) {
        int bestScore = -1;
        int winnerIdx = -1;
        for (int idx : pot.eligible) {
            QVector<Card> allCards = m_players[idx].holeCards + m_community;
            int score = handScore(allCards);
            if (score > bestScore) {
                bestScore = score;
                winnerIdx = idx;
            }
        }
        if (winnerIdx >= 0) {
            m_players[winnerIdx].stack += pot.amount;
        }
    }
}

void PokerRoom::saveHandHistory()
{
    m_currentHandHistory.community = m_community;
    m_currentHandHistory.potAmount = m_pot;
    m_handHistories.append(m_currentHandHistory);

    // Save to JSON file
    QJsonArray allHands;

    // Load existing file if present
    QString filePath = QCoreApplication::applicationDirPath() + "/offline_hand_history.json";
    QFile readFile(filePath);
    if (readFile.open(QIODevice::ReadOnly)) {
        QJsonDocument existingDoc = QJsonDocument::fromJson(readFile.readAll());
        readFile.close();
        if (existingDoc.isArray()) {
            allHands = existingDoc.array();
        }
    }

    // Build JSON for this hand
    QJsonObject hand;
    hand["hand_number"] = m_currentHandHistory.handNumber;
    hand["seed"] = m_currentHandHistory.seed;
    hand["seed_hash"] = m_currentHandHistory.seedHash;
    hand["timestamp"] = m_currentHandHistory.timestamp.toString(Qt::ISODate);
    hand["result"] = m_currentHandHistory.result;
    hand["pot"] = m_currentHandHistory.potAmount;

    // Player cards
    QJsonArray pCards;
    for (const auto& c : m_currentHandHistory.playerCards) pCards.append(c.toString());
    hand["player_cards"] = pCards;

    // Community cards
    QJsonArray cCards;
    for (const auto& c : m_currentHandHistory.community) cCards.append(c.toString());
    hand["community"] = cCards;

    // Actions log
    QJsonArray acts;
    for (const auto& a : m_currentHandHistory.actions) acts.append(a);
    hand["actions"] = acts;

    // Deck order (for verification)
    QJsonArray deckArr;
    for (const auto& c : m_currentHandHistory.deckOrder) deckArr.append(c.toString());
    hand["deck_order"] = deckArr;

    allHands.append(hand);

    QFile writeFile(filePath);
    if (writeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        writeFile.write(QJsonDocument(allHands).toJson(QJsonDocument::Indented));
        writeFile.close();
    }
}

void PokerRoom::showdown()
{
    m_phase = 4;
    m_heroTurn = false;
    m_showActions = false;
    if (m_heroTimer) m_heroTimer->stop();

    int bestScore = -1;
    int winnerIdx = -1;

    for (int i = 0; i < m_players.size(); ++i) {
        if (!m_players[i].seated || m_players[i].folded) continue;
        QVector<Card> allCards = m_players[i].holeCards + m_community;
        int score = handScore(allCards);
        if (score > bestScore) {
            bestScore = score;
            winnerIdx = i;
        }
    }

    if (winnerIdx >= 0) {
        // Log showdown result
        HandRank rank = HighCard;
        if (!m_players[winnerIdx].holeCards.isEmpty()) {
            QVector<Card> allCards = m_players[winnerIdx].holeCards + m_community;
            rank = evaluateHand(allCards);
        }
        QString resultStr = QString("%1 wins with %2").arg(m_players[winnerIdx].name, rankName(rank));
        if (!m_online) {
            logAction(resultStr);
            m_currentHandHistory.result = resultStr;
        }

        awardPot(winnerIdx);
    }

    // Save hand history and show seed (offline only)
    if (!m_online) {
        saveHandHistory();
        m_showOfflineSeed = true;
    }

    update();
}

void PokerRoom::awardPot(int winnerIdx)
{
    m_players[winnerIdx].stack += m_pot;
    HandRank rank = HighCard;
    if (!m_players[winnerIdx].holeCards.isEmpty()) {
        QVector<Card> allCards = m_players[winnerIdx].holeCards + m_community;
        rank = evaluateHand(allCards);
    }

    bool heroWon = m_players[winnerIdx].isHero;
    m_resultText = QString("%1 wins %2 EUR with %3!")
        .arg(m_players[winnerIdx].name)
        .arg(m_pot, 0, 'f', 0)
        .arg(rankName(rank));
    m_resultColor = heroWon ? QColor("#10eb04") : QColor("#eb0404");

    // Sound: win or lose
    if (heroWon)
        SoundEngine::instance().playWin();
    else
        SoundEngine::instance().playLose();

    m_gameActive = false;
    m_heroTurn = false;
    m_showActions = false;
    m_phase = 4;

    // Settle with engine
    auto bets = m_engine->allBets();
    if (!bets.isEmpty()) {
        m_engine->settleBet(bets.last().id, heroWon);
    }
    emit gameFinished(heroWon, heroWon ? m_pot : 0);

    m_pot = 0;
    update();
}

int PokerRoom::nextActivePlayer(int from) const
{
    for (int i = 1; i <= 9; ++i) {
        int idx = (from + i) % 9;
        if (m_players[idx].seated && !m_players[idx].folded && !m_players[idx].allIn)
            return idx;
    }
    return from; // fallback
}

bool PokerRoom::bettingRoundComplete() const
{
    int activePlayers = 0;
    for (const auto& p : m_players) {
        if (!p.seated || p.folded || p.allIn) continue;
        activePlayers++;
        if (p.currentBet != m_currentBet && !p.allIn)
            return false;
    }
    // Everyone has matched the current bet
    return m_actionCount >= activePlayers;
}

void PokerRoom::advanceAction()
{
    // Check if only one player remains
    int active = 0;
    int lastActive = -1;
    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].seated && !m_players[i].folded) {
            active++;
            lastActive = i;
        }
    }

    if (active <= 1) {
        if (lastActive >= 0)
            awardPot(lastActive);
        return;
    }

    // Check if all non-allin players have matched the bet
    if (bettingRoundComplete()) {
        m_dealTimer->start(600);
        return;
    }

    // Move to next active player
    m_activePlayerIdx = nextActivePlayer(m_activePlayerIdx);

    if (m_players[m_activePlayerIdx].isHero) {
        m_heroTurn = true;
        m_showActions = true;
        m_minRaise = qMax(m_bigBlind, m_lastRaiseAmount);
        m_raiseAmount = qMax(m_currentBet + m_minRaise, m_bigBlind * 2);
        // Cap raise amount to hero's total possible bet
        double heroMax = m_players[m_heroSeat].stack + m_players[m_heroSeat].currentBet;
        m_raiseAmount = qMin(m_raiseAmount, heroMax);
        // Start hero turn timer (offline)
        if (!m_online) {
            m_heroTimerSeconds = 30;
            m_heroTimer->start();
        }
        update();
    } else {
        m_heroTurn = false;
        m_showActions = false;
        if (m_heroTimer) m_heroTimer->stop();
        m_aiTimer->start(600 + QRandomGenerator::global()->bounded(800));
    }

    update();
}

// ─── AI Logic ──────────────────────────────────────────────────────────────────

void PokerRoom::aiTurn(int playerIdx)
{
    auto& p = m_players[playerIdx];
    if (!p.seated || p.folded || p.allIn) return;

    // Simple AI: evaluate hand strength and decide
    QVector<Card> allCards = p.holeCards + m_community;
    HandRank rank = evaluateHand(allCards);
    int strength = static_cast<int>(rank);

    double callAmount = m_currentBet - p.currentBet;
    double rng = QRandomGenerator::global()->bounded(100) / 100.0;
    double minRaiseIncrement = qMax(m_bigBlind, m_lastRaiseAmount);

    if (callAmount <= 0) {
        // Can check for free
        if (strength >= ThreeOfAKind && rng < 0.6) {
            // Raise with strong hand — respect minimum raise
            double raiseTotal = m_currentBet + minRaiseIncrement + m_bigBlind * strength;
            double toAdd = raiseTotal - p.currentBet;
            toAdd = qMin(toAdd, p.stack);
            if (toAdd <= 0) {
                p.lastAction = "Check";
                p.hasBet = true;
            } else {
                p.stack -= toAdd;
                p.currentBet += toAdd;
                m_pot += toAdd;
                double raiseIncrement = p.currentBet - m_currentBet;
                if (raiseIncrement > m_lastRaiseAmount) m_lastRaiseAmount = raiseIncrement;
                m_currentBet = p.currentBet;
                if (p.stack <= 0) {
                    p.allIn = true;
                    p.lastAction = "ALL IN";
                } else {
                    p.lastAction = QString("Raise %1").arg(p.currentBet, 0, 'f', 0);
                }
                p.hasBet = true;
                m_actionCount = 1; // reset since we raised
                logAction(QString("%1 %2").arg(p.name, p.lastAction));
                update();
                return; // don't increment m_actionCount again
            }
        } else {
            p.lastAction = "Check";
            p.hasBet = true;
        }
    } else {
        // Must call or fold (can't check when there's a bet to match)
        double foldThreshold = 0.3 - strength * 0.03;
        if (callAmount > p.stack * 0.5 && strength < OnePair) {
            foldThreshold = 0.7;
        }

        if (rng < foldThreshold && strength < OnePair) {
            p.folded = true;
            p.lastAction = "Fold";
        } else if (strength >= FullHouse && rng < 0.5 && p.stack > callAmount * 3) {
            // Re-raise with monster — respect minimum raise
            double raiseTotal = m_currentBet + minRaiseIncrement + m_bigBlind * strength;
            double toAdd = raiseTotal - p.currentBet;
            toAdd = qMin(toAdd, p.stack);
            p.stack -= toAdd;
            p.currentBet += toAdd;
            m_pot += toAdd;
            double raiseIncrement = p.currentBet - m_currentBet;
            if (raiseIncrement > m_lastRaiseAmount) m_lastRaiseAmount = raiseIncrement;
            m_currentBet = p.currentBet;
            if (p.stack <= 0) {
                p.allIn = true;
                p.lastAction = "ALL IN";
            } else {
                p.lastAction = QString("Raise %1").arg(p.currentBet, 0, 'f', 0);
            }
            p.hasBet = true;
            m_actionCount = 1;
            logAction(QString("%1 %2").arg(p.name, p.lastAction));
            update();
            return;
        } else if (callAmount >= p.stack) {
            // All in to call
            m_pot += p.stack;
            p.currentBet += p.stack;
            p.stack = 0;
            p.allIn = true;
            p.lastAction = "ALL IN";
            p.hasBet = true;
        } else {
            p.stack -= callAmount;
            p.currentBet = m_currentBet;
            m_pot += callAmount;
            p.lastAction = QString("Call %1").arg(callAmount, 0, 'f', 0);
            p.hasBet = true;
        }
    }

    logAction(QString("%1 %2").arg(p.name, p.lastAction));
    m_actionCount++;
    update();
}

void PokerRoom::processAIActions()
{
    if (!m_gameActive) return;
    if (m_activePlayerIdx < 0 || m_activePlayerIdx >= m_players.size()) return;

    auto& p = m_players[m_activePlayerIdx];
    if (p.isHero || !p.seated || p.folded || p.allIn) {
        advanceAction();
        return;
    }

    aiTurn(m_activePlayerIdx);
    advanceAction();
}

// ─── Hero Actions ──────────────────────────────────────────────────────────────

void PokerRoom::heroFold()
{
    if (!m_heroTurn) return;
    if (m_heroTimer) m_heroTimer->stop();
    SoundEngine::instance().playFold();
    if (m_online && m_client) {
        m_client->sendAction("fold");
        m_heroTurn = false;
        m_showActions = false;
        update();
        return;
    }
    auto& hero = m_players[m_heroSeat];
    hero.folded = true;
    hero.lastAction = "Fold";
    m_heroTurn = false;
    m_showActions = false;
    m_actionCount++;

    logAction("Hero Fold");

    auto bets = m_engine->allBets();
    if (!bets.isEmpty()) m_engine->settleBet(bets.last().id, false);
    emit gameFinished(false, 0);

    advanceAction();
    update();
}

void PokerRoom::heroCheck()
{
    if (!m_heroTurn) return;
    if (m_heroTimer) m_heroTimer->stop();
    SoundEngine::instance().playCheck();
    if (m_online && m_client) {
        m_client->sendAction("check");
        m_heroTurn = false;
        m_showActions = false;
        update();
        return;
    }
    auto& hero = m_players[m_heroSeat];
    double callAmount = m_currentBet - hero.currentBet;
    if (callAmount > 0) return; // Can't check when there's a bet to match

    hero.lastAction = "Check";
    hero.hasBet = true;
    m_heroTurn = false;
    m_showActions = false;
    m_actionCount++;

    logAction("Hero Check");

    advanceAction();
    update();
}

void PokerRoom::heroCall()
{
    if (!m_heroTurn) return;
    if (m_heroTimer) m_heroTimer->stop();
    SoundEngine::instance().playChipBet();
    if (m_online && m_client) {
        m_client->sendAction("call");
        m_heroTurn = false;
        m_showActions = false;
        update();
        return;
    }
    auto& hero = m_players[m_heroSeat];
    double callAmount = m_currentBet - hero.currentBet;
    if (callAmount <= 0) {
        heroCheck();
        return;
    }

    // If can't afford to call, it becomes all-in
    callAmount = qMin(callAmount, hero.stack);
    hero.stack -= callAmount;
    hero.currentBet += callAmount;
    m_pot += callAmount;

    if (hero.stack <= 0) {
        hero.allIn = true;
        hero.lastAction = "ALL IN";
        logAction(QString("Hero ALL IN (call %1)").arg(callAmount, 0, 'f', 0));
    } else {
        hero.lastAction = QString("Call %1").arg(callAmount, 0, 'f', 0);
        logAction(QString("Hero Call %1").arg(callAmount, 0, 'f', 0));
    }
    hero.hasBet = true;

    m_heroTurn = false;
    m_showActions = false;
    m_actionCount++;
    advanceAction();
    update();
}

void PokerRoom::heroRaise(double amount)
{
    if (!m_heroTurn) return;
    if (m_heroTimer) m_heroTimer->stop();
    SoundEngine::instance().playChipBet();
    if (m_online && m_client) {
        m_client->sendAction("raise", amount);
        m_heroTurn = false;
        m_showActions = false;
        update();
        return;
    }
    auto& hero = m_players[m_heroSeat];

    // Enforce minimum raise: must be at least currentBet + max(bigBlind, lastRaiseAmount)
    double minRaiseTotal = m_currentBet + qMax(m_bigBlind, m_lastRaiseAmount);
    double raiseTotal = qMax(amount, minRaiseTotal);

    double toAdd = raiseTotal - hero.currentBet;
    // Cap at stack (becomes all-in)
    toAdd = qMin(toAdd, hero.stack);

    hero.stack -= toAdd;
    hero.currentBet += toAdd;
    m_pot += toAdd;

    // Track the raise increment for min-raise calculation
    double raiseIncrement = hero.currentBet - m_currentBet;
    if (raiseIncrement > m_lastRaiseAmount) m_lastRaiseAmount = raiseIncrement;
    m_currentBet = hero.currentBet;

    if (hero.stack <= 0) {
        hero.allIn = true;
        hero.lastAction = "ALL IN";
        logAction(QString("Hero ALL IN (raise to %1)").arg(hero.currentBet, 0, 'f', 0));
    } else {
        hero.lastAction = QString("Raise %1").arg(hero.currentBet, 0, 'f', 0);
        logAction(QString("Hero Raise to %1").arg(hero.currentBet, 0, 'f', 0));
    }
    hero.hasBet = true;

    m_heroTurn = false;
    m_showActions = false;
    m_actionCount = 1; // reset since we raised
    advanceAction();
    update();
}

void PokerRoom::heroAllIn()
{
    if (!m_heroTurn) return;
    if (m_heroTimer) m_heroTimer->stop();
    SoundEngine::instance().playAllIn();
    if (m_online && m_client) {
        m_client->sendAction("allin");
        m_heroTurn = false;
        m_showActions = false;
        update();
        return;
    }
    auto& hero = m_players[m_heroSeat];

    double toAdd = hero.stack;
    hero.currentBet += toAdd;
    m_pot += toAdd;
    hero.stack = 0;
    hero.allIn = true;

    if (hero.currentBet > m_currentBet) {
        double raiseIncrement = hero.currentBet - m_currentBet;
        if (raiseIncrement > m_lastRaiseAmount) m_lastRaiseAmount = raiseIncrement;
        m_currentBet = hero.currentBet;
        m_actionCount = 1;
    } else {
        m_actionCount++;
    }

    hero.lastAction = "ALL IN";
    hero.hasBet = true;
    m_heroTurn = false;
    m_showActions = false;

    logAction(QString("Hero ALL IN %1").arg(hero.currentBet, 0, 'f', 0));

    advanceAction();
    update();
}

// ─── Hand Evaluation (from PokerGame) ──────────────────────────────────────────

PokerRoom::HandRank PokerRoom::evaluateHand(const QVector<Card>& cards) const
{
    if (cards.size() < 5) {
        // With fewer than 5 cards, still check for pairs etc.
        if (cards.size() < 2) return HighCard;
        QMap<int, int> rankCount;
        for (const auto& c : cards) rankCount[c.rank]++;
        int pairs = 0;
        for (auto it = rankCount.begin(); it != rankCount.end(); ++it) {
            if (it.value() >= 4) return FourOfAKind;
            if (it.value() >= 3) return ThreeOfAKind;
            if (it.value() == 2) pairs++;
        }
        if (pairs >= 2) return TwoPair;
        if (pairs == 1) return OnePair;
        return HighCard;
    }

    QMap<int, int> rankCount;
    QMap<int, int> suitCount;
    QVector<int> ranks;
    for (const auto& c : cards) {
        rankCount[c.rank]++;
        suitCount[c.suit]++;
        ranks.append(c.rank);
    }
    std::sort(ranks.begin(), ranks.end());

    bool hasFlush = false;
    for (auto it = suitCount.begin(); it != suitCount.end(); ++it) {
        if (it.value() >= 5) { hasFlush = true; break; }
    }

    bool hasStraight = false;
    QVector<int> uniqueRanks;
    for (int r : ranks) if (uniqueRanks.isEmpty() || uniqueRanks.last() != r) uniqueRanks.append(r);
    if (uniqueRanks.contains(1)) uniqueRanks.append(14);
    for (int i = 0; i <= (int)uniqueRanks.size() - 5; ++i) {
        if (uniqueRanks[i+4] - uniqueRanks[i] == 4) hasStraight = true;
    }

    int pairs = 0, trips = 0, quads = 0;
    for (auto it = rankCount.begin(); it != rankCount.end(); ++it) {
        if (it.value() == 2) pairs++;
        else if (it.value() == 3) trips++;
        else if (it.value() == 4) quads++;
    }

    if (hasFlush && hasStraight) {
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

int PokerRoom::handScore(const QVector<Card>& allCards) const
{
    HandRank rank = evaluateHand(allCards);
    int score = rank * 1000;
    int maxRank = 0;
    for (const auto& c : allCards) {
        int r = (c.rank == Card::Ace) ? 14 : static_cast<int>(c.rank);
        if (r > maxRank) maxRank = r;
    }
    return score + maxRank;
}

QString PokerRoom::rankName(HandRank rank) const
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

QString PokerRoom::describeHeroHand() const
{
    auto& hero = m_players[m_heroSeat];
    if (hero.holeCards.isEmpty() || hero.folded) return "";
    QVector<Card> allCards = hero.holeCards + m_community;
    HandRank rank = evaluateHand(allCards);
    return rankName(rank);
}

// ─── Seat Geometry ─────────────────────────────────────────────────────────────

QPointF PokerRoom::seatPosition(int seatIdx) const
{
    // 9 seats around the oval table
    double cx = m_tableCenter.x();
    double cy = m_tableCenter.y();
    double rx = m_tableWidth * 0.52;
    double ry = m_tableHeight * 0.52;

    static const double angles[9] = {
        270.0,  // 0: bottom center (HERO)
        225.0,  // 1: bottom-left
        180.0,  // 2: left
        135.0,  // 3: top-left
        90.0,   // 4: top center
        45.0,   // 5: top-right
        0.0,    // 6: right
        315.0,  // 7: bottom-right
        292.5,  // 8: bottom center-right
    };

    double rad = angles[seatIdx] * M_PI / 180.0;
    double x = cx + rx * cos(rad);
    double y = cy - ry * sin(rad);
    return QPointF(x, y);
}

QPointF PokerRoom::betChipPosition(int seatIdx) const
{
    QPointF seat = seatPosition(seatIdx);
    double t = 0.45;
    return QPointF(
        seat.x() + (m_tableCenter.x() - seat.x()) * t,
        seat.y() + (m_tableCenter.y() - seat.y()) * t
    );
}

QPointF PokerRoom::cardPosition(int seatIdx, int cardIdx) const
{
    QPointF seat = seatPosition(seatIdx);
    double offsetX = (cardIdx - 0.5) * 28;
    double dx = (m_tableCenter.x() - seat.x()) * 0.18;
    double dy = (m_tableCenter.y() - seat.y()) * 0.18;
    return QPointF(seat.x() + dx + offsetX, seat.y() + dy - 30);
}

// ─── Paint Event ───────────────────────────────────────────────────────────────

void PokerRoom::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Background
    p.fillRect(rect(), QColor("#0a0e14"));

    // Calculate table geometry
    double w = width();
    double h = height();
    double actionPanelH = m_showActions ? 110 : 0;
    double availH = h - actionPanelH;

    m_tableWidth = w * 0.62;
    m_tableHeight = availH * 0.50;
    m_tableCenter = QPointF(w / 2.0, availH * 0.45);
    m_tableRect = QRectF(
        m_tableCenter.x() - m_tableWidth / 2,
        m_tableCenter.y() - m_tableHeight / 2,
        m_tableWidth,
        m_tableHeight
    );

    // MULTIPLAYER SCREEN — shown before buy-in when in MP mode
    if (m_mpState == MP_MENU || m_mpState == MP_HOSTING || m_mpState == MP_JOINING) {
        drawTable(p);
        p.fillRect(rect(), QColor(0, 0, 0, 180));
        drawMultiplayerScreen(p);
        return;
    }

    // BUY-IN SCREEN — shown before playing
    if (m_needBuyIn && !m_online) {
        drawTable(p);  // Draw table in background

        // Dark overlay
        p.fillRect(rect(), QColor(0, 0, 0, 180));

        // Buy-in card
        double cardW = 350, cardH = 280;
        QRectF card(w/2 - cardW/2, h/2 - cardH/2, cardW, cardH);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 25, 35, 240));
        p.drawRoundedRect(card, 12, 12);
        p.setPen(QPen(QColor(250, 204, 21, 80), 2));
        p.drawRoundedRect(card, 12, 12);

        // Title
        p.setFont(QFont("Arial", 18, QFont::Bold));
        p.setPen(QColor("#facc15"));
        p.drawText(QRectF(card.left(), card.top() + 15, cardW, 30), Qt::AlignCenter, "BUY IN");

        // Balance info
        double totalBalance = m_engine ? m_engine->getPlayer().balance("EUR") : 10000;
        p.setFont(QFont("Arial", 11));
        p.setPen(QColor("#888"));
        p.drawText(QRectF(card.left(), card.top() + 50, cardW, 20), Qt::AlignCenter,
                   QString("Your balance: %1 EUR").arg(totalBalance, 0, 'f', 0));

        // Buy-in amount display
        p.setFont(QFont("Arial Black", 28, QFont::Black));
        p.setPen(QColor("#facc15"));
        p.drawText(QRectF(card.left(), card.top() + 80, cardW, 40), Qt::AlignCenter,
                   QString("%1 EUR").arg(m_buyInAmount, 0, 'f', 0));

        // Slider track
        double sliderX = card.left() + 30;
        double sliderW = cardW - 60;
        double sliderY = card.top() + 135;
        m_buyInSliderRect = QRectF(sliderX, sliderY - 8, sliderW, 16);

        // Track background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(60, 60, 80));
        p.drawRoundedRect(m_buyInSliderRect, 4, 4);

        // Track fill
        double pct = (m_buyInAmount - m_minBuyIn) / (m_maxBuyIn - m_minBuyIn);
        p.setBrush(QColor("#facc15"));
        p.drawRoundedRect(QRectF(sliderX, sliderY - 8, sliderW * pct, 16), 4, 4);

        // Thumb
        double thumbX = sliderX + sliderW * pct;
        p.setBrush(QColor("#fff"));
        p.setPen(QPen(QColor("#facc15"), 2));
        p.drawEllipse(QPointF(thumbX, sliderY), 10, 10);

        // Min / Max labels
        p.setFont(QFont("Arial", 9));
        p.setPen(QColor("#666"));
        m_buyInMinRect = QRectF(sliderX, sliderY + 14, 60, 20);
        m_buyInMaxRect = QRectF(sliderX + sliderW - 60, sliderY + 14, 60, 20);
        p.drawText(m_buyInMinRect, Qt::AlignLeft, QString("Min: %1").arg(m_minBuyIn, 0, 'f', 0));
        p.drawText(m_buyInMaxRect, Qt::AlignRight, QString("Max: %1").arg(m_maxBuyIn, 0, 'f', 0));

        // Preset buttons
        double btnY = card.top() + 180;
        double btnW = 60, btnH = 28, btnGap = 8;
        double startX = card.left() + (cardW - 4*btnW - 3*btnGap) / 2;
        QVector<double> presets = {100, 500, 1000, 5000};
        QVector<QString> presetLabels = {"100", "500", "1K", "5K"};
        for (int i = 0; i < 4; i++) {
            QRectF btn(startX + i * (btnW + btnGap), btnY, btnW, btnH);
            bool active = qAbs(m_buyInAmount - presets[i]) < 1;
            p.setPen(Qt::NoPen);
            p.setBrush(active ? QColor("#facc15") : QColor(40, 45, 60));
            p.drawRoundedRect(btn, 4, 4);
            p.setFont(QFont("Arial", 10, QFont::Bold));
            p.setPen(active ? QColor("#0a0e14") : QColor("#888"));
            p.drawText(btn, Qt::AlignCenter, presetLabels[i]);
        }

        // PLAY button
        m_buyInPlayBtnRect = QRectF(card.left() + 40, card.top() + 225, cardW - 80, 40);
        QLinearGradient playGrad(m_buyInPlayBtnRect.topLeft(), m_buyInPlayBtnRect.topRight());
        playGrad.setColorAt(0, QColor("#facc15"));
        playGrad.setColorAt(1, QColor("#f59e0b"));
        p.setPen(Qt::NoPen);
        p.setBrush(playGrad);
        p.drawRoundedRect(m_buyInPlayBtnRect, 6, 6);
        p.setFont(QFont("Arial Black", 14, QFont::Black));
        p.setPen(QColor("#0a0e14"));
        p.drawText(m_buyInPlayBtnRect, Qt::AlignCenter, "SIT DOWN & PLAY");

        return; // Don't draw rest of table
    }

    drawTable(p);
    drawCommunityCards(p);
    drawPot(p);
    drawDealerButton(p);

    // Draw hand number in online mode or offline mode
    if (m_online && m_currentHandNumber > 0) {
        drawHandNumberDisplay(p);
    } else if (!m_online && m_offlineHandNumber > 0) {
        // Reuse hand number display for offline
        p.save();
        QString handText = QString("Hand #%1").arg(m_offlineHandNumber);
        QFont handFont("Segoe UI", 10, QFont::Bold);
        p.setFont(handFont);
        QFontMetrics hfm(handFont);
        double htw = hfm.horizontalAdvance(handText) + 16;
        double hth = 22;
        QRectF handRect(w - htw - 10, 10, htw, hth);
        QPainterPath handPath;
        handPath.addRoundedRect(handRect, 6, 6);
        p.fillPath(handPath, QColor(0, 0, 0, 140));
        p.setPen(QPen(QColor("#facc15"), 1));
        p.drawPath(handPath);
        p.setPen(QColor("#facc15"));
        p.drawText(handRect, Qt::AlignCenter, handText);
        p.restore();
    }

    // Draw players
    for (int i = 0; i < m_players.size(); ++i) {
        if (!m_players[i].seated) {
            drawEmptySeat(p, i);
        } else {
            drawPlayer(p, m_players[i], i);
            if (m_players[i].currentBet > 0) {
                drawPlayerBet(p, i, m_players[i].currentBet);
            }
            // Draw timer ring for active player in online mode
            if (m_online && i == m_timerSeat && m_timerSecondsLeft > 0 && m_gameActive) {
                drawTimerRing(p, i);
            }
            // Draw timer ring for hero in offline mode
            if (!m_online && i == m_heroSeat && m_heroTurn && m_heroTimerSeconds > 0 && m_gameActive) {
                // Temporarily set timer values for the drawing function
                int savedSeat = m_timerSeat;
                int savedSeconds = m_timerSecondsLeft;
                m_timerSeat = m_heroSeat;
                m_timerSecondsLeft = m_heroTimerSeconds;
                drawTimerRing(p, i);
                m_timerSeat = savedSeat;
                m_timerSecondsLeft = savedSeconds;
            }
        }
    }

    drawHandStrength(p);

    if (m_showActions && m_heroTurn) {
        drawActionPanel(p);
    }

    // Provably fair badge in online mode
    if (m_online && m_handVerified) {
        drawProvablyFairBadge(p);
    }

    // Provably fair seed hash display for offline mode (during active hand)
    if (!m_online && m_gameActive && !m_offlineSeedHash.isEmpty()) {
        p.save();
        QString hashText = QString("Seed Hash: %1...").arg(m_offlineSeedHash.left(16));
        QFont hashFont("Consolas", 8);
        p.setFont(hashFont);
        QFontMetrics hfm2(hashFont);
        double htw2 = hfm2.horizontalAdvance(hashText) + 14;
        QRectF hashRect(10, 10, htw2, 18);
        QPainterPath hashPath;
        hashPath.addRoundedRect(hashRect, 4, 4);
        p.fillPath(hashPath, QColor(0, 0, 0, 140));
        p.setPen(QColor("#facc15"));
        p.drawText(hashRect, Qt::AlignCenter, hashText);
        p.restore();
    }

    // Result banner
    if (!m_resultText.isEmpty()) {
        p.save();
        QRectF bannerRect(w * 0.15, availH * 0.38, w * 0.7, 50);
        QPainterPath bannerPath;
        bannerPath.addRoundedRect(bannerRect, 14, 14);
        p.fillPath(bannerPath, QColor(0, 0, 0, 200));
        p.setPen(QPen(m_resultColor, 2));
        p.drawPath(bannerPath);
        p.setFont(QFont("Segoe UI", 16, QFont::Bold));
        p.setPen(m_resultColor);
        p.drawText(bannerRect, Qt::AlignCenter, m_resultText);
        p.restore();

        // In offline mode: show seed reveal text below banner after showdown
        if (!m_online && m_showOfflineSeed && !m_offlineSeed.isEmpty()) {
            p.save();
            QString seedText = QString("Seed: %1").arg(m_offlineSeed.left(24) + "...");
            QFont seedFont("Consolas", 9);
            p.setFont(seedFont);
            QFontMetrics sfm2(seedFont);
            double stw2 = sfm2.horizontalAdvance(seedText) + 20;
            QRectF seedRect2(w / 2 - stw2 / 2, availH * 0.38 + 56, stw2, 20);
            QPainterPath seedPath2;
            seedPath2.addRoundedRect(seedRect2, 6, 6);
            p.fillPath(seedPath2, QColor(0, 0, 0, 160));
            p.setPen(QColor("#10eb04"));
            p.drawText(seedRect2, Qt::AlignCenter, seedText);
            p.restore();
        }

        // In online mode: show server seed reveal text below banner
        if (m_online && !m_revealedServerSeed.isEmpty()) {
            p.save();
            QString seedText = QString("Server Seed: %1...").arg(m_revealedServerSeed.left(16));
            QFont seedFont("Consolas", 9);
            p.setFont(seedFont);
            QFontMetrics sfm(seedFont);
            double stw = sfm.horizontalAdvance(seedText) + 20;
            QRectF seedRect(w / 2 - stw / 2, availH * 0.38 + 56, stw, 20);
            QPainterPath seedPath;
            seedPath.addRoundedRect(seedRect, 6, 6);
            p.fillPath(seedPath, QColor(0, 0, 0, 160));
            p.setPen(m_handVerified ? QColor("#10eb04") : QColor("#facc15"));
            p.drawText(seedRect, Qt::AlignCenter, seedText);
            p.restore();
        }

        // Deal again button (OFFLINE ONLY)
        if (!m_online) {
            double bw = 160, bh = 40;
            m_dealBtnRect = QRectF(w / 2 - bw / 2, availH * 0.38 + 80, bw, bh);
            QPainterPath dealPath;
            dealPath.addRoundedRect(m_dealBtnRect, 10, 10);

            bool hover = m_dealBtnRect.contains(m_mousePos);
            QLinearGradient dealGrad(m_dealBtnRect.topLeft(), m_dealBtnRect.topRight());
            dealGrad.setColorAt(0, hover ? QColor("#fbbf24") : QColor("#facc15"));
            dealGrad.setColorAt(1, hover ? QColor("#f5a623") : QColor("#f59e0b"));
            p.fillPath(dealPath, dealGrad);
            p.setFont(QFont("Segoe UI", 14, QFont::Bold));
            p.setPen(QColor("#0f1419"));
            p.drawText(m_dealBtnRect, Qt::AlignCenter, "DEAL AGAIN");
        } else {
            m_dealBtnRect = QRectF();
        }
    } else {
        m_dealBtnRect = QRectF();
    }

    // If waiting, show deal button (offline) or waiting text (online)
    if (m_phase == -1 && m_resultText.isEmpty()) {
        if (m_online) {
            // Online mode: show waiting overlay, no deal button
            drawWaitingOverlay(p);
        } else if (m_heroOutOfChips) {
            // Out of chips overlay
            p.save();
            QRectF chipOverlay(w * 0.2, availH * 0.35, w * 0.6, 80);
            QPainterPath overlayPath;
            overlayPath.addRoundedRect(chipOverlay, 14, 14);
            p.fillPath(overlayPath, QColor(0, 0, 0, 210));
            p.setPen(QPen(QColor("#eb0404"), 2));
            p.drawPath(overlayPath);

            p.setFont(QFont("Segoe UI", 16, QFont::Bold));
            p.setPen(QColor("#eb0404"));
            p.drawText(QRectF(chipOverlay.x(), chipOverlay.y() + 8, chipOverlay.width(), 30),
                       Qt::AlignCenter, "OUT OF CHIPS!");

            // Rebuy button
            double rbW = 140, rbH = 32;
            QRectF rebuyRect(w / 2 - rbW / 2, chipOverlay.y() + 44, rbW, rbH);
            m_dealBtnRect = rebuyRect;  // reuse deal btn for click handling
            QPainterPath rebuyPath;
            rebuyPath.addRoundedRect(rebuyRect, 8, 8);
            bool rebuyHover = rebuyRect.contains(m_mousePos);
            p.fillPath(rebuyPath, rebuyHover ? QColor("#10eb04") : QColor("#0a8f04"));
            p.setPen(QPen(QColor("#10eb04"), 1));
            p.drawPath(rebuyPath);
            p.setFont(QFont("Segoe UI", 12, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(rebuyRect, Qt::AlignCenter, "REBUY 1000");
            p.restore();
        } else {
            double bw = 200, bh = 50;
            m_dealBtnRect = QRectF(w / 2 - bw / 2, availH * 0.45, bw, bh);
            QPainterPath dealPath;
            dealPath.addRoundedRect(m_dealBtnRect, 12, 12);

            bool hover = m_dealBtnRect.contains(m_mousePos);
            QLinearGradient dealGrad(m_dealBtnRect.topLeft(), m_dealBtnRect.topRight());
            dealGrad.setColorAt(0, hover ? QColor("#fbbf24") : QColor("#facc15"));
            dealGrad.setColorAt(1, hover ? QColor("#f5a623") : QColor("#f59e0b"));
            p.fillPath(dealPath, dealGrad);
            p.setPen(QPen(QColor("#b8860b"), 2));
            p.drawPath(dealPath);
            p.setFont(QFont("Segoe UI", 18, QFont::Bold));
            p.setPen(QColor("#0f1419"));
            p.drawText(m_dealBtnRect, Qt::AlignCenter, "DEAL");
        }
    }

    // Rules text at bottom (offline mode)
    if (!m_online) {
        p.save();
        QString rulesText;
        if (m_gameActive && m_heroTurn && m_heroTimerSeconds > 0) {
            rulesText = QString("Texas Hold'em | Blinds %1/%2 | Min Raise %3 | Timer: %4s")
                .arg(m_smallBlind, 0, 'f', 0)
                .arg(m_bigBlind, 0, 'f', 0)
                .arg(qMax(m_bigBlind, m_lastRaiseAmount), 0, 'f', 0)
                .arg(m_heroTimerSeconds);
        } else {
            rulesText = QString("Texas Hold'em | Blinds %1/%2 | Min Raise 2xBB | 30s Timer")
                .arg(m_smallBlind, 0, 'f', 0)
                .arg(m_bigBlind, 0, 'f', 0);
        }
        QFont rulesFont("Segoe UI", 8);
        p.setFont(rulesFont);
        QFontMetrics rfm(rulesFont);
        double rtw = rfm.horizontalAdvance(rulesText) + 16;
        double ruleY = h - 18;
        if (m_showActions) ruleY = h - 118;
        QRectF rulesRect(w / 2 - rtw / 2, ruleY, rtw, 16);
        p.setPen(QColor(255, 255, 255, 80));
        p.drawText(rulesRect, Qt::AlignCenter, rulesText);
        p.restore();
    }
}

// ─── Draw Timer Ring ──────────────────────────────────────────────────────────

void PokerRoom::drawTimerRing(QPainter& p, int seatIdx)
{
    p.save();

    QPointF pos = seatPosition(seatIdx);
    double r = 30.0;
    double fraction = m_timerSecondsLeft / 30.0; // 30s total

    // Color: green -> yellow -> red
    QColor timerColor;
    if (fraction > 0.5) timerColor = QColor("#10eb04");
    else if (fraction > 0.2) timerColor = QColor("#facc15");
    else timerColor = QColor("#eb0404");

    // Draw arc (starts at top, goes clockwise)
    int spanAngle = static_cast<int>(fraction * 360 * 16); // in 1/16th degrees
    int startAngle = 90 * 16; // start at top

    p.setPen(QPen(timerColor, 3));
    p.setBrush(Qt::NoBrush);
    QRectF arcRect(pos.x() - r, pos.y() - r, r * 2, r * 2);
    p.drawArc(arcRect, startAngle, spanAngle);

    // Seconds text
    p.setFont(QFont("Segoe UI", 7, QFont::Bold));
    p.setPen(timerColor);
    p.drawText(QRectF(pos.x() - 15, pos.y() + r + 1, 30, 12),
               Qt::AlignCenter, QString::number(m_timerSecondsLeft));

    p.restore();
}

// ─── Draw Hand Number ─────────────────────────────────────────────────────────

void PokerRoom::drawHandNumberDisplay(QPainter& p)
{
    p.save();

    QString text = QString("Hand #%1").arg(m_currentHandNumber);
    QFont font("Segoe UI", 10, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);
    double tw = fm.horizontalAdvance(text) + 16;
    double th = 22;
    QRectF rect(width() - tw - 10, 10, tw, th);

    QPainterPath path;
    path.addRoundedRect(rect, 6, 6);
    p.fillPath(path, QColor(0, 0, 0, 140));
    p.setPen(QPen(QColor("#facc15"), 1));
    p.drawPath(path);
    p.setPen(QColor("#facc15"));
    p.drawText(rect, Qt::AlignCenter, text);

    p.restore();
}

// ─── Provably Fair Badge ──────────────────────────────────────────────────────

void PokerRoom::drawProvablyFairBadge(QPainter& p)
{
    p.save();

    // Small badge in top-left corner
    QFont font("Segoe UI", 9, QFont::Bold);
    p.setFont(font);
    QString text = "VERIFIED";
    QFontMetrics fm(font);
    double tw = fm.horizontalAdvance(text) + 30;
    double th = 22;
    QRectF rect(10, 10, tw, th);

    QPainterPath path;
    path.addRoundedRect(rect, 6, 6);
    p.fillPath(path, QColor(0, 80, 0, 180));
    p.setPen(QPen(QColor("#10eb04"), 1));
    p.drawPath(path);

    // Lock icon (unicode)
    p.setFont(QFont("Segoe UI Emoji", 10));
    p.setPen(QColor("#10eb04"));
    p.drawText(QRectF(rect.x() + 4, rect.y(), 16, th), Qt::AlignVCenter, QString::fromUtf8("\xF0\x9F\x94\x92"));

    // Text
    p.setFont(font);
    p.setPen(QColor("#10eb04"));
    p.drawText(QRectF(rect.x() + 22, rect.y(), tw - 26, th), Qt::AlignVCenter, text);

    p.restore();
}

// ─── Draw Table ────────────────────────────────────────────────────────────────

void PokerRoom::drawTable(QPainter& p)
{
    p.save();

    double cx = m_tableCenter.x();
    double cy = m_tableCenter.y();
    double rw = m_tableWidth / 2;
    double rh = m_tableHeight / 2;

    // Outer shadow
    for (int i = 4; i >= 1; --i) {
        QPainterPath shadow;
        shadow.addEllipse(QPointF(cx, cy + 2), rw + i * 3, rh + i * 3);
        p.fillPath(shadow, QColor(0, 0, 0, 20 * i));
    }

    // Wood border (brown)
    QPainterPath borderPath;
    borderPath.addEllipse(QPointF(cx, cy), rw + 12, rh + 12);
    QRadialGradient woodGrad(cx, cy, rw + 12);
    woodGrad.setColorAt(0.85, QColor("#5a3a1a"));
    woodGrad.setColorAt(0.92, QColor("#3a2010"));
    woodGrad.setColorAt(1.0, QColor("#2a1508"));
    p.fillPath(borderPath, woodGrad);

    // Wood grain lines
    p.setPen(QPen(QColor(90, 60, 30, 40), 1));
    for (int i = 0; i < 12; ++i) {
        double angle = i * 30.0 * M_PI / 180.0;
        double x1 = cx + (rw + 2) * cos(angle);
        double y1 = cy + (rh + 2) * sin(angle);
        double x2 = cx + (rw + 12) * cos(angle);
        double y2 = cy + (rh + 12) * sin(angle);
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // Inner felt
    QPainterPath feltPath;
    feltPath.addEllipse(QPointF(cx, cy), rw, rh);
    QRadialGradient feltGrad(cx, cy - rh * 0.1, rw * 1.1);
    // Brighter felt — better card contrast
    feltGrad.setColorAt(0.0, QColor("#237a2e"));
    feltGrad.setColorAt(0.4, QColor("#1e6b25"));
    feltGrad.setColorAt(0.8, QColor("#165a1c"));
    feltGrad.setColorAt(1.0, QColor("#0f4a14"));
    p.fillPath(feltPath, feltGrad);

    // Felt texture (subtle dots)
    p.setPen(Qt::NoPen);
    QRandomGenerator texRng(42);
    for (int i = 0; i < 200; ++i) {
        double angle = texRng.bounded(3600) / 10.0 * M_PI / 180.0;
        double dist = texRng.bounded(1000) / 1000.0;
        double tx = cx + rw * dist * cos(angle);
        double ty = cy + rh * dist * sin(angle);
        p.setBrush(QColor(255, 255, 255, texRng.bounded(3, 8)));
        p.drawEllipse(QPointF(tx, ty), 1.0, 1.0);
    }

    // Gold inner line
    QPainterPath innerLine;
    innerLine.addEllipse(QPointF(cx, cy), rw - 6, rh - 6);
    p.setPen(QPen(QColor(250, 204, 21, 38), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(innerLine);

    // Second subtle inner line
    QPainterPath innerLine2;
    innerLine2.addEllipse(QPointF(cx, cy), rw - 2, rh - 2);
    p.setPen(QPen(QColor(250, 204, 21, 15), 0.5));
    p.drawPath(innerLine2);

    p.restore();
}

// ─── Draw Player ───────────────────────────────────────────────────────────────

void PokerRoom::drawPlayer(QPainter& p, const PokerPlayer& player, int seatIdx)
{
    p.save();

    QPointF pos = seatPosition(seatIdx);
    double avatarR = 25.0;
    bool isActive = (seatIdx == m_activePlayerIdx && m_gameActive);

    // Opacity for folded players
    if (player.folded) {
        p.setOpacity(0.4);
    }

    // Avatar background circle
    QPainterPath avatarPath;
    avatarPath.addEllipse(pos, avatarR, avatarR);

    // Glow for active player
    if (isActive && !player.folded) {
        for (int i = 3; i >= 1; --i) {
            QPainterPath glow;
            glow.addEllipse(pos, avatarR + i * 4, avatarR + i * 4);
            p.fillPath(glow, QColor(250, 204, 21, 25 * i));
        }
        p.setPen(QPen(QColor("#facc15"), 3));
    } else {
        p.setPen(QPen(QColor(100, 100, 100, 150), 2));
    }

    // Avatar fill
    QRadialGradient avatarGrad(pos, avatarR);
    if (player.isHero) {
        avatarGrad.setColorAt(0, QColor("#2a3a5a"));
        avatarGrad.setColorAt(1, QColor("#1a2a40"));
    } else {
        avatarGrad.setColorAt(0, QColor("#2a2a3a"));
        avatarGrad.setColorAt(1, QColor("#1a1a2a"));
    }
    p.setBrush(avatarGrad);
    p.drawPath(avatarPath);

    // Emoji
    p.setFont(QFont("Segoe UI Emoji", 18));
    p.setPen(Qt::white);
    p.drawText(QRectF(pos.x() - avatarR, pos.y() - avatarR, avatarR * 2, avatarR * 2),
               Qt::AlignCenter, player.emoji);

    // Name
    p.setFont(QFont("Segoe UI", 10, QFont::Bold));
    p.setPen(Qt::white);
    p.drawText(QRectF(pos.x() - 50, pos.y() + avatarR + 2, 100, 16),
               Qt::AlignCenter, player.name);

    // Stack
    p.setFont(QFont("Segoe UI", 9));
    p.setPen(QColor("#facc15"));
    QString stackText = player.allIn ? "ALL IN" : QString("%1 EUR").arg(player.stack, 0, 'f', 0);
    p.drawText(QRectF(pos.x() - 50, pos.y() + avatarR + 17, 100, 14),
               Qt::AlignCenter, stackText);

    // Action badge
    if (!player.lastAction.isEmpty() && m_gameActive) {
        QColor badgeColor;
        if (player.lastAction.startsWith("Fold")) badgeColor = QColor("#888888");
        else if (player.lastAction.startsWith("Check")) badgeColor = QColor("#10eb04");
        else if (player.lastAction.startsWith("Call")) badgeColor = QColor("#3b82f6");
        else if (player.lastAction.startsWith("Raise")) badgeColor = QColor("#facc15");
        else if (player.lastAction.startsWith("ALL")) badgeColor = QColor("#eb0404");
        else if (player.lastAction.startsWith("SB") || player.lastAction.startsWith("BB"))
            badgeColor = QColor("#888888");
        else badgeColor = QColor("#888888");

        QFont badgeFont("Segoe UI", 8, QFont::Bold);
        p.setFont(badgeFont);
        QFontMetrics fm(badgeFont);
        double tw = fm.horizontalAdvance(player.lastAction) + 12;
        double th = 18;
        QRectF badgeRect(pos.x() - tw / 2, pos.y() + avatarR + 32, tw, th);
        QPainterPath badgePath;
        badgePath.addRoundedRect(badgeRect, 4, 4);
        p.fillPath(badgePath, badgeColor.darker(150));
        p.setPen(QPen(badgeColor, 1));
        p.drawPath(badgePath);
        p.setPen(Qt::white);
        p.drawText(badgeRect, Qt::AlignCenter, player.lastAction);
    }

    p.setOpacity(1.0);

    // Draw hole cards
    if (!player.holeCards.isEmpty()) {
        bool showCards = player.isHero || m_phase == 4;
        for (int c = 0; c < player.holeCards.size(); ++c) {
            QPointF cardPos = cardPosition(seatIdx, c);
            if (showCards && !player.folded) {
                drawCard(p, player.holeCards[c], cardPos, 38, 54, true);
            } else if (!player.folded) {
                drawCardBack(p, cardPos, 38, 54);
            }
        }
    }

    p.restore();
}

void PokerRoom::drawEmptySeat(QPainter& p, int seatIdx)
{
    p.save();
    QPointF pos = seatPosition(seatIdx);
    double r = 22;

    // In online mode: show "SIT" text with dashed border (clickable)
    if (m_online && m_mySeat < 0) {
        // Player hasn't chosen a seat yet — show "SIT HERE" buttons
        p.setPen(QPen(QColor("#facc15"), 2, Qt::DashLine));
        p.setBrush(QColor(250, 204, 21, 20));
        p.drawEllipse(pos, r, r);

        p.setFont(QFont("Segoe UI", 11, QFont::Bold));
        p.setPen(QColor("#facc15"));
        p.drawText(QRectF(pos.x() - r, pos.y() - r, r * 2, r * 2), Qt::AlignCenter, "SIT");

        p.setFont(QFont("Segoe UI", 7));
        p.setPen(QColor(250, 204, 21, 150));
        p.drawText(QRectF(pos.x() - 30, pos.y() + r + 2, 60, 14), Qt::AlignCenter, "Click to sit");
    } else if (m_online) {
        // Player is seated; show empty seat placeholder
        p.setPen(QPen(QColor(255, 255, 255, 30), 1.5, Qt::DashLine));
        p.setBrush(QColor(255, 255, 255, 5));
        p.drawEllipse(pos, r, r);

        p.setFont(QFont("Segoe UI", 8));
        p.setPen(QColor(255, 255, 255, 30));
        p.drawText(QRectF(pos.x() - 30, pos.y() + r + 2, 60, 14), Qt::AlignCenter, "Empty");
    } else {
        // Offline mode: standard empty seat
        p.setPen(QPen(QColor(255, 255, 255, 40), 2, Qt::DashLine));
        p.setBrush(QColor(255, 255, 255, 10));
        p.drawEllipse(pos, r, r);

        p.setFont(QFont("Segoe UI", 18));
        p.setPen(QColor(255, 255, 255, 60));
        p.drawText(QRectF(pos.x() - r, pos.y() - r, r * 2, r * 2), Qt::AlignCenter, "+");

        p.setFont(QFont("Segoe UI", 8));
        p.setPen(QColor(255, 255, 255, 30));
        p.drawText(QRectF(pos.x() - 30, pos.y() + r + 2, 60, 14), Qt::AlignCenter, "Empty");
    }

    // Store hit rect for click detection
    m_seatHitRects[seatIdx] = QRectF(pos.x() - r, pos.y() - r, r * 2, r * 2);

    p.restore();
}

// ─── Draw Card ─────────────────────────────────────────────────────────────────

void PokerRoom::drawCard(QPainter& p, const Card& card, QPointF pos, double w, double h, bool faceUp)
{
    if (!faceUp) {
        drawCardBack(p, pos, w, h);
        return;
    }

    p.save();

    QRectF cardRect(pos.x() - w / 2, pos.y() - h / 2, w, h);

    // Try PNG image first
    QString key = cardImageKey(card);
    if (m_cardImages.contains(key)) {
        // Shadow
        QPainterPath shadowPath;
        shadowPath.addRoundedRect(cardRect.adjusted(3, 3, 3, 3), 5, 5);
        p.fillPath(shadowPath, QColor(0, 0, 0, 120));

        // Clip to rounded rect
        QPainterPath clipPath;
        clipPath.addRoundedRect(cardRect, 5, 5);
        p.setClipPath(clipPath);
        p.drawPixmap(cardRect.toRect(), m_cardImages[key]);
        p.setClipping(false);

        // Border
        p.setPen(QPen(QColor(100, 100, 100), 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(cardRect, 5, 5);

        p.restore();
        return;
    }

    // Fallback: QPainter text rendering

    // Strong shadow for depth
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(cardRect.adjusted(3, 3, 3, 3), 5, 5);
    p.fillPath(shadowPath, QColor(0, 0, 0, 120));

    // Card body — bright white with subtle gradient
    QPainterPath cardPath;
    cardPath.addRoundedRect(cardRect, 5, 5);
    QLinearGradient cardGrad(cardRect.topLeft(), cardRect.bottomRight());
    cardGrad.setColorAt(0, QColor("#ffffff"));
    cardGrad.setColorAt(0.5, QColor("#fafafa"));
    cardGrad.setColorAt(1, QColor("#f0f0ee"));
    p.fillPath(cardPath, cardGrad);

    // Strong border
    p.setPen(QPen(QColor(100, 100, 100), 1.2));
    p.drawPath(cardPath);

    // Suit and rank
    static const char* rankChars[] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    static const QString suitChars[] = {
        QString::fromUtf8("\xe2\x99\xa5"),  // Hearts
        QString::fromUtf8("\xe2\x99\xa6"),  // Diamonds
        QString::fromUtf8("\xe2\x99\xa3"),  // Clubs
        QString::fromUtf8("\xe2\x99\xa0")   // Spades
    };

    QColor suitColor = card.isRed() ? QColor("#e60000") : QColor("#000000");
    QString rankStr = QString(rankChars[card.rank]);
    QString suitStr = suitChars[card.suit];

    p.setFont(QFont("Arial Black", 12, QFont::Black));
    p.setPen(suitColor);
    p.drawText(QRectF(cardRect.left() + 3, cardRect.top() + 2, 24, 16),
               Qt::AlignLeft | Qt::AlignTop, rankStr);

    p.setFont(QFont("Segoe UI Symbol", 9, QFont::Bold));
    p.drawText(QRectF(cardRect.left() + 4, cardRect.top() + 16, 18, 14),
               Qt::AlignLeft | Qt::AlignTop, suitStr);

    p.setFont(QFont("Segoe UI Symbol", 22, QFont::Bold));
    p.drawText(cardRect, Qt::AlignCenter, suitStr);

    p.save();
    p.translate(cardRect.right() - 3, cardRect.bottom() - 2);
    p.rotate(180);
    p.setFont(QFont("Arial Black", 12, QFont::Black));
    p.setPen(suitColor);
    p.drawText(QRectF(0, 0, 24, 16), Qt::AlignLeft | Qt::AlignTop, rankStr);
    p.setFont(QFont("Segoe UI Symbol", 9, QFont::Bold));
    p.drawText(QRectF(1, 16, 18, 14), Qt::AlignLeft | Qt::AlignTop, suitStr);
    p.restore();

    p.restore();
}

void PokerRoom::drawCardBack(QPainter& p, QPointF pos, double w, double h)
{
    p.save();

    QRectF cardRect(pos.x() - w / 2, pos.y() - h / 2, w, h);

    // Try PNG card back image first
    if (!m_cardBackImage.isNull()) {
        QPainterPath shadowPath;
        shadowPath.addRoundedRect(cardRect.adjusted(3, 3, 3, 3), 5, 5);
        p.fillPath(shadowPath, QColor(0, 0, 0, 120));

        QPainterPath clipPath;
        clipPath.addRoundedRect(cardRect, 5, 5);
        p.setClipPath(clipPath);
        p.drawPixmap(cardRect.toRect(), m_cardBackImage);
        p.setClipping(false);

        p.setPen(QPen(QColor(220, 220, 220), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(cardRect, 5, 5);

        p.restore();
        return;
    }

    // Fallback: procedural card back

    // Strong shadow
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(cardRect.adjusted(3, 3, 3, 3), 5, 5);
    p.fillPath(shadowPath, QColor(0, 0, 0, 120));

    QPainterPath cardPath;
    cardPath.addRoundedRect(cardRect, 5, 5);
    QLinearGradient backGrad(cardRect.topLeft(), cardRect.bottomRight());
    backGrad.setColorAt(0, QColor("#8b1a1a"));
    backGrad.setColorAt(0.3, QColor("#6b1515"));
    backGrad.setColorAt(0.7, QColor("#4a1010"));
    backGrad.setColorAt(1, QColor("#300a0a"));
    p.fillPath(cardPath, backGrad);

    p.setPen(QPen(QColor(220, 220, 220), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(cardRect, 5, 5);

    p.setPen(QPen(QColor(250, 204, 21, 60), 0.5));
    QRectF inner = cardRect.adjusted(4, 4, -4, -4);
    p.drawRoundedRect(inner, 3, 3);

    p.setPen(QPen(QColor(250, 204, 21, 25), 0.5));
    double cx = inner.center().x(), cy = inner.center().y();
    for (double d = -inner.width(); d < inner.width(); d += 6) {
        p.drawLine(QPointF(cx + d, inner.top()), QPointF(cx + d + inner.height(), inner.bottom()));
    }

    p.setClipPath(cardPath);
    p.setPen(QPen(QColor(100, 130, 200, 25), 0.5));
    for (double x = inner.left(); x < inner.right(); x += 6) {
        p.drawLine(QPointF(x, inner.top()), QPointF(x + inner.height() * 0.3, inner.bottom()));
    }
    for (double x = inner.right(); x > inner.left(); x -= 6) {
        p.drawLine(QPointF(x, inner.top()), QPointF(x - inner.height() * 0.3, inner.bottom()));
    }
    p.setClipping(false);

    QPainterPath emblem;
    double er = qMin(w, h) * 0.15;
    emblem.addEllipse(QPointF(pos.x(), pos.y()), er, er);
    p.fillPath(emblem, QColor(250, 204, 21, 40));
    p.setPen(QPen(QColor(250, 204, 21, 60), 0.5));
    p.drawPath(emblem);

    p.restore();
}

// ─── Community Cards ───────────────────────────────────────────────────────────

void PokerRoom::drawCommunityCards(QPainter& p)
{
    if (m_phase < 1 && m_community.isEmpty()) return;

    double cardW = 48;
    double cardH = 68;
    double gap = 6;
    double totalW = 5 * cardW + 4 * gap;
    double startX = m_tableCenter.x() - totalW / 2 + cardW / 2;
    double y = m_tableCenter.y();

    for (int i = 0; i < 5; ++i) {
        QPointF pos(startX + i * (cardW + gap), y);
        if (i < m_community.size()) {
            drawCard(p, m_community[i], pos, cardW, cardH, true);
        } else if (m_gameActive) {
            // Empty card slot
            p.save();
            QRectF slotRect(pos.x() - cardW / 2, pos.y() - cardH / 2, cardW, cardH);
            QPainterPath slotPath;
            slotPath.addRoundedRect(slotRect, 3, 3);
            p.fillPath(slotPath, QColor(0, 0, 0, 30));
            p.setPen(QPen(QColor(255, 255, 255, 20), 1, Qt::DashLine));
            p.drawPath(slotPath);
            p.restore();
        }
    }
}

// ─── Pot Display ───────────────────────────────────────────────────────────────

void PokerRoom::drawPot(QPainter& p)
{
    if (m_pot <= 0 && !m_gameActive) return;

    p.save();

    QString potText = QString("POT: %1 EUR").arg(m_pot, 0, 'f', 0);
    QFont potFont("Segoe UI", 13, QFont::Bold);
    p.setFont(potFont);
    QFontMetrics fm(potFont);
    double tw = fm.horizontalAdvance(potText) + 24;
    double th = 30;
    QRectF potRect(m_tableCenter.x() - tw / 2, m_tableCenter.y() - m_tableHeight * 0.25, tw, th);

    QPainterPath potPath;
    potPath.addRoundedRect(potRect, 12, 12);
    p.fillPath(potPath, QColor(0, 0, 0, 160));
    p.setPen(QPen(QColor(250, 204, 21, 80), 1));
    p.drawPath(potPath);

    p.setPen(QColor("#facc15"));
    p.drawText(potRect, Qt::AlignCenter, potText);

    p.restore();
}

// ─── Dealer Button ─────────────────────────────────────────────────────────────

void PokerRoom::drawDealerButton(QPainter& p)
{
    if (!m_gameActive) return;

    int dealerSeat = m_online ? m_dealerSeatOnline : m_dealerSeat;
    if (dealerSeat < 0) return;

    p.save();

    QPointF seat = seatPosition(dealerSeat);
    double dx = (m_tableCenter.x() - seat.x()) * 0.30;
    double dy = (m_tableCenter.y() - seat.y()) * 0.30;
    QPointF btnPos(seat.x() + dx + 20, seat.y() + dy);

    double r = 11;

    // Shadow
    QPainterPath shadow;
    shadow.addEllipse(QPointF(btnPos.x() + 1, btnPos.y() + 1), r, r);
    p.fillPath(shadow, QColor(0, 0, 0, 80));

    // Button
    QPainterPath btnPath;
    btnPath.addEllipse(btnPos, r, r);
    QRadialGradient btnGrad(btnPos, r);
    btnGrad.setColorAt(0, QColor("#ffffff"));
    btnGrad.setColorAt(0.8, QColor("#e8e8e8"));
    btnGrad.setColorAt(1, QColor("#cccccc"));
    p.fillPath(btnPath, btnGrad);
    p.setPen(QPen(QColor("#facc15"), 2));
    p.drawPath(btnPath);

    // "D" text
    p.setFont(QFont("Segoe UI", 10, QFont::Bold));
    p.setPen(QColor("#333333"));
    p.drawText(QRectF(btnPos.x() - r, btnPos.y() - r, r * 2, r * 2),
               Qt::AlignCenter, "D");

    p.restore();
}

// ─── Player Bet Chips ──────────────────────────────────────────────────────────

void PokerRoom::drawPlayerBet(QPainter& p, int seatIdx, double amount)
{
    if (amount <= 0) return;

    p.save();
    QPointF pos = betChipPosition(seatIdx);

    // Chip stack
    int chipCount = qMin(static_cast<int>(amount / 10) + 1, 5);
    for (int i = 0; i < chipCount; ++i) {
        QPointF chipPos(pos.x(), pos.y() - i * 3);
        double cr = 8;

        QPainterPath chipPath;
        chipPath.addEllipse(chipPos, cr, cr);

        QColor chipColor;
        if (amount >= 100) chipColor = QColor("#1a1a1a"); // black
        else if (amount >= 50) chipColor = QColor("#10a010"); // green
        else if (amount >= 25) chipColor = QColor("#cc0000"); // red
        else chipColor = QColor("#3b82f6"); // blue

        p.fillPath(chipPath, chipColor);
        p.setPen(QPen(QColor(255, 255, 255, 120), 1));
        p.drawPath(chipPath);

        // Chip detail lines
        p.setPen(QPen(QColor(255, 255, 255, 50), 0.5));
        QPainterPath innerChip;
        innerChip.addEllipse(chipPos, cr - 2, cr - 2);
        p.drawPath(innerChip);
    }

    // Amount text
    p.setFont(QFont("Segoe UI", 8, QFont::Bold));
    p.setPen(Qt::white);
    QRectF textRect(pos.x() - 25, pos.y() - chipCount * 3 - 14, 50, 12);
    p.drawText(textRect, Qt::AlignCenter, QString::number(amount, 'f', 0));

    p.restore();
}

// ─── Action Panel ──────────────────────────────────────────────────────────────

void PokerRoom::drawActionPanel(QPainter& p)
{
    p.save();

    double panelW = qMin(width() * 0.85, 720.0);
    double panelH = 100;
    double panelX = (width() - panelW) / 2;
    double panelY = height() - panelH - 10;

    // Panel background
    QPainterPath panelPath;
    panelPath.addRoundedRect(QRectF(panelX, panelY, panelW, panelH), 12, 12);
    p.fillPath(panelPath, QColor(0, 0, 0, 190));
    p.setPen(QPen(QColor(250, 204, 21, 60), 1));
    p.drawPath(panelPath);

    // Calculate call amount
    int heroIdx = m_online ? m_mySeat : m_heroSeat;
    if (heroIdx < 0 || heroIdx >= m_players.size()) { p.restore(); return; }

    double callAmount = m_currentBet - m_players[heroIdx].currentBet;
    bool canCheck = (callAmount <= 0);
    double heroStack = m_players[heroIdx].stack;

    // Button dimensions
    double btnW = 85;
    double btnH = 36;
    double btnY = panelY + 12;
    double spacing = 8;
    double startX = panelX + 14;

    // Helper lambda for drawing buttons
    auto drawButton = [&](QRectF& rect, const QString& text, QColor bg, QColor border, bool enabled) {
        QPainterPath btnPath;
        btnPath.addRoundedRect(rect, 6, 6);

        bool hover = rect.contains(m_mousePos);
        QColor fillColor = enabled ? (hover ? bg.lighter(130) : bg) : QColor(60, 60, 60);

        p.fillPath(btnPath, fillColor);
        p.setPen(QPen(enabled ? border : QColor(80, 80, 80), 1));
        p.drawPath(btnPath);

        p.setFont(QFont("Segoe UI", 10, QFont::Bold));
        p.setPen(enabled ? Qt::white : QColor(100, 100, 100));
        p.drawText(rect, Qt::AlignCenter, text);
    };

    // Determine if hero can afford to call
    bool canAffordCall = heroStack >= callAmount;

    if (canCheck) {
        // No bet to match: show CHECK + RAISE (no FOLD needed, but keep for convenience)
        m_foldBtnRect = QRectF(startX, btnY, btnW, btnH);
        drawButton(m_foldBtnRect, "FOLD", QColor(80, 80, 80), QColor(120, 120, 120), true);

        m_checkBtnRect = QRectF(startX + btnW + spacing, btnY, btnW, btnH);
        m_callBtnRect = QRectF(); // disabled
        drawButton(m_checkBtnRect, "CHECK", QColor(16, 140, 4), QColor(16, 235, 4), true);
    } else if (!canAffordCall) {
        // Can't afford call: show FOLD + ALL-IN only
        m_foldBtnRect = QRectF(startX, btnY, btnW, btnH);
        drawButton(m_foldBtnRect, "FOLD", QColor(80, 80, 80), QColor(120, 120, 120), true);

        m_callBtnRect = QRectF(); // disabled
        m_checkBtnRect = QRectF(); // disabled
        // ALL IN button will be drawn below -- but also put a visible "ALL IN" in the call spot
        QRectF allInCallRect(startX + btnW + spacing, btnY, btnW + 10, btnH);
        QString aiText = QString("ALL IN %1").arg(heroStack, 0, 'f', 0);
        drawButton(allInCallRect, aiText, QColor(200, 20, 20), QColor(235, 4, 4), true);
        // Clicking this area triggers all-in (handled via allInBtnRect below)
    } else {
        // Bet exists and can afford: show FOLD + CALL + RAISE
        m_foldBtnRect = QRectF(startX, btnY, btnW, btnH);
        drawButton(m_foldBtnRect, "FOLD", QColor(80, 80, 80), QColor(120, 120, 120), true);

        m_callBtnRect = QRectF(startX + btnW + spacing, btnY, btnW + 10, btnH);
        m_checkBtnRect = QRectF(); // disabled
        QString callText = QString("CALL %1").arg(qMin(callAmount, heroStack), 0, 'f', 0);
        drawButton(m_callBtnRect, callText, QColor(40, 90, 200), QColor(59, 130, 246), true);
    }

    // RAISE section
    double raiseX = startX + (btnW + spacing) * 2 + 10;

    // Raise presets row
    double presetW = 52;
    double presetH = 20;
    double presetY = panelY + 8;
    double presetSpacing = 4;

    // Preset buttons
    auto drawPreset = [&](QRectF& rect, const QString& text) {
        QPainterPath pp;
        pp.addRoundedRect(rect, 3, 3);
        bool hover = rect.contains(m_mousePos);
        p.fillPath(pp, hover ? QColor(250, 204, 21, 50) : QColor(250, 204, 21, 20));
        p.setPen(QPen(QColor(250, 204, 21, 80), 0.5));
        p.drawPath(pp);
        p.setFont(QFont("Segoe UI", 7, QFont::Bold));
        p.setPen(QColor("#facc15"));
        p.drawText(rect, Qt::AlignCenter, text);
    };

    m_halfPotRect = QRectF(raiseX, presetY, presetW, presetH);
    drawPreset(m_halfPotRect, "Min");

    m_threeFourPotRect = QRectF(raiseX + presetW + presetSpacing, presetY, presetW, presetH);
    drawPreset(m_threeFourPotRect, "1/2 Pot");

    m_potBtnRect = QRectF(raiseX + (presetW + presetSpacing) * 2, presetY, presetW, presetH);
    drawPreset(m_potBtnRect, "3/4 Pot");

    m_twoPotRect = QRectF(raiseX + (presetW + presetSpacing) * 3, presetY, presetW, presetH);
    drawPreset(m_twoPotRect, "Pot");

    // Slider
    double sliderW = (presetW + presetSpacing) * 4 - presetSpacing;
    double sliderY = presetY + presetH + 6;
    m_sliderRect = QRectF(raiseX, sliderY, sliderW, 12);

    // Slider track
    QPainterPath trackPath;
    trackPath.addRoundedRect(m_sliderRect, 4, 4);
    p.fillPath(trackPath, QColor(50, 50, 50));

    // Slider fill
    double minRaiseIncr = qMax(m_bigBlind, m_lastRaiseAmount);
    double minRaise = m_currentBet + minRaiseIncr;
    if (minRaise < m_bigBlind * 2) minRaise = m_bigBlind * 2;
    double maxRaise = heroStack + m_players[heroIdx].currentBet;
    if (maxRaise <= minRaise) maxRaise = minRaise + 1;
    m_sliderValue = qBound(0.0, (m_raiseAmount - minRaise) / (maxRaise - minRaise), 1.0);

    double fillW = m_sliderRect.width() * m_sliderValue;
    QPainterPath fillPath;
    fillPath.addRoundedRect(QRectF(m_sliderRect.x(), m_sliderRect.y(), fillW, m_sliderRect.height()), 4, 4);
    p.fillPath(fillPath, QColor("#facc15"));

    // Slider thumb
    double thumbX = m_sliderRect.x() + fillW;
    QPainterPath thumb;
    thumb.addEllipse(QPointF(thumbX, m_sliderRect.center().y()), 7, 7);
    p.fillPath(thumb, QColor("#facc15"));
    p.setPen(QPen(QColor("#b8860b"), 1));
    p.drawPath(thumb);

    // Raise amount text
    p.setFont(QFont("Segoe UI", 8));
    p.setPen(QColor("#facc15"));
    p.drawText(QRectF(raiseX, sliderY + 14, sliderW, 14), Qt::AlignCenter,
               QString("Raise to: %1").arg(m_raiseAmount, 0, 'f', 0));

    // RAISE button
    m_raiseBtnRect = QRectF(raiseX + sliderW + spacing, btnY, btnW, btnH);
    QString raiseText = QString("RAISE\n%1").arg(m_raiseAmount, 0, 'f', 0);
    {
        QPainterPath rp;
        rp.addRoundedRect(m_raiseBtnRect, 6, 6);
        bool hover = m_raiseBtnRect.contains(m_mousePos);
        QLinearGradient rg(m_raiseBtnRect.topLeft(), m_raiseBtnRect.bottomLeft());
        rg.setColorAt(0, hover ? QColor("#fbbf24") : QColor("#facc15"));
        rg.setColorAt(1, hover ? QColor("#f5a623") : QColor("#e6a610"));
        p.fillPath(rp, rg);
        p.setPen(QPen(QColor("#b8860b"), 1));
        p.drawPath(rp);
        p.setFont(QFont("Segoe UI", 9, QFont::Bold));
        p.setPen(QColor("#0f1419"));
        p.drawText(m_raiseBtnRect, Qt::AlignCenter, raiseText);
    }

    // ALL IN button
    m_allInBtnRect = QRectF(raiseX + sliderW + spacing + btnW + spacing, btnY, btnW, btnH);
    {
        QPainterPath ap;
        ap.addRoundedRect(m_allInBtnRect, 6, 6);
        bool hover = m_allInBtnRect.contains(m_mousePos);
        p.fillPath(ap, hover ? QColor("#ff2020") : QColor("#eb0404"));
        p.setPen(QPen(QColor("#cc0000"), 1));
        p.drawPath(ap);
        p.setFont(QFont("Segoe UI", 10, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(m_allInBtnRect, Qt::AlignCenter, "ALL IN");
    }

    p.restore();
}

// ─── Hand Strength Display ─────────────────────────────────────────────────────

void PokerRoom::drawHandStrength(QPainter& p)
{
    if (m_phase < 0) return;
    int heroIdx = m_online ? m_mySeat : m_heroSeat;
    if (heroIdx < 0 || heroIdx >= m_players.size()) return;
    auto& hero = m_players[heroIdx];
    if (hero.holeCards.isEmpty() || hero.folded) return;

    p.save();

    QString desc = describeHeroHand();
    if (desc.isEmpty()) { p.restore(); return; }

    QString text = QString("Your Hand: %1").arg(desc);
    QFont font("Segoe UI", 11, QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);
    double tw = fm.horizontalAdvance(text) + 20;
    double th = 26;
    QRectF rect(12, height() - th - 12, tw, th);

    if (m_showActions) rect.moveTop(height() - 110 - th - 12);

    QPainterPath path;
    path.addRoundedRect(rect, 8, 8);
    p.fillPath(path, QColor(0, 0, 0, 150));
    p.setPen(QPen(QColor(16, 235, 4, 80), 1));
    p.drawPath(path);
    p.setPen(QColor("#10eb04"));
    p.drawText(rect, Qt::AlignCenter, text);

    p.restore();
}

// ─── Mouse Events ──────────────────────────────────────────────────────────────

void PokerRoom::mousePressEvent(QMouseEvent* event)
{
    QPoint pos = event->pos();

    // MULTIPLAYER SCREEN handling
    if (m_mpState == MP_MENU) {
        if (!m_hostBtnRect.isNull() && m_hostBtnRect.contains(pos)) {
            mpHostGame();
            return;
        }
        if (!m_joinBtnRect.isNull() && m_joinBtnRect.contains(pos)) {
            m_mpState = MP_JOINING;
            // Position and show the input field
            m_joinCodeInput->move(
                static_cast<int>(width() / 2.0 - 130),
                static_cast<int>(height() / 2.0 - 10));
            m_joinCodeInput->show();
            m_joinCodeInput->setFocus();
            update();
            return;
        }
        if (!m_mpBackBtnRect.isNull() && m_mpBackBtnRect.contains(pos)) {
            // Back to offline
            m_mpState = MP_NONE;
            m_joinCodeInput->hide();
            setOfflineMode();
            return;
        }
        return;
    }

    if (m_mpState == MP_JOINING) {
        if (!m_joinConnectBtnRect.isNull() && m_joinConnectBtnRect.contains(pos)) {
            QString code = m_joinCodeInput->text().trimmed();
            if (!code.isEmpty()) {
                m_joinCodeInput->hide();
                mpJoinGame(code);
            }
            return;
        }
        if (!m_mpBackBtnRect.isNull() && m_mpBackBtnRect.contains(pos)) {
            m_mpState = MP_MENU;
            m_joinCodeInput->hide();
            update();
            return;
        }
        return;
    }

    if (m_mpState == MP_HOSTING) {
        // In hosting state, waiting for connection; back button
        if (!m_mpBackBtnRect.isNull() && m_mpBackBtnRect.contains(pos)) {
            m_mpState = MP_MENU;
            if (m_client) {
                m_client->disconnect();
                QObject::disconnect(m_client, nullptr, this, nullptr);
                m_client = nullptr;
            }
            m_online = false;
            update();
            return;
        }
        // Fall through to online seat selection if already connected
    }

    // BUY-IN screen handling
    if (m_needBuyIn && !m_online) {
        // Slider drag
        if (m_buyInSliderRect.adjusted(-5, -15, 5, 15).contains(pos)) {
            double pct = (pos.x() - m_buyInSliderRect.left()) / m_buyInSliderRect.width();
            pct = qBound(0.0, pct, 1.0);
            m_buyInAmount = m_minBuyIn + pct * (m_maxBuyIn - m_minBuyIn);
            m_buyInAmount = qRound(m_buyInAmount / 50) * 50; // Round to 50
            if (m_buyInAmount < m_minBuyIn) m_buyInAmount = m_minBuyIn;
            update();
            return;
        }

        // Preset buttons
        double cardW = 350, cardH = 280;
        double cardLeft = width()/2.0 - cardW/2;
        double cardTop = height()/2.0 - cardH/2;
        double btnY = cardTop + 180;
        double btnW = 60, btnH = 28, btnGap = 8;
        double startX = cardLeft + (cardW - 4*btnW - 3*btnGap) / 2;
        QVector<double> presets = {100, 500, 1000, 5000};
        for (int i = 0; i < 4; i++) {
            QRectF btn(startX + i*(btnW+btnGap), btnY, btnW, btnH);
            if (btn.contains(pos)) {
                m_buyInAmount = presets[i];
                update();
                return;
            }
        }

        // PLAY button
        if (m_buyInPlayBtnRect.contains(pos)) {
            // Set hero stack to buy-in amount
            m_players[m_heroSeat].stack = m_buyInAmount;
            // Deduct from engine balance if available
            if (m_engine) {
                double bal = m_engine->getPlayer().balance("EUR");
                if (m_buyInAmount > bal) m_buyInAmount = bal;
                m_players[m_heroSeat].stack = m_buyInAmount;
            }
            m_needBuyIn = false;
            startGame();
            return;
        }
        return;
    }

    // In online mode: handle seat selection clicks
    if (m_online && m_mySeat < 0) {
        for (int i = 0; i < 9; ++i) {
            if (!m_players[i].seated && i < m_seatHitRects.size()) {
                if (m_seatHitRects[i].contains(pos)) {
                    // Send choose_seat to server
                    if (m_client) {
                        m_pendingSeatRequest = i;
                        double buyIn = m_pendingBuyIn > 0 ? m_pendingBuyIn : 0;
                        m_client->chooseSeat(i, buyIn);
                        m_waitingText = QString("Requesting seat %1...").arg(i);
                        update();
                    }
                    return;
                }
            }
        }
    }

    // Deal / Rebuy button (OFFLINE ONLY)
    if (!m_online && !m_dealBtnRect.isNull() && m_dealBtnRect.contains(pos)) {
        // If out of chips, handle rebuy
        if (m_heroOutOfChips) {
            m_players[m_heroSeat].stack = 1000;
            m_heroOutOfChips = false;
            m_resultText.clear();
            update();
            return;
        }
        startGame();
        return;
    }

    if (!m_heroTurn || !m_showActions) return;

    // Action buttons
    if (m_foldBtnRect.contains(pos)) {
        heroFold();
        return;
    }
    if (!m_checkBtnRect.isNull() && m_checkBtnRect.contains(pos)) {
        heroCheck();
        return;
    }
    if (!m_callBtnRect.isNull() && m_callBtnRect.contains(pos)) {
        heroCall();
        return;
    }
    if (m_raiseBtnRect.contains(pos)) {
        heroRaise(m_raiseAmount);
        return;
    }
    if (m_allInBtnRect.contains(pos)) {
        heroAllIn();
        return;
    }

    // Raise presets
    int heroIdx = m_online ? m_mySeat : m_heroSeat;
    if (heroIdx < 0 || heroIdx >= m_players.size()) return;
    double heroStack = m_players[heroIdx].stack + m_players[heroIdx].currentBet;
    double minRaiseIncr = qMax(m_bigBlind, m_lastRaiseAmount);
    double minRaise = m_currentBet + minRaiseIncr;
    if (minRaise < m_bigBlind * 2) minRaise = m_bigBlind * 2;

    if (m_halfPotRect.contains(pos)) {
        // Min Raise preset
        m_raiseAmount = qBound(minRaise, minRaise, heroStack);
        update();
        return;
    }
    if (m_threeFourPotRect.contains(pos)) {
        // 1/2 Pot preset
        m_raiseAmount = qBound(minRaise, m_pot * 0.5 + m_currentBet, heroStack);
        update();
        return;
    }
    if (m_potBtnRect.contains(pos)) {
        // 3/4 Pot preset
        m_raiseAmount = qBound(minRaise, m_pot * 0.75 + m_currentBet, heroStack);
        update();
        return;
    }
    if (m_twoPotRect.contains(pos)) {
        // Pot preset
        m_raiseAmount = qBound(minRaise, m_pot + m_currentBet, heroStack);
        update();
        return;
    }

    // Slider drag start
    if (m_sliderRect.adjusted(-8, -8, 8, 8).contains(pos)) {
        m_draggingSlider = true;
        double fraction = qBound(0.0, (pos.x() - m_sliderRect.x()) / m_sliderRect.width(), 1.0);
        double maxRaise = heroStack;
        m_raiseAmount = minRaise + fraction * (maxRaise - minRaise);
        m_raiseAmount = qBound(minRaise, m_raiseAmount, maxRaise);
        // Round to nearest big blind
        m_raiseAmount = qRound(m_raiseAmount / m_bigBlind) * m_bigBlind;
        m_raiseAmount = qBound(minRaise, m_raiseAmount, maxRaise);
        update();
    }
}

void PokerRoom::mouseMoveEvent(QMouseEvent* event)
{
    m_mousePos = event->pos();

    if (m_draggingSlider && (event->buttons() & Qt::LeftButton)) {
        int heroIdx = m_online ? m_mySeat : m_heroSeat;
        if (heroIdx < 0 || heroIdx >= m_players.size()) return;
        double heroStack = m_players[heroIdx].stack + m_players[heroIdx].currentBet;
        double minRaiseIncrDrag = qMax(m_bigBlind, m_lastRaiseAmount);
        double minRaise = m_currentBet + minRaiseIncrDrag;
        if (minRaise < m_bigBlind * 2) minRaise = m_bigBlind * 2;
        double maxRaise = heroStack;

        double fraction = qBound(0.0, (m_mousePos.x() - m_sliderRect.x()) / m_sliderRect.width(), 1.0);
        m_raiseAmount = minRaise + fraction * (maxRaise - minRaise);
        m_raiseAmount = qRound(m_raiseAmount / m_bigBlind) * m_bigBlind;
        m_raiseAmount = qBound(minRaise, m_raiseAmount, maxRaise);
    } else {
        m_draggingSlider = false;
    }

    update();
}

void PokerRoom::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

// ─── Online Multiplayer Mode ──────────────────────────────────────────────────

void PokerRoom::setOnlineMode(PokerClient* client)
{
    if (!client) return;

    // Disconnect previous client if any
    if (m_client) {
        QObject::disconnect(m_client, nullptr, this, nullptr);
    }

    m_client = client;
    m_online = true;

    // Reset table to empty state — ALL 9 seats empty
    resetGame();
    for (int i = 0; i < 9; ++i) {
        m_players[i].seated = false;
        m_players[i].name.clear();
        m_players[i].emoji = QString::fromUtf8("\xF0\x9F\x98\x8E"); // default emoji
        m_players[i].isAI = false;
        m_players[i].isHero = false;
        m_players[i].stack = 0;
        m_players[i].seatIndex = i;
    }
    m_mySeat = -1;
    m_timerSeat = -1;
    m_timerSecondsLeft = 0;
    m_currentHandNumber = 0;
    m_serverSeedHash.clear();
    m_revealedServerSeed.clear();
    m_handVerified = false;
    m_dealerSeatOnline = -1;
    m_pendingBuyIn = 0;
    m_pendingSeatRequest = -1;
    m_waitingText = "Connecting to server...";

    // Connect PokerClient signals to PokerRoom slots
    connect(m_client, &PokerClient::tableJoined,
            this, &PokerRoom::onTableJoined);
    connect(m_client, &PokerClient::tableStateUpdated,
            this, &PokerRoom::onTableStateReceived);
    connect(m_client, &PokerClient::cardsDealt,
            this, &PokerRoom::onCardsDealt);
    connect(m_client, &PokerClient::yourTurn,
            this, &PokerRoom::onYourTurn);
    connect(m_client, &PokerClient::actionResult,
            this, &PokerRoom::onActionResult);
    connect(m_client, &PokerClient::showdownResult,
            this, &PokerRoom::onShowdownResult);
    connect(m_client, &PokerClient::playerJoinedTable,
            this, &PokerRoom::onPlayerJoinedTable);
    connect(m_client, &PokerClient::playerLeftTable,
            this, &PokerRoom::onPlayerLeftTable);
    connect(m_client, &PokerClient::disconnected,
            this, &PokerRoom::onClientDisconnected);

    // New signal connections
    connect(m_client, &PokerClient::seatChosenResult,
            this, &PokerRoom::onSeatChosen);
    connect(m_client, &PokerClient::seatRejectedResult,
            this, &PokerRoom::onSeatRejected);
    connect(m_client, &PokerClient::handStartReceived,
            this, &PokerRoom::onHandStart);
    connect(m_client, &PokerClient::seedRequested,
            this, &PokerRoom::onSeedRequested);
    connect(m_client, &PokerClient::timerUpdated,
            this, &PokerRoom::onTimerUpdate);
    connect(m_client, &PokerClient::handCompleted,
            this, &PokerRoom::onHandComplete);

    update();
}

void PokerRoom::setOfflineMode()
{
    if (m_client) {
        QObject::disconnect(m_client, nullptr, this, nullptr);
    }
    m_client = nullptr;
    m_online = false;
    m_mySeat = -1;
    m_waitingText.clear();
    m_timerSeat = -1;
    m_timerSecondsLeft = 0;
    m_currentHandNumber = 0;
    m_serverSeedHash.clear();
    m_revealedServerSeed.clear();
    m_handVerified = false;
    m_dealerSeatOnline = -1;

    // Restore default offline player setup
    QStringList names = {"You", "Viktor", "Elena", "Carlos", "Yuki", "Marcel", "Anya", "Diego", "Fatima"};
    QStringList emojis = {
        QString::fromUtf8("\xF0\x9F\x98\x8E"),
        QString::fromUtf8("\xF0\x9F\xA4\xA0"),
        QString::fromUtf8("\xF0\x9F\x91\xA9"),
        QString::fromUtf8("\xF0\x9F\xA7\x94"),
        QString::fromUtf8("\xF0\x9F\x91\xA7"),
        QString::fromUtf8("\xF0\x9F\x91\xB4"),
        QString::fromUtf8("\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\xA6\xB0"),
        QString::fromUtf8("\xF0\x9F\xA7\x91"),
        QString::fromUtf8("\xF0\x9F\x91\xB3")
    };

    for (int i = 0; i < 9; ++i) {
        m_players[i].name = names[i];
        m_players[i].emoji = emojis[i];
        m_players[i].seatIndex = i;
        m_players[i].isHero = (i == 0);
        m_players[i].isAI = (i != 0);
        m_players[i].stack = (i == 0) ? 1000 : (500 + QRandomGenerator::global()->bounded(1500));
        m_players[i].seated = (i != 6 && i != 8);
    }
    m_heroSeat = 0;

    resetGame();
    update();
}

// ─── Online Mode Slots ───────────────────────────────────────────────────────

void PokerRoom::onTableJoined(const QString& tableId, int seat, const QJsonObject& state)
{
    Q_UNUSED(tableId);

    // In the new flow, seat may be -1 (must choose seat)
    if (seat >= 0 && seat < m_players.size()) {
        m_mySeat = seat;
        m_heroSeat = seat;
        m_players[seat].isHero = true;
        m_players[seat].seated = true;
    }

    if (m_mySeat < 0) {
        m_waitingText = "Choose a seat to sit down";
    } else {
        m_waitingText = "Waiting for players...";
    }

    // Apply initial table state if provided
    if (!state.isEmpty()) {
        onTableStateReceived(state);
    }

    update();
}

void PokerRoom::onSeatChosen(int seat, const QString& player)
{
    if (seat < 0 || seat >= m_players.size()) return;

    // Determine if this seat confirmation is for us
    // Match by player_id, or by pending seat request matching the seat number
    bool isMe = false;
    if (m_client) {
        isMe = (player == m_client->playerId());
        // Also match if we have a pending request for this exact seat
        if (!isMe && m_pendingSeatRequest == seat && m_mySeat < 0) {
            isMe = true;
        }
    }

    // If we don't have a seat yet and this confirmation is for us
    if (m_mySeat < 0 && isMe) {
        m_pendingSeatRequest = -1;
        m_mySeat = seat;
        m_heroSeat = seat;
        m_players[seat].isHero = true;
        m_waitingText = "Waiting for players...";
    }

    m_players[seat].seated = true;
    m_players[seat].name = player;
    m_players[seat].isAI = false;

    update();
}

void PokerRoom::onSeatRejected(int seat, const QString& reason)
{
    Q_UNUSED(seat);
    m_waitingText = QString("Seat rejected: %1").arg(reason);
    update();

    // Clear the rejection message after 3 seconds
    QTimer::singleShot(3000, this, [this]() {
        if (m_mySeat < 0) {
            m_waitingText = "Choose a seat to sit down";
        }
        update();
    });
}

void PokerRoom::onHandStart(int handNumber, const QString& serverSeedHash, int dealerSeat)
{
    m_currentHandNumber = handNumber;
    m_serverSeedHash = serverSeedHash;
    m_revealedServerSeed.clear();
    m_handVerified = false;
    m_dealerSeatOnline = dealerSeat;
    m_dealerSeat = dealerSeat;
    m_resultText.clear();
    m_waitingText.clear();

    // Reset per-hand state for all players
    for (auto& p : m_players) {
        p.holeCards.clear();
        p.folded = false;
        p.allIn = false;
        p.hasBet = false;
        p.currentBet = 0;
        p.lastAction.clear();
    }
    m_community.clear();
    m_pot = 0;
    m_currentBet = 0;
    m_phase = 0;  // preflop
    m_gameActive = true;
    m_heroTurn = false;
    m_showActions = false;

    update();
}

void PokerRoom::onSeedRequested()
{
    // Client auto-handles this (generates and sends seed in PokerClient::onTextMessage)
    // Nothing extra needed here
}

void PokerRoom::onTimerUpdate(int seat, int secondsLeft)
{
    m_timerSeat = seat;
    m_timerSecondsLeft = secondsLeft;
    update();
}

void PokerRoom::onHandComplete(int handNumber, const QString& serverSeed, const QJsonObject& metadata)
{
    Q_UNUSED(handNumber);
    m_revealedServerSeed = serverSeed;

    // Verify: SHA256(serverSeed) should equal the hash we received at hand start
    if (!m_serverSeedHash.isEmpty() && !serverSeed.isEmpty()) {
        // Use ProvablyFair::sha256 equivalent (QCryptographicHash)
        QByteArray hash = QCryptographicHash::hash(serverSeed.toUtf8(), QCryptographicHash::Sha256);
        QString computedHash = QString::fromLatin1(hash.toHex());
        m_handVerified = (computedHash == m_serverSeedHash);
    }

    m_timerSeat = -1;
    m_timerSecondsLeft = 0;

    Q_UNUSED(metadata);
    update();
}

void PokerRoom::onTableStateReceived(const QJsonObject& state)
{
    // Parse phase
    QString phaseStr = state["phase"].toString();
    if (!phaseStr.isEmpty()) {
        m_phase = phaseFromString(phaseStr);
    } else if (state.contains("phase") && state["phase"].isDouble()) {
        m_phase = state["phase"].toInt();
    }

    m_pot = state["pot"].toDouble();
    m_currentBet = state["current_bet"].toDouble();

    // Dealer seat
    if (state.contains("dealer_seat")) {
        m_dealerSeatOnline = state["dealer_seat"].toInt(-1);
    }

    // Hand number
    if (state.contains("hand_number") && state["hand_number"].toInt() > 0) {
        m_currentHandNumber = state["hand_number"].toInt();
    }

    // Timer info
    if (state.contains("timer_seat")) {
        m_timerSeat = state["timer_seat"].toInt(-1);
        m_timerSecondsLeft = state["seconds_left"].toInt(0);
    }

    // Community cards
    m_community.clear();
    if (state.contains("community")) {
        QJsonArray commArr = state["community"].toArray();
        for (const auto& c : commArr) {
            m_community.append(cardFromString(c.toString()));
        }
    }

    // Players
    if (state.contains("players")) {
        QJsonArray players = state["players"].toArray();

        // First, mark all seats as not found
        bool seatFound[9] = {false};

        for (const auto& p : players) {
            QJsonObject obj = p.toObject();
            int seat = obj["seat"].toInt();
            if (seat < 0 || seat >= 9) continue;

            bool isSeated = obj["seated"].toBool(false);
            seatFound[seat] = true;

            if (isSeated) {
                m_players[seat].name = obj["name"].toString();
                m_players[seat].stack = obj["stack"].toDouble();
                m_players[seat].folded = obj["folded"].toBool();
                m_players[seat].allIn = obj["all_in"].toBool();
                m_players[seat].currentBet = obj["bet"].toDouble();
                m_players[seat].lastAction = obj["last_action"].toString();
                m_players[seat].seated = true;
                m_players[seat].isHero = (seat == m_mySeat);
                m_players[seat].isAI = false;
                m_players[seat].hasBet = (m_players[seat].currentBet > 0 || !m_players[seat].lastAction.isEmpty());

                // Only show cards if visible
                if (obj.contains("cards") && !obj["cards"].toArray().isEmpty()) {
                    m_players[seat].holeCards.clear();
                    for (const auto& c : obj["cards"].toArray()) {
                        m_players[seat].holeCards.append(cardFromString(c.toString()));
                    }
                }
            } else {
                m_players[seat].seated = false;
                m_players[seat].holeCards.clear();
            }
        }

        // Mark unseated positions as empty
        for (int i = 0; i < 9; ++i) {
            if (!seatFound[i]) {
                m_players[i].seated = false;
                m_players[i].holeCards.clear();
            }
        }
    }

    // Update active player indicator
    if (state.contains("active_seat")) {
        m_activePlayerIdx = state["active_seat"].toInt(-1);
    }

    // Determine if game is active based on phase
    m_gameActive = (m_phase >= 0 && m_phase < 4);

    // Update waiting text
    if (m_phase == -1) {
        int seatedCount = 0;
        for (int i = 0; i < 9; ++i) {
            if (m_players[i].seated) seatedCount++;
        }
        if (m_mySeat < 0) {
            m_waitingText = "Choose a seat to sit down";
        } else if (seatedCount < 2) {
            m_waitingText = "Waiting for players...";
        } else {
            m_waitingText = "Starting soon...";
        }
    } else {
        m_waitingText.clear();
    }

    // Handle result text from server
    if (state.contains("result_text")) {
        m_resultText = state["result_text"].toString();
        bool heroWon = state["hero_won"].toBool(false);
        m_resultColor = heroWon ? QColor("#10eb04") : QColor("#eb0404");
    }

    update();
}

void PokerRoom::onCardsDealt(const QStringList& cards)
{
    // Server sends hero's hole cards
    if (m_mySeat < 0 || m_mySeat >= m_players.size()) return;

    m_players[m_mySeat].holeCards.clear();
    for (const QString& cs : cards) {
        m_players[m_mySeat].holeCards.append(cardFromString(cs));
    }

    // In online mode, other players have face-down cards
    for (int i = 0; i < 9; ++i) {
        if (i == m_mySeat) continue;
        if (m_players[i].seated && !m_players[i].folded && m_players[i].holeCards.isEmpty()) {
            // Add 2 placeholder cards (they will be drawn face-down)
            m_players[i].holeCards.append(Card{Card::Ace, Card::Spades});
            m_players[i].holeCards.append(Card{Card::Ace, Card::Spades});
        }
    }

    m_gameActive = true;
    if (m_phase < 0) m_phase = 0;
    m_waitingText.clear();

    update();
}

void PokerRoom::onYourTurn(double toCall, double minRaise, double pot)
{
    Q_UNUSED(pot);
    m_heroTurn = true;
    m_showActions = true;
    m_currentBet = m_players[m_mySeat].currentBet + toCall;
    m_minRaise = minRaise;
    m_raiseAmount = qMax(m_currentBet + minRaise, m_bigBlind * 2);
    m_activePlayerIdx = m_mySeat;

    update();
}

void PokerRoom::onActionResult(int seat, const QString& action, double amount)
{
    if (seat < 0 || seat >= m_players.size()) return;

    // Map server action string to display text
    QString displayAction = action;
    if (action == "fold" || action == "Fold" || action.contains("Fold")) {
        displayAction = "Fold";
        m_players[seat].folded = true;
    } else if (action == "check" || action == "Check") {
        displayAction = "Check";
    } else if (action == "call" || action.startsWith("Call")) {
        displayAction = QString("Call %1").arg(amount, 0, 'f', 0);
        m_players[seat].stack -= amount;
        m_players[seat].currentBet += amount;
        m_pot += amount;
    } else if (action == "raise" || action.startsWith("Raise")) {
        displayAction = QString("Raise %1").arg(amount, 0, 'f', 0);
        double toAdd = amount - m_players[seat].currentBet;
        m_players[seat].stack -= toAdd;
        m_players[seat].currentBet = amount;
        m_pot += toAdd;
        m_currentBet = amount;
    } else if (action == "allin" || action == "all_in" || action == "ALL IN" || action.contains("ALL IN")) {
        displayAction = "ALL IN";
        m_pot += m_players[seat].stack;
        m_players[seat].currentBet += m_players[seat].stack;
        m_players[seat].stack = 0;
        m_players[seat].allIn = true;
        if (m_players[seat].currentBet > m_currentBet) {
            m_currentBet = m_players[seat].currentBet;
        }
    }

    m_players[seat].lastAction = displayAction;
    m_players[seat].hasBet = true;

    update();
}

void PokerRoom::onShowdownResult(const QJsonObject& result)
{
    m_phase = 4;
    m_heroTurn = false;
    m_showActions = false;
    m_gameActive = false;
    m_timerSeat = -1;
    m_timerSecondsLeft = 0;

    // Reveal all players' cards from the showdown data
    if (result.contains("all_hands")) {
        QJsonArray allHands = result["all_hands"].toArray();
        for (const auto& h : allHands) {
            QJsonObject obj = h.toObject();
            int seat = obj["seat"].toInt();
            if (seat < 0 || seat >= 9) continue;

            if (obj.contains("cards") && !obj["cards"].toArray().isEmpty()) {
                m_players[seat].holeCards.clear();
                for (const auto& c : obj["cards"].toArray()) {
                    m_players[seat].holeCards.append(cardFromString(c.toString()));
                }
            }
        }
    }

    // Community cards update
    if (result.contains("community")) {
        m_community.clear();
        for (const auto& c : result["community"].toArray()) {
            m_community.append(cardFromString(c.toString()));
        }
    }

    // Winner info
    if (result.contains("winners")) {
        QJsonArray winners = result["winners"].toArray();
        QStringList winnerNames;
        double totalWon = 0;
        bool heroWon = false;
        QString handName;

        for (const auto& w : winners) {
            QJsonObject wo = w.toObject();
            winnerNames.append(wo["name"].toString());
            totalWon += wo["pot_won"].toDouble();
            if (wo["seat"].toInt() == m_mySeat) heroWon = true;
            if (wo.contains("hand_name")) handName = wo["hand_name"].toString();
        }

        m_resultText = QString("%1 wins %2 EUR%3")
            .arg(winnerNames.join(" & "))
            .arg(totalWon, 0, 'f', 0)
            .arg(handName.isEmpty() ? "" : QString(" with %1!").arg(handName));
        m_resultColor = heroWon ? QColor("#10eb04") : QColor("#eb0404");
    }

    // Pot update
    if (result.contains("pot")) {
        m_pot = result["pot"].toDouble();
    }

    update();

    // After 5 seconds, clear result and prepare for next hand
    if (m_online) {
        QTimer::singleShot(5000, this, [this]() {
            m_resultText.clear();
            m_phase = -1;
            m_community.clear();
            for (auto& p : m_players) {
                p.holeCards.clear();
                p.folded = false;
                p.allIn = false;
                p.currentBet = 0;
                p.lastAction.clear();
            }
            m_waitingText = "Dealing next hand...";
            m_handVerified = false;
            m_revealedServerSeed.clear();
            m_serverSeedHash.clear();
            update();
        });
    }
}

void PokerRoom::onPlayerJoinedTable(int seat, const QString& name, double stack)
{
    if (seat < 0 || seat >= m_players.size()) return;

    m_players[seat].name = name;
    m_players[seat].stack = stack;
    m_players[seat].seated = true;
    m_players[seat].isAI = false;
    m_players[seat].isHero = (seat == m_mySeat);
    m_players[seat].folded = false;
    m_players[seat].allIn = false;
    m_players[seat].holeCards.clear();
    m_players[seat].lastAction.clear();
    m_players[seat].currentBet = 0;

    // Assign an emoji based on seat index
    QStringList emojis = {
        QString::fromUtf8("\xF0\x9F\x98\x8E"),
        QString::fromUtf8("\xF0\x9F\xA4\xA0"),
        QString::fromUtf8("\xF0\x9F\x91\xA9"),
        QString::fromUtf8("\xF0\x9F\xA7\x94"),
        QString::fromUtf8("\xF0\x9F\x91\xA7"),
        QString::fromUtf8("\xF0\x9F\x91\xB4"),
        QString::fromUtf8("\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\xA6\xB0"),
        QString::fromUtf8("\xF0\x9F\xA7\x91"),
        QString::fromUtf8("\xF0\x9F\x91\xB3")
    };
    m_players[seat].emoji = emojis[seat % emojis.size()];

    // Update waiting text
    int seatedCount = 0;
    for (int i = 0; i < 9; ++i) {
        if (m_players[i].seated) seatedCount++;
    }
    if (m_mySeat < 0) {
        m_waitingText = "Choose a seat to sit down";
    } else if (seatedCount >= 2 && m_phase == -1) {
        m_waitingText = "Starting soon...";
    }

    update();
}

void PokerRoom::onPlayerLeftTable(int seat, const QString& name)
{
    Q_UNUSED(name);
    if (seat < 0 || seat >= m_players.size()) return;

    m_players[seat].seated = false;
    m_players[seat].name.clear();
    m_players[seat].holeCards.clear();
    m_players[seat].lastAction.clear();
    m_players[seat].currentBet = 0;
    m_players[seat].stack = 0;

    update();
}

void PokerRoom::onClientDisconnected()
{
    m_waitingText = "Disconnected from server";
    m_gameActive = false;
    m_heroTurn = false;
    m_showActions = false;
    m_timerSeat = -1;
    m_timerSecondsLeft = 0;
    update();
}

// ─── Online Mode Helpers ─────────────────────────────────────────────────────

Card PokerRoom::cardFromString(const QString& s)
{
    if (s.length() < 2) return Card{Card::Ace, Card::Spades};

    QChar rankCh = s[0].toUpper();
    QChar suitCh = s[s.length() - 1].toLower();

    Card::Rank rank = Card::Ace;
    if (rankCh == 'A') rank = Card::Ace;
    else if (rankCh == '2') rank = Card::Two;
    else if (rankCh == '3') rank = Card::Three;
    else if (rankCh == '4') rank = Card::Four;
    else if (rankCh == '5') rank = Card::Five;
    else if (rankCh == '6') rank = Card::Six;
    else if (rankCh == '7') rank = Card::Seven;
    else if (rankCh == '8') rank = Card::Eight;
    else if (rankCh == '9') rank = Card::Nine;
    else if (rankCh == 'T' || rankCh == '1') rank = Card::Ten;
    else if (rankCh == 'J') rank = Card::Jack;
    else if (rankCh == 'Q') rank = Card::Queen;
    else if (rankCh == 'K') rank = Card::King;

    Card::Suit suit = Card::Spades;
    if (suitCh == 'h') suit = Card::Hearts;
    else if (suitCh == 'd') suit = Card::Diamonds;
    else if (suitCh == 'c') suit = Card::Clubs;
    else if (suitCh == 's') suit = Card::Spades;

    return Card{rank, suit};
}

int PokerRoom::phaseFromString(const QString& s) const
{
    QString lower = s.toLower();
    if (lower == "waiting" || lower == "idle") return -1;
    if (lower == "preflop") return 0;
    if (lower == "flop") return 1;
    if (lower == "turn") return 2;
    if (lower == "river") return 3;
    if (lower == "showdown") return 4;
    return -1;
}

QString PokerRoom::phaseFromInt(int phase)
{
    switch (phase) {
        case -1: return "waiting";
        case 0: return "preflop";
        case 1: return "flop";
        case 2: return "turn";
        case 3: return "river";
        case 4: return "showdown";
        default: return "unknown";
    }
}

void PokerRoom::drawWaitingOverlay(QPainter& p)
{
    if (m_waitingText.isEmpty()) return;

    double w = width();
    double h = height();
    double availH = h - (m_showActions ? 110 : 0);

    // Semi-transparent text in center of table
    p.save();
    QRectF textRect(w * 0.2, availH * 0.40, w * 0.6, 50);

    // Background pill
    QPainterPath pillPath;
    pillPath.addRoundedRect(textRect, 16, 16);
    p.fillPath(pillPath, QColor(0, 0, 0, 140));
    p.setPen(QPen(QColor("#facc15"), 1.5));
    p.drawPath(pillPath);

    // Text
    p.setFont(QFont("Segoe UI", 16, QFont::Bold));
    p.setPen(QColor("#facc15"));
    p.drawText(textRect, Qt::AlignCenter, m_waitingText);

    // Online indicator dot
    QPointF dotPos(textRect.left() + 20, textRect.center().y());
    p.setPen(Qt::NoPen);
    bool connected = m_client && m_client->isConnected();
    p.setBrush(connected ? QColor("#10eb04") : QColor("#eb0404"));
    p.drawEllipse(dotPos, 5, 5);

    p.restore();
}

// ─── Multiplayer Screen ──────────────────────────────────────────────────────

void PokerRoom::setMultiplayerMode()
{
    // Reset to offline first
    if (m_online) setOfflineMode();
    m_mpState = MP_MENU;
    m_gameCode.clear();
    if (m_joinCodeInput) {
        m_joinCodeInput->clear();
        m_joinCodeInput->hide();
    }
    update();
}

void PokerRoom::drawMultiplayerScreen(QPainter& p)
{
    double w = width();
    double h = height();
    double cardW = 420, cardH = 340;
    QRectF card(w / 2 - cardW / 2, h / 2 - cardH / 2, cardW, cardH);

    // Card background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 25, 35, 245));
    p.drawRoundedRect(card, 14, 14);
    p.setPen(QPen(QColor(250, 204, 21, 80), 2));
    p.drawRoundedRect(card, 14, 14);

    if (m_mpState == MP_MENU) {
        // Title
        p.setFont(QFont("Segoe UI", 22, QFont::Bold));
        p.setPen(QColor("#facc15"));
        p.drawText(QRectF(card.left(), card.top() + 20, cardW, 30), Qt::AlignCenter, "MULTIPLAYER POKER");

        p.setFont(QFont("Segoe UI", 11));
        p.setPen(QColor("#888"));
        p.drawText(QRectF(card.left(), card.top() + 55, cardW, 20), Qt::AlignCenter,
                   "Play with friends on your local network");

        // HOST GAME button
        double btnW = 260, btnH = 55;
        double btnX = w / 2 - btnW / 2;
        m_hostBtnRect = QRectF(btnX, card.top() + 100, btnW, btnH);
        {
            QPainterPath bp;
            bp.addRoundedRect(m_hostBtnRect, 10, 10);
            bool hover = m_hostBtnRect.contains(m_mousePos);
            QLinearGradient grad(m_hostBtnRect.topLeft(), m_hostBtnRect.topRight());
            grad.setColorAt(0, hover ? QColor("#fbbf24") : QColor("#facc15"));
            grad.setColorAt(1, hover ? QColor("#f5a623") : QColor("#f59e0b"));
            p.fillPath(bp, grad);
            p.setFont(QFont("Segoe UI", 16, QFont::Bold));
            p.setPen(QColor("#0a0e14"));
            p.drawText(m_hostBtnRect, Qt::AlignCenter, "HOST GAME");
        }

        // JOIN GAME button
        m_joinBtnRect = QRectF(btnX, card.top() + 175, btnW, btnH);
        {
            QPainterPath bp;
            bp.addRoundedRect(m_joinBtnRect, 10, 10);
            bool hover = m_joinBtnRect.contains(m_mousePos);
            p.fillPath(bp, hover ? QColor(40, 100, 220) : QColor(30, 80, 200));
            p.setPen(QPen(QColor(59, 130, 246), 2));
            p.drawPath(bp);
            p.setFont(QFont("Segoe UI", 16, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(m_joinBtnRect, Qt::AlignCenter, "JOIN GAME");
        }

        // Back button
        m_mpBackBtnRect = QRectF(card.left() + 20, card.bottom() - 50, 80, 32);
        {
            QPainterPath bp;
            bp.addRoundedRect(m_mpBackBtnRect, 6, 6);
            bool hover = m_mpBackBtnRect.contains(m_mousePos);
            p.fillPath(bp, hover ? QColor(60, 60, 80) : QColor(40, 45, 60));
            p.setFont(QFont("Segoe UI", 10, QFont::Bold));
            p.setPen(QColor("#888"));
            p.drawText(m_mpBackBtnRect, Qt::AlignCenter, "BACK");
        }

    } else if (m_mpState == MP_HOSTING) {
        // Show game code
        p.setFont(QFont("Segoe UI", 20, QFont::Bold));
        p.setPen(QColor("#facc15"));
        p.drawText(QRectF(card.left(), card.top() + 20, cardW, 30), Qt::AlignCenter, "HOSTING GAME");

        p.setFont(QFont("Segoe UI", 11));
        p.setPen(QColor("#10eb04"));
        p.drawText(QRectF(card.left(), card.top() + 60, cardW, 20), Qt::AlignCenter,
                   "Server running on port 9999");

        // Game code display
        p.setFont(QFont("Consolas", 24, QFont::Bold));
        p.setPen(QColor("#facc15"));
        p.drawText(QRectF(card.left(), card.top() + 100, cardW, 40), Qt::AlignCenter, m_gameCode);

        p.setFont(QFont("Segoe UI", 10));
        p.setPen(QColor("#888"));
        p.drawText(QRectF(card.left(), card.top() + 145, cardW, 20), Qt::AlignCenter,
                   "Share this code with friends to join");

        // Status
        bool connected = m_client && m_client->isConnected();
        p.setFont(QFont("Segoe UI", 12, QFont::Bold));
        p.setPen(connected ? QColor("#10eb04") : QColor("#facc15"));
        QString statusText = connected ? "Connected! Waiting for players..."
                                       : "Connecting to local server...";
        p.drawText(QRectF(card.left(), card.top() + 190, cardW, 25), Qt::AlignCenter, statusText);

        // Seated count
        if (connected) {
            int seated = 0;
            for (int i = 0; i < 9; ++i)
                if (m_players[i].seated) seated++;
            p.setFont(QFont("Segoe UI", 11));
            p.setPen(QColor("#facc15"));
            p.drawText(QRectF(card.left(), card.top() + 220, cardW, 20), Qt::AlignCenter,
                       QString("Players at table: %1/9").arg(seated));
        }

        // Back button
        m_mpBackBtnRect = QRectF(card.left() + 20, card.bottom() - 50, 100, 32);
        {
            QPainterPath bp;
            bp.addRoundedRect(m_mpBackBtnRect, 6, 6);
            bool hover = m_mpBackBtnRect.contains(m_mousePos);
            p.fillPath(bp, hover ? QColor(60, 60, 80) : QColor(40, 45, 60));
            p.setFont(QFont("Segoe UI", 10, QFont::Bold));
            p.setPen(QColor("#888"));
            p.drawText(m_mpBackBtnRect, Qt::AlignCenter, "DISCONNECT");
        }

    } else if (m_mpState == MP_JOINING) {
        // Title
        p.setFont(QFont("Segoe UI", 20, QFont::Bold));
        p.setPen(QColor("#3b82f6"));
        p.drawText(QRectF(card.left(), card.top() + 20, cardW, 30), Qt::AlignCenter, "JOIN GAME");

        p.setFont(QFont("Segoe UI", 11));
        p.setPen(QColor("#888"));
        p.drawText(QRectF(card.left(), card.top() + 55, cardW, 20), Qt::AlignCenter,
                   "Enter game code or IP address");

        // The QLineEdit (m_joinCodeInput) is positioned in mousePressEvent / setMultiplayerMode
        // Position it here too in case of resize
        m_joinCodeInput->move(
            static_cast<int>(w / 2.0 - 130),
            static_cast<int>(card.top() + 100));

        // CONNECT button
        double btnW = 180, btnH = 42;
        m_joinConnectBtnRect = QRectF(w / 2 - btnW / 2, card.top() + 155, btnW, btnH);
        {
            QPainterPath bp;
            bp.addRoundedRect(m_joinConnectBtnRect, 8, 8);
            bool hover = m_joinConnectBtnRect.contains(m_mousePos);
            QLinearGradient grad(m_joinConnectBtnRect.topLeft(), m_joinConnectBtnRect.topRight());
            grad.setColorAt(0, hover ? QColor("#4d9fff") : QColor("#3b82f6"));
            grad.setColorAt(1, hover ? QColor("#2070e8") : QColor("#1d5fcc"));
            p.fillPath(bp, grad);
            p.setFont(QFont("Segoe UI", 14, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(m_joinConnectBtnRect, Qt::AlignCenter, "CONNECT");
        }

        p.setFont(QFont("Segoe UI", 9));
        p.setPen(QColor("#666"));
        p.drawText(QRectF(card.left(), card.top() + 210, cardW, 20), Qt::AlignCenter,
                   "Formats: OMNI-XXXX or 192.168.1.5:9999");

        // Back button
        m_mpBackBtnRect = QRectF(card.left() + 20, card.bottom() - 50, 80, 32);
        {
            QPainterPath bp;
            bp.addRoundedRect(m_mpBackBtnRect, 6, 6);
            bool hover = m_mpBackBtnRect.contains(m_mousePos);
            p.fillPath(bp, hover ? QColor(60, 60, 80) : QColor(40, 45, 60));
            p.setFont(QFont("Segoe UI", 10, QFont::Bold));
            p.setPen(QColor("#888"));
            p.drawText(m_mpBackBtnRect, Qt::AlignCenter, "BACK");
        }
    }
}

void PokerRoom::mpHostGame()
{
    // Generate game code from local IP
    QString localIp = "127.0.0.1";
    for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol &&
            !addr.isLoopback() && addr.toString().startsWith("192.")) {
            localIp = addr.toString();
            break;
        }
    }
    // Also check 10.x and 172.x ranges
    if (localIp == "127.0.0.1") {
        for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
            if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
                localIp = addr.toString();
                break;
            }
        }
    }

    m_gameCode = GameCode::encode(localIp, 9999);
    m_mpState = MP_HOSTING;

    // Ensure server is running
    if (m_pokerServer && !m_pokerServer->isRunning()) {
        m_pokerServer->start();
    }

    // Create a NEW PokerClient for this connection
    auto* client = new PokerClient(this);
    setOnlineMode(client);

    // When connected, auto-login and join table
    connect(client, &PokerClient::connected, this, [this, client]() {
        QString name = "Host";
        if (m_accountSystem && m_accountSystem->isLoggedIn())
            name = m_accountSystem->displayName();
        client->login(name, 10000);
    });

    connect(client, &PokerClient::loginOk, this, [this, client](const QString&, double) {
        client->joinTable("t1", 1000);
        QObject::disconnect(client, &PokerClient::loginOk, this, nullptr);
    });

    client->connectToServer("ws://localhost:9999");
    update();
}

void PokerRoom::mpJoinGame(const QString& codeOrIp)
{
    QString ip;
    quint16 port = 9999;

    if (codeOrIp.contains(":")) {
        // Direct IP:port format
        auto parts = codeOrIp.split(":");
        ip = parts[0].trimmed();
        port = parts[1].trimmed().toUShort();
        if (port == 0) port = 9999;
    } else if (codeOrIp.startsWith("OMNI-", Qt::CaseInsensitive) || GameCode::isValid(codeOrIp)) {
        auto decoded = GameCode::decode(codeOrIp);
        ip = decoded.first;
        port = decoded.second;
    } else if (codeOrIp.startsWith("DIRECT-")) {
        auto parts = codeOrIp.mid(7).split("-");
        if (parts.size() >= 2) {
            ip = parts[0];
            port = parts[1].toUShort();
        }
    } else {
        // Assume it's just an IP
        ip = codeOrIp;
    }

    if (ip.isEmpty()) {
        ip = "localhost";
        port = 9999;
    }

    m_mpState = MP_CONNECTED;

    // Create a NEW PokerClient for this connection
    auto* client = new PokerClient(this);
    setOnlineMode(client);

    connect(client, &PokerClient::connected, this, [this, client]() {
        QString name = "Player";
        if (m_accountSystem && m_accountSystem->isLoggedIn())
            name = m_accountSystem->displayName();
        client->login(name, 10000);
    });

    connect(client, &PokerClient::loginOk, this, [this, client](const QString&, double) {
        client->joinTable("t1", 1000);
        m_mpState = MP_PLAYING;
        QObject::disconnect(client, &PokerClient::loginOk, this, nullptr);
        update();
    });

    QString url = QString("ws://%1:%2").arg(ip).arg(port);
    client->connectToServer(url);
    update();
}

// ─── Card Image Loading ──────────────────────────────────────────────────────

void PokerRoom::loadCardImages()
{
    // Look for assets/cards/ relative to the executable, or relative to the source tree
    QStringList searchPaths;
    QString appDir = QCoreApplication::applicationDirPath();
    searchPaths << appDir + "/assets/cards"
                << appDir + "/../assets/cards"
                << appDir + "/../../assets/cards"
                << appDir + "/../../../assets/cards";

    QString cardsDir;
    for (const QString& path : searchPaths) {
        if (QDir(path).exists()) {
            cardsDir = path;
            break;
        }
    }

    if (cardsDir.isEmpty()) return;

    // Ranks: A, 2, 3, 4, 5, 6, 7, 8, 9, 0(=10), J, Q, K
    // Suits: S, H, C, D
    static const char* rankChars[] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "0", "J", "Q", "K"};
    static const char* suitChars[] = {"H", "D", "C", "S"}; // matches Card::Suit enum order

    for (int r = 1; r <= 13; ++r) {
        for (int s = 0; s < 4; ++s) {
            QString key = QString("%1%2").arg(rankChars[r]).arg(suitChars[s]);
            QString filePath = cardsDir + "/" + key + ".png";
            QPixmap pix(filePath);
            if (!pix.isNull()) {
                m_cardImages[key] = pix;
            }
        }
    }

    // Card back
    QString backPath = cardsDir + "/back.png";
    QPixmap backPix(backPath);
    if (!backPix.isNull()) {
        m_cardBackImage = backPix;
    }
}

QString PokerRoom::cardImageKey(const Card& card) const
{
    // Map Card::Rank to deckofcardsapi rank char
    static const char* rankChars[] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "0", "J", "Q", "K"};
    // Map Card::Suit to deckofcardsapi suit char
    static const char* suitChars[] = {"H", "D", "C", "S"}; // Hearts, Diamonds, Clubs, Spades

    int r = static_cast<int>(card.rank);
    int s = static_cast<int>(card.suit);
    if (r < 1 || r > 13 || s < 0 || s > 3) return QString();

    return QString("%1%2").arg(rankChars[r]).arg(suitChars[s]);
}
