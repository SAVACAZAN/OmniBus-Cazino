#include "PokerTable.h"
#include "engine/ProvablyFair.h"
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <algorithm>

// ─── Constructor ──────────────────────────────────────────────────────────────

PokerTable::PokerTable(const QString& id, const QString& name,
                       double smallBlind, double bigBlind,
                       double minBuyIn, double maxBuyIn,
                       int maxSeats, QObject* parent)
    : QObject(parent)
    , m_id(id), m_name(name)
    , m_smallBlind(smallBlind), m_bigBlind(bigBlind)
    , m_minBuyIn(minBuyIn), m_maxBuyIn(maxBuyIn)
    , m_maxSeats(maxSeats)
{
    m_seats.resize(maxSeats);
    for (int i = 0; i < maxSeats; ++i) {
        m_seats[i].seatIndex = i;
        m_seats[i].seated = false;
    }

    // Main action timer: 30s single-shot, triggers auto-fold
    m_actionTimer = new QTimer(this);
    m_actionTimer->setSingleShot(true);
    m_actionTimer->setInterval(ACTION_TIMEOUT_SECS * 1000);
    connect(m_actionTimer, &QTimer::timeout, this, &PokerTable::autoFoldTimeout);

    // Countdown timer: ticks every 1s to broadcast seconds remaining
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, &PokerTable::onTimerTick);
}

// ─── Player Management ───────────────────────────────────────────────────────

int PokerTable::addPlayer(const QString& playerId, const QString& name, double buyIn)
{
    // Validate buy-in
    if (buyIn < m_minBuyIn || buyIn > m_maxBuyIn)
        return -1;

    // Check if player already at table
    for (const auto& s : m_seats) {
        if (s.seated && s.id == playerId)
            return -1;
    }

    // Find first empty seat
    for (int i = 0; i < m_seats.size(); ++i) {
        if (!m_seats[i].seated) {
            m_seats[i].id = playerId;
            m_seats[i].name = name;
            m_seats[i].stack = buyIn;
            m_seats[i].seated = true;
            m_seats[i].connected = true;
            m_seats[i].folded = false;
            m_seats[i].allIn = false;
            m_seats[i].currentBet = 0;
            m_seats[i].totalBetThisHand = 0;
            m_seats[i].holeCards.clear();
            m_seats[i].lastAction.clear();
            m_seats[i].hasBet = false;

            emit playerJoinedTable(m_id, i, name, buyIn);

            // Auto-start hand if we now have 2+ players and no hand active
            if (!m_handActive && seatedCount() >= 2) {
                QTimer::singleShot(3000, this, &PokerTable::tryStartHand);
            }

            return i;
        }
    }
    return -1; // table full
}

int PokerTable::chooseSeat(const QString& playerId, const QString& name, double buyIn, int requestedSeat)
{
    // Validate buy-in
    if (buyIn < m_minBuyIn || buyIn > m_maxBuyIn) {
        emit seatRejected(m_id, requestedSeat, "Invalid buy-in amount");
        return -1;
    }

    // Check if player already at table
    for (const auto& s : m_seats) {
        if (s.seated && s.id == playerId) {
            emit seatRejected(m_id, requestedSeat, "Already seated at this table");
            return -1;
        }
    }

    // Validate seat index
    if (requestedSeat < 0 || requestedSeat >= m_seats.size()) {
        emit seatRejected(m_id, requestedSeat, "Invalid seat number");
        return -1;
    }

    // Check if seat is taken
    if (m_seats[requestedSeat].seated) {
        emit seatRejected(m_id, requestedSeat, "Seat taken");
        return -1;
    }

    // Seat the player
    m_seats[requestedSeat].id = playerId;
    m_seats[requestedSeat].name = name;
    m_seats[requestedSeat].stack = buyIn;
    m_seats[requestedSeat].seated = true;
    m_seats[requestedSeat].connected = true;
    m_seats[requestedSeat].folded = false;
    m_seats[requestedSeat].allIn = false;
    m_seats[requestedSeat].currentBet = 0;
    m_seats[requestedSeat].totalBetThisHand = 0;
    m_seats[requestedSeat].holeCards.clear();
    m_seats[requestedSeat].lastAction.clear();
    m_seats[requestedSeat].hasBet = false;

    emit seatChosen(m_id, requestedSeat, name);
    emit playerJoinedTable(m_id, requestedSeat, name, buyIn);

    // Auto-start hand if we now have 2+ players and no hand active
    if (!m_handActive && seatedCount() >= 2) {
        QTimer::singleShot(3000, this, &PokerTable::tryStartHand);
    }

    return requestedSeat;
}

