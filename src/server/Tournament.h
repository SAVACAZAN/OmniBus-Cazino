#pragma once
#include <QObject>
#include <QVector>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QDateTime>
#include <QPair>
#include <QString>
#include <algorithm>

// ─── Enums ──────────────────────────────────────────────────────────────────────

enum class TournamentType { SitAndGo, MTT };
enum class TournamentStatus { Registering, Running, FinalTable, Finished };

// ─── Structs ────────────────────────────────────────────────────────────────────

struct TournamentConfig {
    QString name;
    TournamentType type = TournamentType::SitAndGo;
    int maxPlayers = 9;
    double buyIn = 500;
    double startingStack = 5000;
    int blindLevelMinutes = 5;
    QVector<QPair<double, double>> blindSchedule;  // {smallBlind, bigBlind} per level
    QVector<double> prizeStructure;                 // percentage per place
    bool allowLateReg = false;
    int lateRegLevels = 3;
    bool allowRebuys = false;
    int rebuyLevels = 4;
    double rebuyAmount = 0;

    // Fill defaults if empty
    void ensureDefaults() {
        if (blindSchedule.isEmpty()) {
            blindSchedule = {
                {10, 20}, {15, 30}, {25, 50}, {50, 100}, {75, 150},
                {100, 200}, {150, 300}, {200, 400}, {300, 600}, {500, 1000}
            };
        }
        if (prizeStructure.isEmpty()) {
            if (type == TournamentType::SitAndGo) {
                // SNG: top 3
                prizeStructure = {50.0, 30.0, 20.0};
            } else {
                // MTT: top 9
                prizeStructure = {30.0, 20.0, 15.0, 10.0, 7.5, 7.5, 5.0, 5.0, 5.0};
            }
        }
        if (rebuyAmount <= 0) {
            rebuyAmount = buyIn;
        }
    }

    QJsonObject toJson() const {
        QJsonObject o;
        o["name"] = name;
        o["type"] = (type == TournamentType::SitAndGo) ? "SitAndGo" : "MTT";
        o["maxPlayers"] = maxPlayers;
        o["buyIn"] = buyIn;
        o["startingStack"] = startingStack;
        o["blindLevelMinutes"] = blindLevelMinutes;
        o["allowLateReg"] = allowLateReg;
        o["lateRegLevels"] = lateRegLevels;
        o["allowRebuys"] = allowRebuys;
        o["rebuyLevels"] = rebuyLevels;
        o["rebuyAmount"] = rebuyAmount;
        QJsonArray bs;
        for (const auto& p : blindSchedule) {
            QJsonObject lvl;
            lvl["sb"] = p.first;
            lvl["bb"] = p.second;
            bs.append(lvl);
        }
        o["blindSchedule"] = bs;
        QJsonArray ps;
        for (double d : prizeStructure) ps.append(d);
        o["prizeStructure"] = ps;
        return o;
    }
};

struct TournamentPlayer {
    QString playerId;
    QString name;
    double chips = 0;
    int tableId = -1;           // which table index (-1 if eliminated)
    int finishPosition = 0;     // 0 = still playing
    double prizeWon = 0;
    bool eliminated = false;
    QDateTime registeredAt;
    QDateTime eliminatedAt;
    int rebuysUsed = 0;

    QJsonObject toJson() const {
        QJsonObject o;
        o["playerId"] = playerId;
        o["name"] = name;
        o["chips"] = chips;
        o["tableId"] = tableId;
        o["finishPosition"] = finishPosition;
        o["prizeWon"] = prizeWon;
        o["eliminated"] = eliminated;
        o["rebuysUsed"] = rebuysUsed;
        if (registeredAt.isValid())
            o["registeredAt"] = registeredAt.toString(Qt::ISODate);
        if (eliminatedAt.isValid())
            o["eliminatedAt"] = eliminatedAt.toString(Qt::ISODate);
        return o;
    }
};

// ─── Tournament Class ───────────────────────────────────────────────────────────

class Tournament : public QObject {
    Q_OBJECT
public:
    explicit Tournament(const QString& id, const TournamentConfig& config, QObject* parent = nullptr);
    ~Tournament() override;

    // Registration
    bool registerPlayer(const QString& playerId, const QString& name);
    void unregisterPlayer(const QString& playerId);
    int registeredCount() const;
    bool isFull() const;

    // Tournament lifecycle
    void start();
    void onHandComplete(int tableIndex, const QString& winnerId);
    void onPlayerEliminated(const QString& playerId, int tableIndex);
    void onPlayerBusted(const QString& playerId);

    // Blind management
    void advanceBlinds();
    int currentBlindLevel() const;
    QPair<double, double> currentBlinds() const;
    int minutesUntilNextLevel() const;
    int secondsUntilNextLevel() const;

    // Table management (for MTT)
    void balanceTables();
    void mergeTablesIfNeeded();

    // Rebuy
    bool canRebuy(const QString& playerId) const;
    bool processRebuy(const QString& playerId);

    // Status
    TournamentStatus status() const { return m_status; }
    QString id() const { return m_id; }
    TournamentConfig config() const { return m_config; }
    QVector<TournamentPlayer> players() const { return m_players; }
    QVector<TournamentPlayer> leaderboard() const;
    int playersRemaining() const;
    int tablesActive() const;
    double totalPrizePool() const { return m_prizePool; }
    QVector<QPair<int, double>> prizeTable() const;

    // Find specific player
    TournamentPlayer* findPlayer(const QString& playerId);
    const TournamentPlayer* findPlayer(const QString& playerId) const;

    // Serialization
    QJsonObject toJson() const;

signals:
    void tournamentStarted(const QString& tournamentId);
    void blindsIncreased(int level, double sb, double bb);
    void playerEliminated(const QString& playerId, int position, double prize);
    void playerRebuyed(const QString& playerId);
    void tablesRebalanced();
    void tournamentFinished(const QString& tournamentId, const QVector<TournamentPlayer>& results);
    void statusChanged(TournamentStatus status);

private:
    void calculatePrizes();
    void checkForWinner();
    void setStatus(TournamentStatus s);
    int assignTableForPlayer();

    QString m_id;
    TournamentConfig m_config;
    TournamentStatus m_status = TournamentStatus::Registering;
    QVector<TournamentPlayer> m_players;
    int m_currentBlindLevel = 0;
    QTimer* m_blindTimer = nullptr;
    QDateTime m_startTime;
    QDateTime m_lastBlindAdvance;
    double m_prizePool = 0;
    int m_nextTableId = 0;
    int m_numTables = 0;
    int m_eliminationOrder = 0; // counts up as players are eliminated
};
