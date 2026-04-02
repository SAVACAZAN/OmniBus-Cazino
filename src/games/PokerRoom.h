#pragma once
#include "ui/GameWidget.h"
#include "utils/CardDeck.h"
#include <QPainter>
#include <QTimer>
#include <QVector>
#include <QPointF>
#include <QMouseEvent>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QDateTime>
#include <QCoreApplication>
#include <QPixmap>
#include <QLineEdit>
#include <cmath>

class PokerClient;
class PokerServer;

// Offline hand history record for provably fair verification
struct OfflineHandHistory {
    int handNumber = 0;
    QString seed;               // SHA-256 random seed used for deck shuffle
    QString seedHash;           // SHA-256 hash of seed (shown during hand)
    QVector<Card> deckOrder;    // Full deck order after seeded shuffle (for verification)
    QVector<Card> playerCards;
    QVector<Card> aiCards;      // cards of AI players (first active AI)
    QVector<Card> community;
    QStringList actions;        // "PREFLOP: Hero CHECK", "FLOP: AI RAISE 40", etc.
    QString result;             // "Hero wins with Two Pair"
    double potAmount = 0;
    QDateTime timestamp;
};

// Player at the table
struct PokerPlayer {
    QString name;
    QString emoji;        // avatar emoji
    double stack = 1000;
    QVector<Card> holeCards;
    bool folded = false;
    bool allIn = false;
    bool isAI = true;
    bool isHero = false;  // the human player
    bool hasBet = false;
    double currentBet = 0;
    QString lastAction;   // "Fold", "Check", "Call 40", "Raise 120", "ALL IN"
    bool seated = true;   // false = empty seat
    int seatIndex = 0;
};