void PokerTable::removePlayer(const QString& playerId)
{
    for (int i = 0; i < m_seats.size(); ++i) {
        if (m_seats[i].seated && m_seats[i].id == playerId) {
            QString removedName = m_seats[i].name;

            // If hand is active and it is this player's turn, auto-fold
            if (m_handActive && !m_seats[i].folded && !m_seats[i].allIn) {
                m_seats[i].folded = true;
                m_seats[i].lastAction = "Fold";
                logAction(i, "FOLD (disconnect)", 0);
                if (m_activePlayerSeat == i) {
                    m_actionTimer->stop();
                    m_countdownTimer->stop();
                    m_actionCount++;
                }
            }

            m_seats[i].seated = false;
            m_seats[i].id.clear();
            m_seats[i].name.clear();
            m_seats[i].stack = 0;
            m_seats[i].holeCards.clear();
            m_seats[i].connected = false;

            emit playerLeftTable(m_id, i, removedName);

            // If it was this player's turn, advance
            if (m_handActive && m_activePlayerSeat == i) {
                advanceToNextPlayer();
            }

            // Check if hand should end (only 1 player left)
            if (m_handActive) {
                int active = 0;
                for (const auto& s : m_seats) {
                    if (s.seated && !s.folded) active++;
                }
                if (active <= 1) {
                    awardPot();
                }
            }
            return;
        }
    }
}

int PokerTable::playerCount() const
{
    int count = 0;
    for (const auto& s : m_seats)
        if (s.seated) count++;
    return count;
}

int PokerTable::seatedCount() const
{
    return playerCount();
}

ServerPlayer* PokerTable::findPlayer(const QString& playerId)
{
    for (auto& s : m_seats)
        if (s.seated && s.id == playerId)
            return &s;
    return nullptr;
}

QString PokerTable::activePlayerId() const
{
    if (m_activePlayerSeat >= 0 && m_activePlayerSeat < m_seats.size()
        && m_seats[m_activePlayerSeat].seated)
        return m_seats[m_activePlayerSeat].id;
    return QString();
}

int PokerTable::secondsLeft() const
{
    if (!m_actionDeadline.isValid()) return 0;
    qint64 msLeft = QDateTime::currentDateTime().msecsTo(m_actionDeadline);
    return qMax(0, static_cast<int>((msLeft + 999) / 1000));
}

// ─── Client Seed Collection ──────────────────────────────────────────────────

void PokerTable::submitClientSeed(const QString& playerId, const QString& seed)
{
    if (!m_waitingForSeeds) return;

    m_pendingClientSeeds[playerId] = seed;

    // Check if we have all seeds
    if (m_pendingClientSeeds.size() >= m_expectedSeedCount) {
        m_waitingForSeeds = false;
        computeCombinedSeed();

        // Now actually deal with the deterministic seed
        m_deck.reset();
        m_deck.shuffleSeeded(m_handMeta.combinedSeed.toUtf8());

        // Deal hole cards
        dealPreflop();

        // Emit deal signals for each player
        for (auto& s : m_seats) {
            if (s.seated && !s.holeCards.isEmpty()) {
                emit dealCards(m_id, s.id, s.holeCards);
            }
        }

        // Find UTG
        int sbIdx = nextActivePlayer(m_dealerSeat);
        int bbIdx = nextActivePlayer(sbIdx);
        m_activePlayerSeat = nextActivePlayer(bbIdx);
        m_actionCount = 0;

        emit handStarted(m_id);
        emit stateChanged(m_id);
        notifyTurn();
    }
}

// ─── Provably Fair ───────────────────────────────────────────────────────────

void PokerTable::generateServerSeed()
{
    m_handMeta.serverSeed = ProvablyFair::generateServerSeed();
    m_handMeta.serverSeedHash = ProvablyFair::sha256(m_handMeta.serverSeed);
}

void PokerTable::computeCombinedSeed()
{
    // Combine: serverSeed + ":" + clientSeed1 + ":" + clientSeed2 + ...
    QString combined = m_handMeta.serverSeed;
    m_handMeta.clientSeeds.clear();

    // Sort by player ID for deterministic order
    QStringList playerIds = m_pendingClientSeeds.keys();
    std::sort(playerIds.begin(), playerIds.end());

    for (const QString& pid : playerIds) {
        combined += ":" + m_pendingClientSeeds[pid];
        m_handMeta.clientSeeds.append(m_pendingClientSeeds[pid]);
    }

    m_handMeta.combinedSeed = ProvablyFair::sha256(combined);
}

void PokerTable::logAction(int seat, const QString& action, double amount)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString playerName = (seat >= 0 && seat < m_seats.size()) ? m_seats[seat].name : "Unknown";
    double playerStack = (seat >= 0 && seat < m_seats.size()) ? m_seats[seat].stack : 0;

    QString entry = QString("%1 | Hand #%2 | Seat %3 (%4) | %5")
        .arg(timestamp)
        .arg(m_handMeta.handNumber)
        .arg(seat)
        .arg(playerName)
        .arg(action);

    if (amount > 0) {
        entry += QString(" %1").arg(amount, 0, 'f', 0);
    }
    entry += QString(" | Pot: %1 | Stack: %2")
        .arg(m_pot, 0, 'f', 0)
        .arg(playerStack, 0, 'f', 0);

    m_handMeta.actionLog.append(entry);
}

