#include "Tournament.h"
#include <QRandomGenerator>
#include <QtMath>

// ─── Constructor / Destructor ────────────────────────────────────────────────────

Tournament::Tournament(const QString& id, const TournamentConfig& config, QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_config(config)
{
    m_config.ensureDefaults();

    m_blindTimer = new QTimer(this);
    m_blindTimer->setTimerType(Qt::PreciseTimer);
    connect(m_blindTimer, &QTimer::timeout, this, &Tournament::advanceBlinds);
}

Tournament::~Tournament()
{
    m_blindTimer->stop();
}

// ─── Registration ────────────────────────────────────────────────────────────────

bool Tournament::registerPlayer(const QString& playerId, const QString& name)
{
    // Only allow registration while Registering (or late reg in Running)
    if (m_status == TournamentStatus::Finished || m_status == TournamentStatus::FinalTable)
        return false;

    if (m_status == TournamentStatus::Running) {
        if (!m_config.allowLateReg)
            return false;
        if (m_currentBlindLevel >= m_config.lateRegLevels)
            return false;
    }

    if (isFull())
        return false;

    // Check not already registered
    for (const auto& p : m_players) {
        if (p.playerId == playerId)
            return false;
    }

    TournamentPlayer tp;
    tp.playerId = playerId;
    tp.name = name;
    tp.chips = m_config.startingStack;
    tp.registeredAt = QDateTime::currentDateTime();
    tp.tableId = -1;

    m_players.append(tp);
    m_prizePool += m_config.buyIn;

    // If tournament is already running (late reg), assign to a table
    if (m_status == TournamentStatus::Running) {
        int idx = m_players.size() - 1;
        m_players[idx].tableId = assignTableForPlayer();
        balanceTables();
    }

    return true;
}

void Tournament::unregisterPlayer(const QString& playerId)
{
    if (m_status != TournamentStatus::Registering)
        return;

    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].playerId == playerId) {
            m_prizePool -= m_config.buyIn;
            m_players.removeAt(i);
            return;
        }
    }
}

int Tournament::registeredCount() const
{
    return m_players.size();
}

bool Tournament::isFull() const
{
    return m_players.size() >= m_config.maxPlayers;
}

// ─── Tournament Lifecycle ────────────────────────────────────────────────────────

void Tournament::start()
{
    if (m_status != TournamentStatus::Registering)
        return;

    int count = m_players.size();
    if (count < 2)
        return;

    m_startTime = QDateTime::currentDateTime();
    m_lastBlindAdvance = m_startTime;
    m_currentBlindLevel = 0;
    m_eliminationOrder = 0;

    // Determine number of tables needed (max 9 per table)
    int playersPerTable = 9;
    if (m_config.type == TournamentType::SitAndGo && m_config.maxPlayers <= 9) {
        playersPerTable = m_config.maxPlayers;
    }
    m_numTables = qMax(1, (count + playersPerTable - 1) / playersPerTable);
    m_nextTableId = m_numTables;

    // Shuffle player order for random seating
    for (int i = count - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        m_players.swapItemsAt(i, j);
    }

    // Assign players to tables round-robin
    for (int i = 0; i < count; ++i) {
        m_players[i].tableId = i % m_numTables;
    }

    // Start blind timer
    m_blindTimer->start(m_config.blindLevelMinutes * 60 * 1000);

    setStatus(TournamentStatus::Running);
    emit tournamentStarted(m_id);
}

void Tournament::onHandComplete(int tableIndex, const QString& winnerId)
{
    Q_UNUSED(tableIndex);
    Q_UNUSED(winnerId);
    // After each hand, check if we need to balance or merge tables
    if (m_status == TournamentStatus::Running || m_status == TournamentStatus::FinalTable) {
        mergeTablesIfNeeded();
        balanceTables();
        checkForWinner();
    }
}

