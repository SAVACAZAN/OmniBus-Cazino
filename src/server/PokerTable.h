#pragma once
#include <QObject>
#include <QVector>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QDateTime>
#include <QFile>
#include "utils/CardDeck.h"

struct ServerPlayer {
    QString id;
    QString name;
    double stack = 0;
    QVector<Card> holeCards;
    bool folded = false;
    bool allIn = false;
    bool seated = false;
    double currentBet = 0;
    QString lastAction;
    int seatIndex = -1;
    bool connected = true;
    bool hasBet = false;
    double totalBetThisHand = 0;  // total chips invested this hand (for side pots)
};

// Side pot structure for all-in side pot calculation
struct SidePot {
    double amount = 0;
    QVector<int> eligibleSeats;  // seats eligible to win this pot
};

// Provably fair hand metadata
struct HandMetadata {
    int handNumber = 0;
    QString serverSeed;         // random 256-bit hex, hidden until hand ends
    QString serverSeedHash;     // SHA256(serverSeed), shared with all players at deal
    QStringList clientSeeds;    // each player contributes a seed
    QString combinedSeed;       // SHA256(serverSeed + client1 + client2 + ...)
    QDateTime startTime;
    QDateTime endTime;
    QVector<QString> actionLog; // "14:32:05.123 | Hand #247 | Seat 2 (CryptoKing) | RAISE 120 | Pot: 340 | Stack: 8080"
    QJsonObject finalState;     // complete state at showdown
    QString resultHash;         // SHA256 of everything for verification
};

class PokerTable : public QObject {
    Q_OBJECT
public:
    explicit PokerTable(const QString& id, const QString& name,
                        double smallBlind, double bigBlind,
                        double minBuyIn, double maxBuyIn,
                        int maxSeats = 9, QObject* parent = nullptr);

    // Player management
    int addPlayer(const QString& playerId, const QString& name, double buyIn);
    int chooseSeat(const QString& playerId, const QString& name, double buyIn, int requestedSeat);
    void removePlayer(const QString& playerId);
    int playerCount() const;
    int seatedCount() const;

    // Client seed collection
    void submitClientSeed(const QString& playerId, const QString& seed);

    // Game flow
    void tryStartHand();
    void handleAction(const QString& playerId, const QString& action, double amount);

    // State
    QJsonObject tableInfo() const;
    QJsonObject fullState(const QString& forPlayerId = "") const;

    // Hand history / verification
    QJsonObject getHandHistory(int handNumber) const;
    HandMetadata currentHandMetadata() const { return m_handMeta; }

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    bool isHandActive() const { return m_handActive; }
    double smallBlind() const { return m_smallBlind; }
    double bigBlind() const { return m_bigBlind; }
    double minBuyIn() const { return m_minBuyIn; }
    double maxBuyIn() const { return m_maxBuyIn; }
    int maxSeats() const { return m_maxSeats; }

    // Find player by id
    ServerPlayer* findPlayer(const QString& playerId);
    QString activePlayerId() const;
    int activePlayerSeat() const { return m_activePlayerSeat; }

    // Timer info
    int secondsLeft() const;

signals:
    void stateChanged(const QString& tableId);
    void handStarted(const QString& tableId);
    void playerActed(const QString& tableId, int seat, const QString& action, double amount);
    void showdownResult(const QString& tableId, const QJsonObject& result);
    void playerJoinedTable(const QString& tableId, int seat, const QString& name, double stack);
    void playerLeftTable(const QString& tableId, int seat, const QString& name);
    void chatMessage(const QString& tableId, const QString& from, const QString& message);
    void turnChanged(const QString& tableId, const QString& playerId, double toCall, double minRaise);
    void dealCards(const QString& tableId, const QString& playerId, const QVector<Card>& cards);

    // New signals for provably fair + seat selection + timer
    void seatChosen(const QString& tableId, int seat, const QString& playerName);
    void seatRejected(const QString& tableId, int seat, const QString& reason);
    void handStartBroadcast(const QString& tableId, int handNumber, const QString& serverSeedHash, int dealerSeat);
    void seedRequest(const QString& tableId);
    void timerUpdate(const QString& tableId, int seat, int secondsLeft);
    void handComplete(const QString& tableId, int handNumber, const QString& serverSeed, const QJsonObject& metadata);

private:
    void dealPreflop();
    void dealFlop();
    void dealTurn();
    void dealRiver();
    void showdown();
    void nextPhase();
    void advanceToNextPlayer();
    int nextActivePlayer(int from) const;
    int firstPostFlopPlayer() const;
    void collectBets();
    void awardPot();
    void resetHand();
    bool isBettingComplete() const;
    void notifyTurn();
    void autoFoldTimeout();
    void onTimerTick();

    // Provably fair
    void generateServerSeed();
    void computeCombinedSeed();
    void logAction(int seat, const QString& action, double amount);
    void saveHandHistory();
    void finalizeHandMetadata(const QJsonObject& showdownResult);

    // Side pot calculation
    QVector<SidePot> calculateSidePots() const;

    // Hand evaluation
    enum HandRank { HighCard, OnePair, TwoPair, ThreeOfAKind, Straight, Flush, FullHouse, FourOfAKind, StraightFlush, RoyalFlush };
    HandRank evaluateHand(const QVector<Card>& cards) const;
    int handScore(const QVector<Card>& allCards) const;
    QString rankName(HandRank rank) const;
    QVector<Card> bestFiveCards(const QVector<Card>& allCards) const;

    QString m_id, m_name;
    double m_smallBlind, m_bigBlind, m_minBuyIn, m_maxBuyIn;
    int m_maxSeats;

    QVector<ServerPlayer> m_seats;
    QVector<Card> m_community;
    CardDeck m_deck;

    int m_dealerSeat = -1;
    int m_activePlayerSeat = -1;
    int m_phase = -1; // -1=waiting, 0=preflop, 1=flop, 2=turn, 3=river
    double m_pot = 0;
    double m_currentBet = 0;
    double m_lastRaiseSize = 0;   // for proper min raise tracking
    bool m_handActive = false;
    int m_handNumber = 0;
    int m_actionCount = 0;

    // Timer system
    QTimer* m_actionTimer;        // 30s per action, fires autoFold
    QTimer* m_countdownTimer;     // 1s tick for broadcasting countdown
    QDateTime m_actionDeadline;   // when current player's turn expires
    static constexpr int ACTION_TIMEOUT_SECS = 30;

    // Provably fair metadata
    HandMetadata m_handMeta;
    QMap<QString, QString> m_pendingClientSeeds;  // playerId -> seed
    bool m_waitingForSeeds = false;
    int m_expectedSeedCount = 0;

    // Hand history archive (last N hands)
    QVector<HandMetadata> m_handHistory;
    static constexpr int MAX_HAND_HISTORY = 100;
};