void PokerTable::saveHandHistory()
{
    // Archive the hand metadata
    if (m_handHistory.size() >= MAX_HAND_HISTORY) {
        m_handHistory.removeFirst();
    }
    m_handHistory.append(m_handMeta);

    // Save to JSON file
    QJsonObject handObj;
    handObj["hand_number"] = m_handMeta.handNumber;
    handObj["server_seed"] = m_handMeta.serverSeed;
    handObj["server_seed_hash"] = m_handMeta.serverSeedHash;
    handObj["combined_seed"] = m_handMeta.combinedSeed;
    handObj["start_time"] = m_handMeta.startTime.toString(Qt::ISODate);
    handObj["end_time"] = m_handMeta.endTime.toString(Qt::ISODate);

    QJsonArray clientSeedsArr;
    for (const auto& cs : m_handMeta.clientSeeds)
        clientSeedsArr.append(cs);
    handObj["client_seeds"] = clientSeedsArr;

    QJsonArray actionsArr;
    for (const auto& a : m_handMeta.actionLog)
        actionsArr.append(a);
    handObj["actions"] = actionsArr;

    handObj["final_state"] = m_handMeta.finalState;
    handObj["result_hash"] = m_handMeta.resultHash;

    // Append to hand_history.json
    QString historyPath = QDir::currentPath() + "/hand_history.json";
    QFile file(historyPath);
    QJsonArray allHands;

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isArray()) allHands = doc.array();
        file.close();
    }

    allHands.append(handObj);

    // Keep only last 1000 hands in file
    while (allHands.size() > 1000) {
        allHands.removeFirst();
    }

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(allHands).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void PokerTable::finalizeHandMetadata(const QJsonObject& showdownResult)
{
    m_handMeta.endTime = QDateTime::currentDateTime();
    m_handMeta.finalState = showdownResult;

    // Compute result hash: SHA256 of everything
    QString hashInput = QString::number(m_handMeta.handNumber) + "|"
        + m_handMeta.serverSeed + "|"
        + m_handMeta.combinedSeed + "|"
        + m_handMeta.startTime.toString(Qt::ISODate) + "|"
        + m_handMeta.endTime.toString(Qt::ISODate) + "|"
        + QString::fromUtf8(QJsonDocument(showdownResult).toJson(QJsonDocument::Compact));

    m_handMeta.resultHash = ProvablyFair::sha256(hashInput);
}

QJsonObject PokerTable::getHandHistory(int handNumber) const
{
    for (const auto& hm : m_handHistory) {
        if (hm.handNumber == handNumber) {
            QJsonObject obj;
            obj["hand_number"] = hm.handNumber;
            obj["server_seed"] = hm.serverSeed;
            obj["server_seed_hash"] = hm.serverSeedHash;
            obj["combined_seed"] = hm.combinedSeed;
            obj["start_time"] = hm.startTime.toString(Qt::ISODate);
            obj["end_time"] = hm.endTime.toString(Qt::ISODate);

            QJsonArray clientSeedsArr;
            for (const auto& cs : hm.clientSeeds) clientSeedsArr.append(cs);
            obj["client_seeds"] = clientSeedsArr;

            QJsonArray actionsArr;
            for (const auto& a : hm.actionLog) actionsArr.append(a);
            obj["actions"] = actionsArr;

            obj["final_state"] = hm.finalState;
            obj["result_hash"] = hm.resultHash;
            return obj;
        }
    }
    return QJsonObject(); // not found
}

// ─── Game Flow ────────────────────────────────────────────────────────────────