void Tournament::onPlayerEliminated(const QString& playerId, int tableIndex)
{
    Q_UNUSED(tableIndex);
    onPlayerBusted(playerId);
}

void Tournament::onPlayerBusted(const QString& playerId)
{
    TournamentPlayer* tp = findPlayer(playerId);
    if (!tp || tp->eliminated)
        return;

    tp->eliminated = true;
    tp->eliminatedAt = QDateTime::currentDateTime();
    tp->chips = 0;
    tp->tableId = -1;

    m_eliminationOrder++;

    // Finish position: total players minus elimination order + 1
    // Last person eliminated = 2nd place, etc.
    int remaining = playersRemaining();
    tp->finishPosition = remaining + 1; // +1 because remaining doesn't include this player anymore since we just set eliminated

    // Calculate prize
    calculatePrizes(); // recalculate in case of rebuys
    QVector<QPair<int, double>> prizes = prizeTable();
    for (const auto& pr : prizes) {
        if (pr.first == tp->finishPosition) {
            tp->prizeWon = pr.second;
            break;
        }
    }

    emit playerEliminated(playerId, tp->finishPosition, tp->prizeWon);

    // Check if tournament is over or final table
    checkForWinner();
    mergeTablesIfNeeded();
}

// ─── Blind Management ────────────────────────────────────────────────────────────

void Tournament::advanceBlinds()
{
    if (m_status != TournamentStatus::Running && m_status != TournamentStatus::FinalTable)
        return;

    m_currentBlindLevel++;
    m_lastBlindAdvance = QDateTime::currentDateTime();

    if (m_currentBlindLevel >= m_config.blindSchedule.size()) {
        // Stay at the last level (cap)
        m_currentBlindLevel = m_config.blindSchedule.size() - 1;
    }

    auto blinds = currentBlinds();
    emit blindsIncreased(m_currentBlindLevel, blinds.first, blinds.second);
}

int Tournament::currentBlindLevel() const
{
    return m_currentBlindLevel;
}

QPair<double, double> Tournament::currentBlinds() const
{
    if (m_currentBlindLevel >= 0 && m_currentBlindLevel < m_config.blindSchedule.size()) {
        return m_config.blindSchedule[m_currentBlindLevel];
    }
    // Fallback: last level
    if (!m_config.blindSchedule.isEmpty()) {
        return m_config.blindSchedule.last();
    }
    return {10, 20};
}

int Tournament::minutesUntilNextLevel() const
{
    return (secondsUntilNextLevel() + 59) / 60;
}

int Tournament::secondsUntilNextLevel() const
{
    if (m_status != TournamentStatus::Running && m_status != TournamentStatus::FinalTable)
        return 0;

    qint64 elapsed = m_lastBlindAdvance.secsTo(QDateTime::currentDateTime());
    int totalSecs = m_config.blindLevelMinutes * 60;
    int remaining = totalSecs - static_cast<int>(elapsed);
    return qMax(0, remaining);
}

// ─── Table Management ────────────────────────────────────────────────────────────

void Tournament::balanceTables()
{
    if (m_numTables <= 1)
        return;

    // Count players per table
    QMap<int, int> tableCounts;
    for (int t = 0; t < m_numTables; ++t)
        tableCounts[t] = 0;

    for (const auto& p : m_players) {
        if (!p.eliminated && p.tableId >= 0) {
            tableCounts[p.tableId]++;
        }
    }

    // Find the table with the most and least players
    bool moved = true;
    while (moved) {
        moved = false;

        int maxTable = -1, maxCount = 0;
        int minTable = -1, minCount = INT_MAX;

        for (auto it = tableCounts.begin(); it != tableCounts.end(); ++it) {
            if (it.value() > maxCount) {
                maxCount = it.value();
                maxTable = it.key();
            }
            if (it.value() < minCount && it.value() > 0) {
                minCount = it.value();
                minTable = it.key();
            }
        }

        // If difference > 1, move one player from max to min
        if (maxTable >= 0 && minTable >= 0 && maxTable != minTable && (maxCount - minCount) > 1) {
            for (auto& p : m_players) {
                if (!p.eliminated && p.tableId == maxTable) {
                    p.tableId = minTable;
                    tableCounts[maxTable]--;
                    tableCounts[minTable]++;
                    moved = true;
                    break;
                }
            }
        }
    }

    emit tablesRebalanced();
}

