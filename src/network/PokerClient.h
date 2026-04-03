#pragma once
#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>

class PokerClient : public QObject {
    Q_OBJECT
public:
    explicit PokerClient(QObject* parent = nullptr);
    ~PokerClient();

    void connectToServer(const QString& url = "ws://193.219.97.147:9999");
    void disconnect();
    bool isConnected() const;

    // Actions
    void login(const QString& name, double balance);
    void requestTableList();
    void createTable(const QString& name, double sb, double bb, double minBuy, double maxBuy, int seats = 9);
    void joinTable(const QString& tableId, double buyIn);
    void leaveTable();
    void sendAction(const QString& action, double amount = 0);
    void sendChat(const QString& message);
    void addFunds(double amount);

    // New actions for seat selection & provably fair
    void chooseSeat(int seat, double buyIn = 0);
    void sendClientSeed(const QString& seed);
    void verifyHand(int handNumber);

    // State accessors
    QString playerId() const { return m_playerId; }
    double balance() const { return m_balance; }
    bool isLoggedIn() const { return m_loggedIn; }
    QString currentTableId() const { return m_currentTableId; }

signals:
    void connected();
    void disconnected();
    void loginOk(const QString& playerId, double balance);
    void tableListReceived(const QJsonArray& tables);
    void tableJoined(const QString& tableId, int seat, const QJsonObject& state);
    void tableStateUpdated(const QJsonObject& state);
    void cardsDealt(const QStringList& cards);
    void yourTurn(double toCall, double minRaise, double pot);
    void actionResult(int seat, const QString& action, double amount);
    void showdownResult(const QJsonObject& result);
    void chatReceived(const QString& from, const QString& message);
    void playerJoinedTable(int seat, const QString& name, double stack);
    void playerLeftTable(int seat, const QString& name);
    void errorReceived(const QString& message);
    void balanceUpdated(double balance);

    // New signals for seat selection, provably fair, and timer
    void seatChosenResult(int seat, const QString& player);
    void seatRejectedResult(int seat, const QString& reason);
    void handStartReceived(int handNumber, const QString& serverSeedHash, int dealerSeat);
    void seedRequested();
    void timerUpdated(int seat, int secondsLeft);
    void handCompleted(int handNumber, const QString& serverSeed, const QJsonObject& metadata);
    void handHistoryReceived(int handNumber, const QJsonObject& data);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessage(const QString& message);

private:
    QWebSocket* m_socket;
    QString m_playerId;
    double m_balance = 0;
    bool m_loggedIn = false;
    QString m_currentTableId;
};