void PokerTable::tryStartHand()
{
    if (m_handActive) return;

    // Count players with chips
    int ready = 0;
    for (int i = 0; i < m_seats.size(); ++i) {
        if (m_seats[i].seated && m_seats[i].stack > 0) {
            ready++;
        }
    }

    if (ready < 2) {
        // Not enough players — try again in 3 seconds
        QTimer::singleShot(3000, this, &PokerTable::tryStartHand);
        return;
    }

    resetHand();
    m_handActive = true;
    m_handNumber++;
    m_phase = 0;

    // Setup provably fair metadata
    m_handMeta = HandMetadata();
    m_handMeta.handNumber = m_handNumber;
    m_handMeta.startTime = QDateTime::currentDateTime();
    generateServerSeed();

    // Advance dealer
    if (m_dealerSeat < 0) {
        for (int i = 0; i < m_seats.size(); ++i) {
            if (m_seats[i].seated && m_seats[i].stack > 0) {
                m_dealerSeat = i;
                break;
            }
        }
    } else {
        m_dealerSeat = nextActivePlayer(m_dealerSeat);
    }

    // Post blinds
    int sbIdx = nextActivePlayer(m_dealerSeat);
    int bbIdx = nextActivePlayer(sbIdx);

    double sbAmount = qMin(m_smallBlind, m_seats[sbIdx].stack);
    m_seats[sbIdx].currentBet = sbAmount;
    m_seats[sbIdx].stack -= sbAmount;
    m_seats[sbIdx].totalBetThisHand += sbAmount;
    m_seats[sbIdx].hasBet = true;
    m_seats[sbIdx].lastAction = QString("SB %1").arg(sbAmount, 0, 'f', 0);
    if (m_seats[sbIdx].stack <= 0) m_seats[sbIdx].allIn = true;

    double bbAmount = qMin(m_bigBlind, m_seats[bbIdx].stack);
    m_seats[bbIdx].currentBet = bbAmount;
    m_seats[bbIdx].stack -= bbAmount;
    m_seats[bbIdx].totalBetThisHand += bbAmount;
    m_seats[bbIdx].hasBet = true;
    m_seats[bbIdx].lastAction = QString("BB %1").arg(bbAmount, 0, 'f', 0);
    if (m_seats[bbIdx].stack <= 0) m_seats[bbIdx].allIn = true;

    m_pot = sbAmount + bbAmount;
    m_currentBet = m_bigBlind;
    m_lastRaiseSize = m_bigBlind;

    logAction(sbIdx, "SB", sbAmount);
    logAction(bbIdx, "BB", bbAmount);

    // Broadcast hand_start with server seed hash (commitment)
    emit handStartBroadcast(m_id, m_handNumber, m_handMeta.serverSeedHash, m_dealerSeat);

    // Request client seeds from all seated players
    m_pendingClientSeeds.clear();
    m_expectedSeedCount = 0;
    for (const auto& s : m_seats) {
        if (s.seated && s.stack >= 0) m_expectedSeedCount++;
    }
    m_waitingForSeeds = true;
    emit seedRequest(m_id);

    // Start a timeout for seed collection (5 seconds max wait)
    // If not all seeds arrive, proceed with what we have
    QTimer::singleShot(5000, this, [this]() {
        if (m_waitingForSeeds && m_handActive) {
            m_waitingForSeeds = false;
            // Fill missing seeds with empty strings
            for (const auto& s : m_seats) {
                if (s.seated && !m_pendingClientSeeds.contains(s.id)) {
                    m_pendingClientSeeds[s.id] = "";
                }
            }
            computeCombinedSeed();
            m_deck.reset();
            m_deck.shuffleSeeded(m_handMeta.combinedSeed.toUtf8());

            dealPreflop();

            for (auto& s : m_seats) {
                if (s.seated && !s.holeCards.isEmpty()) {
                    emit dealCards(m_id, s.id, s.holeCards);
                }
            }

            int sbIdx2 = nextActivePlayer(m_dealerSeat);
            int bbIdx2 = nextActivePlayer(sbIdx2);
            m_activePlayerSeat = nextActivePlayer(bbIdx2);
            m_actionCount = 0;

            emit handStarted(m_id);
            emit stateChanged(m_id);
            notifyTurn();
        }
    });
}

void PokerTable::handleAction(const QString& playerId, const QString& action, double amount)
{
    if (!m_handActive) return;
    if (m_activePlayerSeat < 0 || m_activePlayerSeat >= m_seats.size()) return;

    auto& player = m_seats[m_activePlayerSeat];
    if (player.id != playerId) return; // not this player's turn
    if (player.folded || player.allIn) return;

    m_actionTimer->stop();
    m_countdownTimer->stop();

    double toCall = m_currentBet - player.currentBet;

    if (action == "fold") {
        player.folded = true;
        player.lastAction = "Fold";
        logAction(m_activePlayerSeat, "FOLD", 0);
    }
    else if (action == "check") {
        if (toCall > 0) return; // can't check when there's a bet
        player.lastAction = "Check";
        player.hasBet = true;
        logAction(m_activePlayerSeat, "CHECK", 0);
    }
    else if (action == "call") {
        if (toCall <= 0) {
            // Treat as check
            player.lastAction = "Check";
            player.hasBet = true;
            logAction(m_activePlayerSeat, "CHECK", 0);
        } else {
            double callAmt = qMin(toCall, player.stack);
            player.stack -= callAmt;
            player.currentBet += callAmt;
            player.totalBetThisHand += callAmt;
            m_pot += callAmt;
            if (player.stack <= 0) {
                player.allIn = true;
                player.lastAction = "ALL IN";
                logAction(m_activePlayerSeat, "ALL IN (call)", callAmt);
            } else {
                player.lastAction = QString("Call %1").arg(callAmt, 0, 'f', 0);
                logAction(m_activePlayerSeat, "CALL", callAmt);
            }
            player.hasBet = true;
        }
    }
    else if (action == "raise") {
        // Min raise = current bet + max(big blind, last raise size)
        double minRaiseTotal = m_currentBet + qMax(m_bigBlind, m_lastRaiseSize);
        double raiseTotal = amount; // total bet amount

        // Allow smaller raise only if it's an all-in
        if (raiseTotal < minRaiseTotal && raiseTotal < player.stack + player.currentBet) {
            return; // invalid raise
        }

        double toAdd = raiseTotal - player.currentBet;
        toAdd = qMin(toAdd, player.stack);

        double actualRaiseSize = (player.currentBet + toAdd) - m_currentBet;

        player.stack -= toAdd;
        player.currentBet += toAdd;
        player.totalBetThisHand += toAdd;
        m_pot += toAdd;

        // Update last raise size (for min raise tracking)
        if (actualRaiseSize > 0) {
            m_lastRaiseSize = actualRaiseSize;
        }

        m_currentBet = player.currentBet;
        if (player.stack <= 0) {
            player.allIn = true;
            player.lastAction = "ALL IN";
            logAction(m_activePlayerSeat, "ALL IN (raise)", player.currentBet);
        } else {
            player.lastAction = QString("Raise %1").arg(player.currentBet, 0, 'f', 0);
            logAction(m_activePlayerSeat, "RAISE", player.currentBet);
        }
        player.hasBet = true;
        m_actionCount = 1; // reset action count since raise reopens action
    }
    else if (action == "allin") {
        double allInAmount = player.stack;
        player.currentBet += allInAmount;
        player.totalBetThisHand += allInAmount;
        m_pot += allInAmount;
        player.stack = 0;
        player.allIn = true;
        if (player.currentBet > m_currentBet) {
            double raiseSize = player.currentBet - m_currentBet;
            // Only reopens action if the all-in is a full raise
            if (raiseSize >= m_lastRaiseSize) {
                m_lastRaiseSize = raiseSize;
                m_actionCount = 1; // reopens action
            }
            m_currentBet = player.currentBet;
        }
        player.lastAction = "ALL IN";
        player.hasBet = true;
        logAction(m_activePlayerSeat, "ALL IN", player.currentBet);
    }
    else {
        return; // unknown action
    }

    m_actionCount++;
    emit playerActed(m_id, m_activePlayerSeat, player.lastAction, player.currentBet);
    emit stateChanged(m_id);

    advanceToNextPlayer();
}

