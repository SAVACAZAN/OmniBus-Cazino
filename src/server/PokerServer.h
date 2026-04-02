#pragma once
#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QSslConfiguration>
#include <QMap>
#include "PokerTable.h"
#include "Tournament.h"

struct ConnectedPlayer {
    QWebSocket* socket = nullptr;
    QString playerId;
    QString name;
    double balance = 0;
    QString currentTableId;
    QString currentTournamentId;  // tournament the player is registered in
};

class PokerServer : public QObject {
    Q_OBJECT
public:
    explicit PokerServer(quint16 port = 9999, QObject* parent = nullptr);
    ~PokerServer();

    bool start();
    bool startSecure(const QString& certPath, const QString& keyPath);  // WSS with SSL
    void stop();
    bool isRunning() const;
    int playerCount() const;
    int tableCount() const;

    // Tournament management
    Tournament* createTournament(const TournamentConfig& config);
    bool registerForTournament(const QString& playerId, const QString& tournamentId);
    bool unregisterFromTournament(const QString& playerId, const QString& tournamentId);
    QVector<Tournament*> getTournaments() const;
    Tournament* findTournament(const QString& tournamentId) const;
    int tournamentCount() const;

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void playerConnected(const QString& name);
    void playerDisconnected(const QString& name);
    void logMessage(const QString& msg);
    void tournamentCreated(Tournament* tournament);

private slots:
    void onNewConnection();
    void onTextMessage(const QString& message);
    void onDisconnected();

private:
    void handleLogin(QWebSocket* socket, const QJsonObject& msg);
    void handleListTables(QWebSocket* socket);
    void handleCreateTable(QWebSocket* socket, const QJsonObject& msg);
    void handleJoinTable(QWebSocket* socket, const QJsonObject& msg);
    void handleLeaveTable(QWebSocket* socket);
    void handleAction(QWebSocket* socket, const QJsonObject& msg);
    void handleChat(QWebSocket* socket, const QJsonObject& msg);
    void handleAddFunds(QWebSocket* socket, const QJsonObject& msg);
    void handleChooseSeat(QWebSocket* socket, const QJsonObject& msg);
    void handleClientSeed(QWebSocket* socket, const QJsonObject& msg);
    void handleVerifyHand(QWebSocket* socket, const QJsonObject& msg);

    // Tournament message handlers
    void handleCreateTournament(QWebSocket* socket, const QJsonObject& msg);
    void handleRegisterTournament(QWebSocket* socket, const QJsonObject& msg);
    void handleUnregisterTournament(QWebSocket* socket, const QJsonObject& msg);
    void handleListTournaments(QWebSocket* socket);

    void broadcastToTable(const QString& tableId, const QJsonObject& msg, QWebSocket* except = nullptr);
    void broadcastToAll(const QJsonObject& msg, QWebSocket* except = nullptr);
    void sendTo(QWebSocket* socket, const QJsonObject& msg);
    ConnectedPlayer* findPlayer(QWebSocket* socket);
    ConnectedPlayer* findPlayerById(const QString& id);

    void connectTableSignals(PokerTable* table);
    void connectTournamentSignals(Tournament* t);
    void createPresetTournaments();

    QWebSocketServer* m_server;
    QMap<QWebSocket*, ConnectedPlayer> m_players;
    QMap<QString, PokerTable*> m_tables;
    QMap<QString, Tournament*> m_tournaments;
    quint16 m_port;
    int m_nextPlayerId = 1;
    int m_nextTableId = 1;
    int m_nextTournamentId = 1;
};
