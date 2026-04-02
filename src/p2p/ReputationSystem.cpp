#include "ReputationSystem.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

ReputationSystem::ReputationSystem(QObject* parent)
    : QObject(parent)
{
}

void ReputationSystem::ensurePlayer(const QString& playerId)
{
    if (!m_players.contains(playerId)) {
        PlayerReputation rep;
        rep.playerId = playerId;
        rep.fairPlayScore = 100.0;
        rep.firstSeen = QDateTime::currentDateTimeUtc();
        rep.lastSeen = rep.firstSeen;
        m_players[playerId] = rep;
    }
}

void ReputationSystem::clampScore(PlayerReputation& rep)
{
    if (rep.fairPlayScore < 0.0)
        rep.fairPlayScore = 0.0;
    if (rep.fairPlayScore > 100.0)
        rep.fairPlayScore = 100.0;
}

void ReputationSystem::recordHandPlayed(const QString& playerId)
{
    ensurePlayer(playerId);
    auto& rep = m_players[playerId];
    rep.handsPlayed++;
    rep.fairPlayScore += 0.1;
    rep.lastSeen = QDateTime::currentDateTimeUtc();
    clampScore(rep);
    emit reputationChanged(playerId);
}

void ReputationSystem::recordHandWon(const QString& playerId, double amount)
{
    ensurePlayer(playerId);
    auto& rep = m_players[playerId];
    rep.handsWon++;
    rep.totalWon += amount;
    rep.lastSeen = QDateTime::currentDateTimeUtc();
    emit reputationChanged(playerId);
}

void ReputationSystem::recordDisconnect(const QString& playerId)
{
    ensurePlayer(playerId);
    auto& rep = m_players[playerId];
    rep.disconnects++;
    rep.fairPlayScore -= 5.0;
    rep.lastSeen = QDateTime::currentDateTimeUtc();
    clampScore(rep);

    if (rep.fairPlayScore < 20.0) {
        emit playerBanned(playerId);
    }

    emit reputationChanged(playerId);
}

void ReputationSystem::recordGameComplete(const QString& playerId)
{
    ensurePlayer(playerId);
    auto& rep = m_players[playerId];
    rep.gamesCompleted++;
    rep.fairPlayScore += 0.5;
    rep.lastSeen = QDateTime::currentDateTimeUtc();
    clampScore(rep);
    emit reputationChanged(playerId);
}

void ReputationSystem::recordGameHash(const QString& playerId, const QString& hash)
{
    ensurePlayer(playerId);
    m_players[playerId].gameHashes.append(hash);
    m_players[playerId].lastSeen = QDateTime::currentDateTimeUtc();
}

void ReputationSystem::recordWager(const QString& playerId, double amount)
{
    ensurePlayer(playerId);
    m_players[playerId].totalWagered += amount;
    m_players[playerId].lastSeen = QDateTime::currentDateTimeUtc();
}

void ReputationSystem::setWalletAddress(const QString& playerId, const QString& address)
{
    ensurePlayer(playerId);
    m_players[playerId].walletAddress = address;
}

PlayerReputation ReputationSystem::getReputation(const QString& playerId) const
{
    if (m_players.contains(playerId))
        return m_players[playerId];
    PlayerReputation empty;
    empty.playerId = playerId;
    return empty;
}

double ReputationSystem::fairPlayScore(const QString& playerId) const
{
    if (m_players.contains(playerId))
        return m_players[playerId].fairPlayScore;
    return 100.0;  // New players start trusted
}

bool ReputationSystem::isTrusted(const QString& playerId) const
{
    return fairPlayScore(playerId) > 70.0;
}

bool ReputationSystem::isBanned(const QString& playerId) const
{
    return fairPlayScore(playerId) < 20.0;
}

bool ReputationSystem::hasPlayer(const QString& playerId) const
{
    return m_players.contains(playerId);
}

int ReputationSystem::playerCount() const
{
    return m_players.size();
}

QVector<PlayerReputation> ReputationSystem::leaderboard(int top) const
{
    QVector<PlayerReputation> all;
    all.reserve(m_players.size());
    for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
        all.append(it.value());
    }

    // Sort by totalWon descending
    std::sort(all.begin(), all.end(), [](const PlayerReputation& a, const PlayerReputation& b) {
        return a.totalWon > b.totalWon;
    });

    if (all.size() > top)
        all.resize(top);

    return all;
}

void ReputationSystem::updateFairPlayScores()
{
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        auto& rep = it.value();
        // Recalculate from scratch
        rep.fairPlayScore = 100.0;
        rep.fairPlayScore -= 5.0 * rep.disconnects;
        rep.fairPlayScore += 0.1 * rep.handsPlayed;
        rep.fairPlayScore += 0.5 * rep.gamesCompleted;
        clampScore(rep);

        if (rep.fairPlayScore < 20.0) {
            emit playerBanned(rep.playerId);
        }

        emit reputationChanged(rep.playerId);
    }
}

// --- Persistence ---

void ReputationSystem::save(const QString& path)
{
    QJsonObject root;
    QJsonArray playersArr;

    for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
        const auto& rep = it.value();
        QJsonObject obj;
        obj["playerId"]       = rep.playerId;
        obj["walletAddress"]  = rep.walletAddress;
        obj["handsPlayed"]    = rep.handsPlayed;
        obj["handsWon"]       = rep.handsWon;
        obj["disconnects"]    = rep.disconnects;
        obj["gamesCompleted"] = rep.gamesCompleted;
        obj["totalWagered"]   = rep.totalWagered;
        obj["totalWon"]       = rep.totalWon;
        obj["fairPlayScore"]  = rep.fairPlayScore;
        obj["firstSeen"]      = rep.firstSeen.toString(Qt::ISODate);
        obj["lastSeen"]       = rep.lastSeen.toString(Qt::ISODate);

        QJsonArray hashes;
        for (const auto& h : rep.gameHashes)
            hashes.append(h);
        obj["gameHashes"] = hashes;

        playersArr.append(obj);
    }

    root["players"] = playersArr;
    root["version"] = 1;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void ReputationSystem::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();
    QJsonArray playersArr = root["players"].toArray();

    m_players.clear();

    for (const auto& val : playersArr) {
        QJsonObject obj = val.toObject();
        PlayerReputation rep;
        rep.playerId       = obj["playerId"].toString();
        rep.walletAddress  = obj["walletAddress"].toString();
        rep.handsPlayed    = obj["handsPlayed"].toInt();
        rep.handsWon       = obj["handsWon"].toInt();
        rep.disconnects    = obj["disconnects"].toInt();
        rep.gamesCompleted = obj["gamesCompleted"].toInt();
        rep.totalWagered   = obj["totalWagered"].toDouble();
        rep.totalWon       = obj["totalWon"].toDouble();
        rep.fairPlayScore  = obj["fairPlayScore"].toDouble(100.0);
        rep.firstSeen      = QDateTime::fromString(obj["firstSeen"].toString(), Qt::ISODate);
        rep.lastSeen       = QDateTime::fromString(obj["lastSeen"].toString(), Qt::ISODate);

        QJsonArray hashes = obj["gameHashes"].toArray();
        for (const auto& h : hashes)
            rep.gameHashes.append(h.toString());

        if (!rep.playerId.isEmpty())
            m_players[rep.playerId] = rep;
    }
}