void PokerTable::advanceToNextPlayer()
{
    // Check if only one non-folded player remains
    int active = 0;
    int lastActive = -1;
    for (int i = 0; i < m_seats.size(); ++i) {
        if (m_seats[i].seated && !m_seats[i].folded) {
            active++;
            lastActive = i;
        }
    }

    if (active <= 1) {
        awardPot();
        return;
    }

    // Check if betting round is complete
    if (isBettingComplete()) {
        nextPhase();
        return;
    }

    // Move to next active player
    m_activePlayerSeat = nextActivePlayer(m_activePlayerSeat);
    emit stateChanged(m_id);
    notifyTurn();
}

int PokerTable::nextActivePlayer(int from) const
{
    for (int i = 1; i <= m_maxSeats; ++i) {
        int idx = (from + i) % m_maxSeats;
        if (m_seats[idx].seated && !m_seats[idx].folded && !m_seats[idx].allIn && m_seats[idx].stack >= 0)
            return idx;
    }
    return from;
}

int PokerTable::firstPostFlopPlayer() const
{
    // Post-flop: small blind acts first, or first active player after dealer
    int sbIdx = nextActivePlayer(m_dealerSeat);

    // Check if SB is still active
    if (m_seats[sbIdx].seated && !m_seats[sbIdx].folded && !m_seats[sbIdx].allIn) {
        return sbIdx;
    }

    // Otherwise find first active after dealer
    return nextActivePlayer(m_dealerSeat);
}

bool PokerTable::isBettingComplete() const
{
    int activePlayers = 0;
    for (const auto& s : m_seats) {
        if (!s.seated || s.folded || s.allIn) continue;
        activePlayers++;
        if (!s.hasBet) return false;
        if (s.currentBet != m_currentBet)
            return false;
    }
    return m_actionCount >= activePlayers;
}

void PokerTable::notifyTurn()
{
    if (m_activePlayerSeat < 0 || m_activePlayerSeat >= m_seats.size()) return;
    auto& p = m_seats[m_activePlayerSeat];
    if (!p.seated || p.folded || p.allIn) return;

    double toCall = m_currentBet - p.currentBet;
    double minRaise = m_currentBet + qMax(m_bigBlind, m_lastRaiseSize);

    // Start action timer and countdown
    m_actionDeadline = QDateTime::currentDateTime().addSecs(ACTION_TIMEOUT_SECS);
    m_actionTimer->start();
    m_countdownTimer->start();

    emit timerUpdate(m_id, m_activePlayerSeat, ACTION_TIMEOUT_SECS);
    emit turnChanged(m_id, p.id, toCall, minRaise);
}

void PokerTable::onTimerTick()
{
    if (!m_handActive || m_activePlayerSeat < 0) {
        m_countdownTimer->stop();
        return;
    }

    int secsLeft = secondsLeft();
    emit timerUpdate(m_id, m_activePlayerSeat, secsLeft);

    if (secsLeft <= 0) {
        m_countdownTimer->stop();
    }
}

void PokerTable::autoFoldTimeout()
{
    if (!m_handActive) return;
    if (m_activePlayerSeat < 0 || m_activePlayerSeat >= m_seats.size()) return;

    auto& p = m_seats[m_activePlayerSeat];
    if (!p.seated || p.folded || p.allIn) return;

    m_countdownTimer->stop();

    // Auto-fold on timeout (or check if free)
    double toCall = m_currentBet - p.currentBet;
    if (toCall <= 0) {
        // Can check for free
        p.lastAction = "Check";
        p.hasBet = true;
        logAction(m_activePlayerSeat, "CHECK (timeout)", 0);
    } else {
        p.folded = true;
        p.lastAction = "Fold (timeout)";
        logAction(m_activePlayerSeat, "FOLD (timeout)", 0);
    }
    m_actionCount++;

    emit playerActed(m_id, m_activePlayerSeat, p.lastAction, p.currentBet);
    emit stateChanged(m_id);
    advanceToNextPlayer();
}

