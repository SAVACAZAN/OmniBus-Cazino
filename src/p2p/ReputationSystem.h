#pragma once
#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <QDateTime>

/**
 * PlayerReputation — Tracks a player's history and trustworthiness.
 */
struct PlayerReputation {
    QString playerId;
    QString walletAddress;       // OmniBus blockchain address
    int handsPlayed = 0;
    int handsWon = 0;
    int disconnects = 0;         // rage quits
    int gamesCompleted = 0;
    double totalWagered = 0.0;
    double totalWon = 0.0;
    double fairPlayScore = 100.0; // 0-100, starts at 100
    QDateTime firstSeen;
    QDateTime lastSeen;
    QVector<QString> gameHashes;  // hashes of games played (blockchain proof)
};

/**
 * ReputationSystem — On-chain-ready reputation tracking.
 *
 * Tracks: hands played, wins, disconnects, wager amounts.
 * Computes a fairPlayScore: starts at 100, -5 per disconnect,
 * +0.1 per hand played, +0.5 per game completed.
 * Players with score < 20 are banned. Score > 70 = trusted.
 *
 * Persists to JSON file. Ready for blockchain anchoring.
 */
class ReputationSystem : public QObject {
    Q_OBJECT
public:
    explicit ReputationSystem(QObject* parent = nullptr);

    // --- Record events ---
    void recordHandPlayed(const QString& playerId);
    void recordHandWon(const QString& playerId, double amount);
    void recordDisconnect(const QString& playerId);
    void recordGameComplete(const QString& playerId);
    void recordGameHash(const QString& playerId, const QString& hash);
    void recordWager(const QString& playerId, double amount);

    // --- Set wallet address ---
    void setWalletAddress(const QString& playerId, const QString& address);

    // --- Query ---
    PlayerReputation getReputation(const QString& playerId) const;
    double fairPlayScore(const QString& playerId) const;
    bool isTrusted(const QString& playerId) const;   // score > 70
    bool isBanned(const QString& playerId) const;     // score < 20
    QVector<PlayerReputation> leaderboard(int top = 10) const;
    bool hasPlayer(const QString& playerId) const;
    int playerCount() const;

    // --- Persistence ---
    void save(const QString& path = "reputation.json");
    void load(const QString& path = "reputation.json");

    // --- Score recalculation ---
    void updateFairPlayScores();

signals:
    void reputationChanged(const QString& playerId);
    void playerBanned(const QString& playerId);  // score < 20

private:
    void ensurePlayer(const QString& playerId);
    void clampScore(PlayerReputation& rep);

    QMap<QString, PlayerReputation> m_players;
};