class PokerRoom : public GameWidget {
    Q_OBJECT
public:
    explicit PokerRoom(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

    // Online multiplayer mode
    void setOnlineMode(PokerClient* client);
    void setOfflineMode();
    bool isOnline() const { return m_online; }

    // Multiplayer screen mode (simplified — replaces P2P lobby)
    enum MultiplayerState { MP_NONE, MP_MENU, MP_HOSTING, MP_JOINING, MP_CONNECTED, MP_PLAYING };
    Q_ENUM(MultiplayerState)

    void setMultiplayerMode();   // Switch to MP_MENU
    MultiplayerState mpState() const { return m_mpState; }

    // Provide server and account refs for MP auto-connect
    void setPokerServer(PokerServer* server) { m_pokerServer = server; }
    void setAccountSystem(class AccountSystem* acc) { m_accountSystem = acc; }

private slots:
    // Online mode slots (connected to PokerClient signals)
    void onTableJoined(const QString& tableId, int seat, const QJsonObject& state);
    void onTableStateReceived(const QJsonObject& state);
    void onCardsDealt(const QStringList& cards);
    void onYourTurn(double toCall, double minRaise, double pot);
    void onActionResult(int seat, const QString& action, double amount);
    void onShowdownResult(const QJsonObject& result);
    void onPlayerJoinedTable(int seat, const QString& name, double stack);
    void onPlayerLeftTable(int seat, const QString& name);
    void onClientDisconnected();

    // New online slots for provably fair + seat selection + timer
    void onSeatChosen(int seat, const QString& player);
    void onSeatRejected(int seat, const QString& reason);
    void onHandStart(int handNumber, const QString& serverSeedHash, int dealerSeat);
    void onSeedRequested();
    void onTimerUpdate(int seat, int secondsLeft);
    void onHandComplete(int handNumber, const QString& serverSeed, const QJsonObject& metadata);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    // Drawing
    void drawMultiplayerScreen(QPainter& p);
    void drawTable(QPainter& p);
    void drawPlayer(QPainter& p, const PokerPlayer& player, int seatIdx);
    void drawCard(QPainter& p, const Card& card, QPointF pos, double w, double h, bool faceUp);
    void drawCardBack(QPainter& p, QPointF pos, double w, double h);
    void drawCommunityCards(QPainter& p);
    void drawPot(QPainter& p);
    void drawDealerButton(QPainter& p);
    void drawActionPanel(QPainter& p);
    void drawPlayerBet(QPainter& p, int seatIdx, double amount);
    void drawHandStrength(QPainter& p);
    void drawEmptySeat(QPainter& p, int seatIdx);
    void drawTimerRing(QPainter& p, int seatIdx);
    void drawHandNumberDisplay(QPainter& p);
    void drawProvablyFairBadge(QPainter& p);

    // Seat positions (9 seats around oval table)
    QPointF seatPosition(int seatIdx) const;
    QPointF betChipPosition(int seatIdx) const;
    QPointF cardPosition(int seatIdx, int cardIdx) const;

    // Game logic (offline only)
    void dealPreflop();
    void dealFlop();
    void dealTurn();
    void dealRiver();
    void showdown();
    void nextPhase();
    void aiTurn(int playerIdx);
    void processAIActions();
    void advanceAction();
    int nextActivePlayer(int from) const;
    bool bettingRoundComplete() const;
    void collectBets();
    void awardPot(int winnerIdx);

    // Actions
    void heroFold();
    void heroCheck();
    void heroCall();
    void heroRaise(double amount);
    void heroAllIn();

    // Hand evaluation
    enum HandRank { HighCard, OnePair, TwoPair, ThreeOfAKind, Straight, Flush, FullHouse, FourOfAKind, StraightFlush, RoyalFlush };
    HandRank evaluateHand(const QVector<Card>& cards) const;
    QString rankName(HandRank rank) const;
    int handScore(const QVector<Card>& allCards) const;
    QString describeHeroHand() const;

    // State
    CardDeck m_deck;
    QVector<PokerPlayer> m_players; // 9 seats
    QVector<Card> m_community;
    int m_heroSeat = 0;
    int m_dealerSeat = 3;
    int m_phase = -1; // -1=waiting, 0=preflop, 1=flop, 2=turn, 3=river, 4=showdown
    double m_pot = 0;
    double m_smallBlind = 10;
    double m_bigBlind = 20;
    double m_currentBet = 0;  // current bet to match
    int m_activePlayerIdx = -1;
    bool m_heroTurn = false;
    QString m_resultText;
    QColor m_resultColor;
    QTimer* m_aiTimer;
    QTimer* m_dealTimer;
    bool m_gameActive = false;
    int m_actionCount = 0; // how many players have acted this round
    double m_lastRaiseAmount = 0; // tracks the last raise increment for min-raise calculation

    // Hero turn timer (offline mode)
    QTimer* m_heroTimer = nullptr;
    int m_heroTimerSeconds = 0;    // countdown value

    // Offline hand history / provably fair
    int m_offlineHandNumber = 0;
    QString m_offlineSeed;           // seed used for current hand
    QString m_offlineSeedHash;       // SHA-256 hash of seed
    QVector<Card> m_offlineDeckOrder; // full deck snapshot after seeded shuffle
    OfflineHandHistory m_currentHandHistory;
    QVector<OfflineHandHistory> m_handHistories; // all past hands this session
    bool m_showOfflineSeed = false;  // show seed after showdown
    bool m_heroOutOfChips = false;   // true when hero stack <= 0

    // Buy-in screen
    bool m_needBuyIn = true;         // show buy-in screen before playing
    double m_buyInAmount = 1000.0;   // selected buy-in
    double m_minBuyIn = 100.0;
    double m_maxBuyIn = 10000.0;
    QRectF m_buyInSliderRect;
    QRectF m_buyInPlayBtnRect;
    QRectF m_buyInMinRect, m_buyInMaxRect;

    // Side pots
    struct SidePot {
        double amount = 0;
        QVector<int> eligible; // player indices eligible for this pot
    };
    QVector<SidePot> m_sidePots;
    void calculateSidePots();
    void awardSidePots();

    // History saving
    void logAction(const QString& action);
    void saveHandHistory();
    QString generateSeed();

    // UI state
    QPoint m_mousePos;
    QRectF m_foldBtnRect, m_checkBtnRect, m_callBtnRect, m_raiseBtnRect, m_allInBtnRect;
    QRectF m_halfPotRect, m_threeFourPotRect, m_potBtnRect, m_twoPotRect;
    QRectF m_sliderRect;
    QRectF m_dealBtnRect;
    double m_raiseAmount = 0;
    double m_minRaise = 0;
    bool m_showActions = false;
    double m_sliderValue = 0.0; // 0..1
    bool m_draggingSlider = false;

    // Table geometry
    QRectF m_tableRect;
    double m_tableWidth = 0, m_tableHeight = 0;
    QPointF m_tableCenter;

    // Online multiplayer mode
    bool m_online = false;
    PokerClient* m_client = nullptr;
    int m_mySeat = -1;
    QString m_waitingText;  // "Waiting for players..." etc.

    // Online mode: timer display
    int m_timerSeat = -1;
    int m_timerSecondsLeft = 0;

    // Online mode: hand number & provably fair
    int m_currentHandNumber = 0;
    QString m_serverSeedHash;       // commitment hash shown during hand
    QString m_revealedServerSeed;   // revealed after hand ends
    bool m_handVerified = false;    // true if we verified the seed matches hash
    int m_dealerSeatOnline = -1;

    // Seat selection state (cached for hit-testing)
    QVector<QRectF> m_seatHitRects; // click areas for empty seats
    double m_pendingBuyIn = 0;      // buy-in to use when choosing seat
    int m_pendingSeatRequest = -1;  // seat we requested (to match seat_chosen response)

    // Helpers for online mode
    static Card cardFromString(const QString& s);
    static QString phaseFromInt(int phase);
    int phaseFromString(const QString& s) const;
    void drawWaitingOverlay(QPainter& p);

    // ─── Multiplayer screen (simplified, replaces P2P lobby) ─────────
    MultiplayerState m_mpState = MP_NONE;
    QString m_gameCode;
    QLineEdit* m_joinCodeInput = nullptr;
    QRectF m_hostBtnRect, m_joinBtnRect, m_joinConnectBtnRect, m_mpBackBtnRect;
    PokerServer* m_pokerServer = nullptr;
    AccountSystem* m_accountSystem = nullptr;
    void mpHostGame();
    void mpJoinGame(const QString& codeOrIp);

    // ─── Card image assets ───────────────────────────────────────────
    QMap<QString, QPixmap> m_cardImages;
    QPixmap m_cardBackImage;
    void loadCardImages();
    QString cardImageKey(const Card& card) const;
};