// ─── Dealing ──────────────────────────────────────────────────────────────────

void PokerTable::dealPreflop()
{
    // Deck should already be shuffled (seeded) before calling this
    m_community.clear();

    for (auto& s : m_seats) {
        s.holeCards.clear();
        s.folded = false;
        s.allIn = (s.stack <= 0 && s.currentBet > 0); // keep all-in from blinds
    }

    // Deal 2 hole cards to each seated player with chips (or all-in from blind)
    for (int round = 0; round < 2; ++round) {
        for (auto& s : m_seats) {
            if (s.seated)
                s.holeCards.append(m_deck.deal());
        }
    }
}

void PokerTable::dealFlop()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    m_community.append(m_deck.deal());
    m_community.append(m_deck.deal());
    logAction(-1, "FLOP", 0);
}

void PokerTable::dealTurn()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    logAction(-1, "TURN", 0);
}

void PokerTable::dealRiver()
{
    m_deck.deal(); // burn
    m_community.append(m_deck.deal());
    logAction(-1, "RIVER", 0);
}

void PokerTable::collectBets()
{
    for (auto& s : m_seats) {
        s.currentBet = 0;
        s.hasBet = false;
        s.lastAction.clear();
    }
    m_currentBet = 0;
    m_lastRaiseSize = m_bigBlind;
    m_actionCount = 0;
}

void PokerTable::nextPhase()
{
    collectBets();

    // Count active (non-folded, seated) players
    int active = 0;
    int notAllIn = 0;
    for (const auto& s : m_seats) {
        if (s.seated && !s.folded) {
            active++;
            if (!s.allIn) notAllIn++;
        }
    }

    if (active <= 1) {
        awardPot();
        return;
    }

    m_phase++;

    if (m_phase == 1) dealFlop();
    else if (m_phase == 2) dealTurn();
    else if (m_phase == 3) dealRiver();
    else if (m_phase >= 4) {
        showdown();
        return;
    }

    emit stateChanged(m_id);

    // If all remaining players are all-in, run out the board
    if (notAllIn <= 1) {
        QTimer::singleShot(800, this, &PokerTable::nextPhase);
        return;
    }

    // First to act post-flop: first active player after dealer (SB position)
    m_activePlayerSeat = firstPostFlopPlayer();
    m_actionCount = 0;
    notifyTurn();
}

// ─── Side Pot Calculation ────────────────────────────────────────────────────

QVector<SidePot> PokerTable::calculateSidePots() const
{
    // Collect all-in amounts from players still in the hand
    struct PlayerContrib {
        int seat;
        double totalBet;
        bool folded;
    };
    QVector<PlayerContrib> contribs;

    for (int i = 0; i < m_seats.size(); ++i) {
        if (!m_seats[i].seated) continue;
        if (m_seats[i].totalBetThisHand <= 0 && m_seats[i].folded) continue;
        contribs.append({i, m_seats[i].totalBetThisHand, m_seats[i].folded});
    }

    if (contribs.isEmpty()) return {};

    // Get unique bet levels (sorted ascending)
    QVector<double> levels;
    for (const auto& c : contribs) {
        if (c.totalBet > 0 && !levels.contains(c.totalBet))
            levels.append(c.totalBet);
    }
    std::sort(levels.begin(), levels.end());

    QVector<SidePot> pots;
    double prevLevel = 0;

    for (double level : levels) {
        double potAmount = 0;
        QVector<int> eligible;

        double slice = level - prevLevel;
        for (const auto& c : contribs) {
            if (c.totalBet >= level) {
                potAmount += slice;
                if (!c.folded) eligible.append(c.seat);
            } else if (c.totalBet > prevLevel) {
                potAmount += (c.totalBet - prevLevel);
                // Folded players contribute but aren't eligible
            }
        }

        if (potAmount > 0 && !eligible.isEmpty()) {
            pots.append({potAmount, eligible});
        }

        prevLevel = level;
    }

    return pots;
}

// ─── Showdown ────────────────────────────────────────────────────────────────