void Tournament::mergeTablesIfNeeded()
{
    if (m_numTables <= 1)
        return;

    int remaining = playersRemaining();

    // If all remaining players fit in fewer tables, merge
    int maxPerTable = 9;
    int neededTables = qMax(1, (remaining + maxPerTable - 1) / maxPerTable);

    if (neededTables < m_numTables) {
        // Collect all active players
        QVector<int> activePlayerIndices;
        for (int i = 0; i < m_players.size(); ++i) {
            if (!m_players[i].eliminated) {
                activePlayerIndices.append(i);
            }
        }

        m_numTables = neededTables;

        // Reassign tables round-robin
        for (int i = 0; i < activePlayerIndices.size(); ++i) {
            m_players[activePlayerIndices[i]].tableId = i % m_numTables;
        }

        // Check if we're down to 1 table = final table
        if (m_numTables == 1 && m_status == TournamentStatus::Running) {
            setStatus(TournamentStatus::FinalTable);
        }

        emit tablesRebalanced();
    }
}

// ─── Rebuy ───────────────────────────────────────────────────────────────────────

bool Tournament::canRebuy(const QString& playerId) const
{
    if (!m_config.allowRebuys)
        return false;
    if (m_currentBlindLevel >= m_config.rebuyLevels)
        return false;

    const TournamentPlayer* tp = findPlayer(playerId);
    if (!tp)
        return false;

    // Can only rebuy if eliminated or chips <= starting stack
    // (common rebuy rule: chips must be at or below starting stack)
    return tp->eliminated || tp->chips <= m_config.startingStack;
}

bool Tournament::processRebuy(const QString& playerId)
{
    if (!canRebuy(playerId))
        return false;

    TournamentPlayer* tp = findPlayer(playerId);
    if (!tp)
        return false;

    if (tp->eliminated) {
        tp->eliminated = false;
        tp->eliminatedAt = QDateTime();
        tp->finishPosition = 0;
        tp->chips = m_config.startingStack;
        tp->tableId = assignTableForPlayer();
        m_eliminationOrder--; // undo elimination counter
    } else {
        tp->chips += m_config.startingStack;
    }

    tp->rebuysUsed++;
    m_prizePool += m_config.rebuyAmount;

    emit playerRebuyed(playerId);
    return true;
}

// ─── Status Queries ──────────────────────────────────────────────────────────────

QVector<TournamentPlayer> Tournament::leaderboard() const
{
    QVector<TournamentPlayer> lb;
    for (const auto& p : m_players) {
        if (!p.eliminated) {
            lb.append(p);
        }
    }
    // Sort by chips descending
    std::sort(lb.begin(), lb.end(), [](const TournamentPlayer& a, const TournamentPlayer& b) {
        return a.chips > b.chips;
    });
    return lb;
}

int Tournament::playersRemaining() const
{
    int count = 0;
    for (const auto& p : m_players) {
        if (!p.eliminated)
            count++;
    }
    return count;
}

int Tournament::tablesActive() const
{
    if (m_status == TournamentStatus::Registering || m_status == TournamentStatus::Finished)
        return 0;
    return m_numTables;
}

QVector<QPair<int, double>> Tournament::prizeTable() const
{
    QVector<QPair<int, double>> result;
    for (int i = 0; i < m_config.prizeStructure.size(); ++i) {
        double prize = m_prizePool * m_config.prizeStructure[i] / 100.0;
        result.append({i + 1, prize}); // position is 1-based
    }
    return result;
}

