#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QTimer>
#include <QUdpSocket>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>

/**
 * P2PGame — Metadata for a discoverable P2P poker game.
 */
struct P2PGame {
    QString gameCode;       // "OMNI-X4K9" or encoded IP
    QString hostName;       // "CryptoKing"
    QString hostIp;
    quint16 hostPort = 9999;
    double smallBlind = 1.0;
    double bigBlind = 2.0;
    int maxSeats = 9;
    int currentPlayers = 0;
    QString currency;       // "EUR", "BTC", "OMNI"
    QDateTime created;
    double minBuyIn = 100.0;
    double maxBuyIn = 1000.0;

    QJsonObject toJson() const;
    static P2PGame fromJson(const QJsonObject& obj);
};

/**
 * P2PDiscovery — Host, discover, and join P2P poker games.
 *
 * Hosting: Starts a QWebSocketServer, broadcasts presence via UDP on LAN.
 * Joining: By game code (decoded to IP:port), direct IP, or LAN scan.
 * LAN discovery uses UDP broadcast on port 9998.
 */
class P2PDiscovery : public QObject {
    Q_OBJECT
public:
    explicit P2PDiscovery(QObject* parent = nullptr);
    ~P2PDiscovery();

    /// Host a game: start WebSocket server, begin UDP broadcasting
    /// Returns the generated game code
    QString hostGame(const P2PGame& config);

    /// Stop hosting and close all connections
    void stopHosting();

    /// Whether we are currently hosting a game
    bool isHosting() const;

    /// Join a game by its OMNI-code (decodes to IP:port)
    void joinByCode(const QString& gameCode);

    /// Join by direct IP address and port
    void joinByIP(const QString& ip, quint16 port);

    /// Broadcast a UDP scan request on LAN to find games
    void scanLAN();

    /// Get list of discovered games (from LAN scan or manual add)
    QVector<P2PGame> availableGames() const;

    /// Get hosted game info
    P2PGame hostedGame() const;

    /// Get connected peer sockets (for host)
    QVector<QWebSocket*> connectedPeers() const;

    /// Send message to all connected peers (host mode)
    void broadcastMessage(const QJsonObject& msg);

    /// Send message to specific peer
    void sendToPeer(QWebSocket* peer, const QJsonObject& msg);

signals:
    void gameHosted(const QString& gameCode);
    void gameFound(const P2PGame& game);
    void lanScanComplete(const QVector<P2PGame>& games);
    void connectionEstablished(const QString& gameCode);
    void connectionFailed(const QString& reason);
    void playerJoined(const QString& playerName);
    void playerLeft(const QString& playerName);
    void messageReceived(QWebSocket* sender, const QJsonObject& msg);

private slots:
    void onNewConnection();
    void onPeerDisconnected();
    void onPeerMessage(const QString& message);
    void onClientConnected();
    void onClientDisconnected();
    void onClientMessage(const QString& message);

private:
    void broadcastPresence();
    void onUdpReadyRead();
    void setupUdpSocket();

    // Server (host mode)
    QWebSocketServer* m_server = nullptr;
    QVector<QWebSocket*> m_peers;

    // Client (join mode)
    QWebSocket* m_clientSocket = nullptr;

    // UDP discovery
    QUdpSocket* m_udpSocket = nullptr;
    QTimer* m_broadcastTimer = nullptr;

    // State
    QVector<P2PGame> m_discoveredGames;
    P2PGame m_hostedGame;
    bool m_hosting = false;
    quint16 m_port = 9999;

    static constexpr quint16 UDP_DISCOVERY_PORT = 9998;
    static constexpr int BROADCAST_INTERVAL_MS = 3000;
    static const QByteArray DISCOVER_MAGIC;
    static const QByteArray ANNOUNCE_MAGIC;
};