void PokerTable::showdown()
{
    m_phase = 4;
    m_handActive = false;
    m_actionTimer->stop();
    m_countdownTimer->stop();

    // Evaluate all remaining hands
    struct HandResult {
        int seat;
        int score;
        HandRank rank;
        QVector<Card> bestHand;
    };
    QVector<HandResult> results;

    for (int i = 0; i < m_seats.size(); ++i) {
        if (!m_seats[i].seated || m_seats[i].folded) continue;
        QVector<Card> allCards = m_seats[i].holeCards + m_community;
        int score = handScore(allCards);
        HandRank rank = evaluateHand(allCards);
        results.append({i, score, rank, m_seats[i].holeCards});
    }

    if (results.isEmpty()) {
        resetHand();
        return;
    }

    // Calculate side pots
    QVector<SidePot> sidePots = calculateSidePots();

    // If no side pots calculated (shouldn't happen), fall back to simple pot
    if (sidePots.isEmpty()) {
        SidePot fallbackPot;
        fallbackPot.amount = m_pot;
        for (const auto& r : results)
            fallbackPot.eligibleSeats.append(r.seat);
        sidePots.append(fallbackPot);
    }

    // Build showdown result
    QJsonObject showdownMsg;
    showdownMsg["type"] = "showdown";

    QJsonArray winnersArray;
    QJsonArray sidePotsArray;

    for (const auto& sp : sidePots) {
        // Find best hand among eligible players in this pot
        int bestScore = -1;
        QVector<int> potWinners;

        for (int seat : sp.eligibleSeats) {
            for (const auto& r : results) {
                if (r.seat == seat) {
                    if (r.score > bestScore) {
                        bestScore = r.score;
                        potWinners.clear();
                        potWinners.append(seat);
                    } else if (r.score == bestScore) {
                        potWinners.append(seat);
                    }
                    break;
                }
            }
        }

        double share = potWinners.isEmpty() ? 0 : sp.amount / potWinners.size();

        QJsonObject potObj;
        potObj["amount"] = sp.amount;
        QJsonArray potWinnersArr;
        for (int ws : potWinners) {
            m_seats[ws].stack += share;

            QJsonObject w;
            w["seat"] = ws;
            w["name"] = m_seats[ws].name;
            // Find this player's hand rank
            for (const auto& r : results) {
                if (r.seat == ws) {
                    w["hand_name"] = rankName(r.rank);
                    break;
                }
            }
            QJsonArray cardsArr;
            for (const auto& c : m_seats[ws].holeCards)
                cardsArr.append(c.toString());
            w["cards"] = cardsArr;
            w["pot_won"] = share;
            winnersArray.append(w);
            potWinnersArr.append(ws);
        }
        potObj["winners"] = potWinnersArr;
        sidePotsArray.append(potObj);
    }
    showdownMsg["winners"] = winnersArray;
    showdownMsg["side_pots"] = sidePotsArray;

    QJsonArray allHandsArray;
    for (const auto& r : results) {
        QJsonObject h;
        h["seat"] = r.seat;
        h["name"] = m_seats[r.seat].name;
        h["hand_name"] = rankName(r.rank);
        h["score"] = r.score;
        QJsonArray cardsArr;
        for (const auto& c : m_seats[r.seat].holeCards)
            cardsArr.append(c.toString());
        h["cards"] = cardsArr;
        allHandsArray.append(h);
    }
    showdownMsg["all_hands"] = allHandsArray;
    showdownMsg["pot"] = m_pot;

    logAction(-1, "SHOWDOWN", m_pot);

    // Finalize provably fair metadata
    finalizeHandMetadata(showdownMsg);
    saveHandHistory();

    m_pot = 0;
    emit showdownResult(m_id, showdownMsg);

    // Broadcast hand_complete with server seed reveal
    QJsonObject metaObj;
    metaObj["server_seed_hash"] = m_handMeta.serverSeedHash;
    metaObj["combined_seed"] = m_handMeta.combinedSeed;
    metaObj["result_hash"] = m_handMeta.resultHash;
    QJsonArray actionsArr;
    for (const auto& a : m_handMeta.actionLog) actionsArr.append(a);
    metaObj["actions"] = actionsArr;

    emit handComplete(m_id, m_handNumber, m_handMeta.serverSeed, metaObj);
    emit stateChanged(m_id);

    // Auto-start next hand after delay
    QTimer::singleShot(5000, this, &PokerTable::tryStartHand);
}

void PokerTable::awardPot()
{
    m_handActive = false;
    m_actionTimer->stop();
    m_countdownTimer->stop();

    // Find the last remaining non-folded player
    int winnerSeat = -1;
    for (int i = 0; i < m_seats.size(); ++i) {
        if (m_seats[i].seated && !m_seats[i].folded) {
            winnerSeat = i;
            break;
        }
    }

    if (winnerSeat < 0) {
        resetHand();
        return;
    }

    m_seats[winnerSeat].stack += m_pot;

    QJsonObject result;
    result["type"] = "showdown";
    QJsonArray winnersArray;
    QJsonObject w;
    w["seat"] = winnerSeat;
    w["name"] = m_seats[winnerSeat].name;
    w["hand_name"] = "Others folded";
    w["cards"] = QJsonArray();
    w["pot_won"] = m_pot;
    winnersArray.append(w);
    result["winners"] = winnersArray;
    result["all_hands"] = QJsonArray();
    result["pot"] = m_pot;

    logAction(winnerSeat, "WIN (others folded)", m_pot);

    // Finalize provably fair metadata
    finalizeHandMetadata(result);
    saveHandHistory();

    m_pot = 0;

    // Broadcast hand_complete with server seed reveal
    QJsonObject metaObj;
    metaObj["server_seed_hash"] = m_handMeta.serverSeedHash;
    metaObj["combined_seed"] = m_handMeta.combinedSeed;
    metaObj["result_hash"] = m_handMeta.resultHash;

    emit showdownResult(m_id, result);
    emit handComplete(m_id, m_handNumber, m_handMeta.serverSeed, metaObj);
    emit stateChanged(m_id);

    // Auto-start next hand
    QTimer::singleShot(3000, this, &PokerTable::tryStartHand);
}