TournamentPlayer* Tournament::findPlayer(const QString& playerId)
{
    for (auto& p : m_players) {
        if (p.playerId == playerId)
            return &p;
    }
    return nullptr;
}

const TournamentPlayer* Tournament::findPlayer(const QString& playerId) const
{
    for (const auto& p : m_players) {
        if (p.playerId == playerId)
            return &p;
    }
    return nullptr;
}

// ─── Serialization ───────────────────────────────────────────────────────────────

QJsonObject Tournament::toJson() const
{
    QJsonObject o;
    o["id"] = m_id;
    o["config"] = m_config.toJson();

    QString statusStr;
    switch (m_status) {
    case TournamentStatus::Registering: statusStr = "Registering"; break;
    case TournamentStatus::Running:     statusStr = "Running"; break;
    case TournamentStatus::FinalTable:  statusStr = "FinalTable"; break;
    case TournamentStatus::Finished:    statusStr = "Finished"; break;
    }
    o["status"] = statusStr;

    o["registeredCount"] = registeredCount();
    o["playersRemaining"] = playersRemaining();
    o["tablesActive"] = tablesActive();
    o["currentBlindLevel"] = m_currentBlindLevel;

    auto blinds = currentBlinds();
    o["currentSB"] = blinds.first;
    o["currentBB"] = blinds.second;
    o["secondsUntilNextLevel"] = secondsUntilNextLevel();
    o["prizePool"] = m_prizePool;

    if (m_startTime.isValid())
        o["startTime"] = m_startTime.toString(Qt::ISODate);

    QJsonArray playersArr;
    for (const auto& p : m_players)
        playersArr.append(p.toJson());
    o["players"] = playersArr;

    QJsonArray prizesArr;
    auto pt = prizeTable();
    for (const auto& pr : pt) {
        QJsonObject po;
        po["position"] = pr.first;
        po["prize"] = pr.second;
        prizesArr.append(po);
    }
    o["prizeTable"] = prizesArr;

    return o;
}

// ─── Private Helpers ─────────────────────────────────────────────────────────────

void Tournament::calculatePrizes()
{
    // Prize pool is already tracked incrementally via registrations and rebuys
    // Nothing extra needed here unless we want to recalculate structure
}

void Tournament::checkForWinner()
{
    int remaining = playersRemaining();

    if (remaining == 1) {
        // Find the winner
        for (auto& p : m_players) {
            if (!p.eliminated) {
                p.finishPosition = 1;
                // Award 1st place prize
                QVector<QPair<int, double>> prizes = prizeTable();
                for (const auto& pr : prizes) {
                    if (pr.first == 1) {
                        p.prizeWon = pr.second;
                        break;
                    }
                }
                emit playerEliminated(p.playerId, 1, p.prizeWon);
                break;
            }
        }

        setStatus(TournamentStatus::Finished);
        m_blindTimer->stop();
        emit tournamentFinished(m_id, m_players);
    } else if (remaining == 0) {
        // Edge case: shouldn't happen, but handle gracefully
        setStatus(TournamentStatus::Finished);
        m_blindTimer->stop();
        emit tournamentFinished(m_id, m_players);
    }
}

void Tournament::setStatus(TournamentStatus s)
{
    if (m_status == s)
        return;
    m_status = s;
    emit statusChanged(s);
}

int Tournament::assignTableForPlayer()
{
    if (m_numTables <= 0)
        return 0;

    // Find table with fewest players
    QMap<int, int> counts;
    for (int t = 0; t < m_numTables; ++t)
        counts[t] = 0;

    for (const auto& p : m_players) {
        if (!p.eliminated && p.tableId >= 0 && p.tableId < m_numTables) {
            counts[p.tableId]++;
        }
    }

    int minTable = 0;
    int minCount = INT_MAX;
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        if (it.value() < minCount) {
            minCount = it.value();
            minTable = it.key();
        }
    }

    return minTable;
}