void PokerTable::resetHand()
{
    m_deck.reset();
    m_community.clear();
    m_pot = 0;
    m_currentBet = 0;
    m_lastRaiseSize = m_bigBlind;
    m_phase = -1;
    m_activePlayerSeat = -1;
    m_actionCount = 0;
    m_handActive = false;
    m_actionTimer->stop();
    m_countdownTimer->stop();
    m_waitingForSeeds = false;
    m_pendingClientSeeds.clear();

    for (auto& s : m_seats) {
        s.holeCards.clear();
        s.folded = false;
        s.allIn = false;
        s.hasBet = false;
        s.currentBet = 0;
        s.totalBetThisHand = 0;
        s.lastAction.clear();
    }
}

// ─── State Serialization ──────────────────────────────────────────────────────

QJsonObject PokerTable::tableInfo() const
{
    QJsonObject info;
    info["id"] = m_id;
    info["name"] = m_name;
    info["small_blind"] = m_smallBlind;
    info["big_blind"] = m_bigBlind;
    info["min_buy_in"] = m_minBuyIn;
    info["max_buy_in"] = m_maxBuyIn;
    info["max_seats"] = m_maxSeats;
    info["players"] = playerCount();
    info["hand_active"] = m_handActive;
    info["avg_pot"] = m_pot;
    return info;
}

QJsonObject PokerTable::fullState(const QString& forPlayerId) const
{
    static const QStringList phaseNames = {"preflop", "flop", "turn", "river", "showdown", "waiting"};

    QJsonObject state;
    state["type"] = "table_state";
    state["table_id"] = m_id;
    state["table_name"] = m_name;
    state["phase"] = (m_phase >= 0 && m_phase <= 4) ? phaseNames[m_phase] : "waiting";
    state["pot"] = m_pot;
    state["current_bet"] = m_currentBet;
    state["active_seat"] = m_activePlayerSeat;
    state["dealer_seat"] = m_dealerSeat;
    state["hand_active"] = m_handActive;
    state["hand_number"] = m_handNumber;
    state["small_blind"] = m_smallBlind;
    state["big_blind"] = m_bigBlind;

    // Timer info
    if (m_handActive && m_activePlayerSeat >= 0) {
        state["timer_seat"] = m_activePlayerSeat;
        state["seconds_left"] = secondsLeft();
    }

    // Community cards
    QJsonArray communityArr;
    for (const auto& c : m_community)
        communityArr.append(c.toString());
    state["community"] = communityArr;

    // Players
    QJsonArray playersArr;
    for (int i = 0; i < m_seats.size(); ++i) {
        QJsonObject pObj;
        pObj["seat"] = i;
        pObj["seated"] = m_seats[i].seated;
        if (m_seats[i].seated) {
            pObj["name"] = m_seats[i].name;
            pObj["stack"] = m_seats[i].stack;
            pObj["folded"] = m_seats[i].folded;
            pObj["all_in"] = m_seats[i].allIn;
            pObj["bet"] = m_seats[i].currentBet;
            pObj["last_action"] = m_seats[i].lastAction;
            pObj["connected"] = m_seats[i].connected;

            // Show hole cards only to the requesting player, or at showdown
            bool showCards = (m_seats[i].id == forPlayerId) || (m_phase == 4 && !m_seats[i].folded);
            if (showCards && !m_seats[i].holeCards.isEmpty()) {
                QJsonArray cardsArr;
                for (const auto& c : m_seats[i].holeCards)
                    cardsArr.append(c.toString());
                pObj["cards"] = cardsArr;
                pObj["cards_visible"] = true;
            } else {
                pObj["cards_visible"] = !m_seats[i].holeCards.isEmpty();
            }
        }
        playersArr.append(pObj);
    }
    state["players"] = playersArr;

    return state;
}

// ─── Hand Evaluation ──────────────────────────────────────────────────────────

PokerTable::HandRank PokerTable::evaluateHand(const QVector<Card>& cards) const
{
    if (cards.size() < 5) {
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
    for (int r : ranks)
        if (uniqueRanks.isEmpty() || uniqueRanks.last() != r)
            uniqueRanks.append(r);
    if (uniqueRanks.contains(1)) uniqueRanks.append(14); // Ace high
    for (int i = 0; i <= (int)uniqueRanks.size() - 5; ++i) {
        if (uniqueRanks[i + 4] - uniqueRanks[i] == 4) hasStraight = true;
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

int PokerTable::handScore(const QVector<Card>& allCards) const
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

QString PokerTable::rankName(HandRank rank) const
{
    switch (rank) {
    case HighCard:      return "High Card";
    case OnePair:       return "One Pair";
    case TwoPair:       return "Two Pair";
    case ThreeOfAKind:  return "Three of a Kind";
    case Straight:      return "Straight";
    case Flush:         return "Flush";
    case FullHouse:     return "Full House";
    case FourOfAKind:   return "Four of a Kind";
    case StraightFlush: return "Straight Flush";
    case RoyalFlush:    return "Royal Flush";
    }
    return "Unknown";
}

QVector<Card> PokerTable::bestFiveCards(const QVector<Card>& allCards) const
{
    // Simple: just return all cards (evaluation already handles 7-card combos)
    return allCards;
}
