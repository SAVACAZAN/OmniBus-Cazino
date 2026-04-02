#include "P2PDiscovery.h"
#include "GameCode.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QHostAddress>

const QByteArray P2PDiscovery::DISCOVER_MAGIC = QByteArrayLiteral("OMNIBUS_POKER_DISCOVER");
const QByteArray P2PDiscovery::ANNOUNCE_MAGIC = QByteArrayLiteral("OMNIBUS_POKER_ANNOUNCE");

// --- P2PGame JSON serialization ---

QJsonObject P2PGame::toJson() const
{
    QJsonObject obj;
    obj["gameCode"]       = gameCode;
    obj["hostName"]       = hostName;
    obj["hostIp"]         = hostIp;
    obj["hostPort"]       = static_cast<int>(hostPort);
    obj["smallBlind"]     = smallBlind;
    obj["bigBlind"]       = bigBlind;
    obj["maxSeats"]       = maxSeats;
    obj["currentPlayers"] = currentPlayers;
    obj["currency"]       = currency;
    obj["created"]        = created.toString(Qt::ISODate);
    obj["minBuyIn"]       = minBuyIn;
    obj["maxBuyIn"]       = maxBuyIn;
    return obj;
}

P2PGame P2PGame::fromJson(const QJsonObject& obj)
{
    P2PGame g;
    g.gameCode       = obj["gameCode"].toString();
    g.hostName       = obj["hostName"].toString();
    g.hostIp         = obj["hostIp"].toString();
    g.hostPort       = static_cast<quint16>(obj["hostPort"].toInt(9999));
    g.smallBlind     = obj["smallBlind"].toDouble(1.0);
    g.bigBlind       = obj["bigBlind"].toDouble(2.0);
    g.maxSeats       = obj["maxSeats"].toInt(9);
    g.currentPlayers = obj["currentPlayers"].toInt(0);
    g.currency       = obj["currency"].toString("EUR");
    g.created        = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
    g.minBuyIn       = obj["minBuyIn"].toDouble(100.0);
    g.maxBuyIn       = obj["maxBuyIn"].toDouble(1000.0);
    return g;
}

// --- P2PDiscovery ---

P2PDiscovery::P2PDiscovery(QObject* parent)
    : QObject(parent)
{
    setupUdpSocket();
}

P2PDiscovery::~P2PDiscovery()
{
    stopHosting();

    if (m_clientSocket) {
        m_clientSocket->close();
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }

    if (m_udpSocket) {
        m_udpSocket->close();
        m_udpSocket->deleteLater();
        m_udpSocket = nullptr;
    }
}

void P2PDiscovery::setupUdpSocket()
{
    m_udpSocket = new QUdpSocket(this);
    m_udpSocket->bind(QHostAddress::AnyIPv4, UDP_DISCOVERY_PORT,
                      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(m_udpSocket, &QUdpSocket::readyRead,
            this, &P2PDiscovery::onUdpReadyRead);
}

// --- Hosting ---

QString P2PDiscovery::hostGame(const P2PGame& config)
{
    if (m_hosting)
        stopHosting();

    m_hostedGame = config;
    m_hostedGame.created = QDateTime::currentDateTimeUtc();

    // Determine our LAN IP
    QString localIp;
    const auto interfaces = QNetworkInterface::allAddresses();
    for (const QHostAddress& addr : interfaces) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol &&
            !addr.isLoopback() &&
            addr.toString().startsWith("192.168")) {
            localIp = addr.toString();
            break;
        }
    }
    // Fallback: try any non-loopback IPv4
    if (localIp.isEmpty()) {
        for (const QHostAddress& addr : interfaces) {
            if (addr.protocol() == QAbstractSocket::IPv4Protocol &&
                !addr.isLoopback()) {
                localIp = addr.toString();
                break;
            }
        }
    }
    if (localIp.isEmpty())
        localIp = QStringLiteral("127.0.0.1");

    m_hostedGame.hostIp = localIp;
    m_hostedGame.hostPort = m_port;

    // Generate game code from IP:port
    m_hostedGame.gameCode = GameCode::encode(localIp, m_port);
    if (m_hostedGame.gameCode.isEmpty())
        m_hostedGame.gameCode = GameCode::generate();

    // Start WebSocket server
    m_server = new QWebSocketServer(
        QStringLiteral("OmniBus P2P Poker"),
        QWebSocketServer::NonSecureMode, this);

    if (!m_server->listen(QHostAddress::Any, m_port)) {
        // Try next port if busy
        for (quint16 p = m_port + 1; p < m_port + 10; ++p) {
            if (m_server->listen(QHostAddress::Any, p)) {
                m_port = p;
                m_hostedGame.hostPort = p;
                m_hostedGame.gameCode = GameCode::encode(localIp, p);
                break;
            }
        }
        if (!m_server->isListening()) {
            emit connectionFailed("Could not start server on any port");
            delete m_server;
            m_server = nullptr;
            return QString();
        }
    }

    connect(m_server, &QWebSocketServer::newConnection,
            this, &P2PDiscovery::onNewConnection);

    m_hosting = true;

    // Start broadcasting presence on LAN
    m_broadcastTimer = new QTimer(this);
    connect(m_broadcastTimer, &QTimer::timeout,
            this, &P2PDiscovery::broadcastPresence);
    m_broadcastTimer->start(BROADCAST_INTERVAL_MS);

    // Broadcast immediately
    broadcastPresence();

    emit gameHosted(m_hostedGame.gameCode);
    return m_hostedGame.gameCode;
}

void P2PDiscovery::stopHosting()
{
    if (m_broadcastTimer) {
        m_broadcastTimer->stop();
        m_broadcastTimer->deleteLater();
        m_broadcastTimer = nullptr;
    }

    // Close all peer connections
    for (QWebSocket* peer : m_peers) {
        peer->close();
        peer->deleteLater();
    }
    m_peers.clear();

    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }

    m_hosting = false;
}

bool P2PDiscovery::isHosting() const
{
    return m_hosting;
}

P2PGame P2PDiscovery::hostedGame() const
{
    return m_hostedGame;
}

QVector<QWebSocket*> P2PDiscovery::connectedPeers() const
{
    return m_peers;
}

// --- Server connection handling ---

void P2PDiscovery::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QWebSocket* peer = m_server->nextPendingConnection();
        if (!peer) continue;

        m_peers.append(peer);
        m_hostedGame.currentPlayers = m_peers.size() + 1;  // +1 for host

        connect(peer, &QWebSocket::textMessageReceived,
                this, &P2PDiscovery::onPeerMessage);
        connect(peer, &QWebSocket::disconnected,
                this, &P2PDiscovery::onPeerDisconnected);

        // Send welcome message with game info
        QJsonObject welcome;
        welcome["type"] = QStringLiteral("welcome");
        welcome["game"] = m_hostedGame.toJson();
        peer->sendTextMessage(
            QString::fromUtf8(QJsonDocument(welcome).toJson(QJsonDocument::Compact)));

        emit playerJoined(peer->peerAddress().toString());
    }
}

void P2PDiscovery::onPeerDisconnected()
{
    auto* peer = qobject_cast<QWebSocket*>(sender());
    if (!peer) return;

    m_peers.removeAll(peer);
    m_hostedGame.currentPlayers = m_peers.size() + 1;

    emit playerLeft(peer->peerAddress().toString());
    peer->deleteLater();
}

void P2PDiscovery::onPeerMessage(const QString& message)
{
    auto* peer = qobject_cast<QWebSocket*>(sender());
    if (!peer) return;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
        emit messageReceived(peer, doc.object());
    }
}

// --- Client (join mode) ---

void P2PDiscovery::joinByCode(const QString& gameCode)
{
    if (!GameCode::isValid(gameCode)) {
        emit connectionFailed("Invalid game code format");
        return;
    }

    auto [ip, port] = GameCode::decode(gameCode);
    if (ip.isEmpty() || port == 0) {
        emit connectionFailed("Could not decode game code");
        return;
    }

    joinByIP(ip, port);
}

void P2PDiscovery::joinByIP(const QString& ip, quint16 port)
{
    if (m_clientSocket) {
        m_clientSocket->close();
        m_clientSocket->deleteLater();
    }

    m_clientSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_clientSocket, &QWebSocket::connected,
            this, &P2PDiscovery::onClientConnected);
    connect(m_clientSocket, &QWebSocket::disconnected,
            this, &P2PDiscovery::onClientDisconnected);
    connect(m_clientSocket, &QWebSocket::textMessageReceived,
            this, &P2PDiscovery::onClientMessage);

    QUrl url;
    url.setScheme(QStringLiteral("ws"));
    url.setHost(ip);
    url.setPort(port);
    url.setPath(QStringLiteral("/"));

    m_clientSocket->open(url);
}

void P2PDiscovery::onClientConnected()
{
    emit connectionEstablished(m_hostedGame.gameCode);
}

void P2PDiscovery::onClientDisconnected()
{
    emit playerLeft("host");
}

void P2PDiscovery::onClientMessage(const QString& message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();

        if (type == QStringLiteral("welcome")) {
            // Got game info from host
            P2PGame game = P2PGame::fromJson(obj["game"].toObject());
            emit connectionEstablished(game.gameCode);
        }

        emit messageReceived(m_clientSocket, obj);
    }
}

// --- UDP LAN Discovery ---

void P2PDiscovery::broadcastPresence()
{
    if (!m_hosting || !m_udpSocket)
        return;

    QJsonObject announce;
    announce["magic"] = QString::fromLatin1(ANNOUNCE_MAGIC);
    announce["game"] = m_hostedGame.toJson();

    QByteArray data = QJsonDocument(announce).toJson(QJsonDocument::Compact);

    // Broadcast to all network interfaces
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::CanBroadcast))
            continue;

        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            QHostAddress broadcast = entry.broadcast();
            if (!broadcast.isNull()) {
                m_udpSocket->writeDatagram(data, broadcast, UDP_DISCOVERY_PORT);
            }
        }
    }

    // Also broadcast to 255.255.255.255 as fallback
    m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, UDP_DISCOVERY_PORT);
}

void P2PDiscovery::scanLAN()
{
    if (!m_udpSocket)
        return;

    m_discoveredGames.clear();

    QByteArray data = DISCOVER_MAGIC;

    // Broadcast discovery request
    m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, UDP_DISCOVERY_PORT);

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::CanBroadcast))
            continue;

        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            QHostAddress broadcast = entry.broadcast();
            if (!broadcast.isNull()) {
                m_udpSocket->writeDatagram(data, broadcast, UDP_DISCOVERY_PORT);
            }
        }
    }

    // Emit results after a short delay to collect responses
    QTimer::singleShot(2000, this, [this]() {
        emit lanScanComplete(m_discoveredGames);
    });
}

void P2PDiscovery::onUdpReadyRead()
{
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_udpSocket->pendingDatagramSize()));
        QHostAddress senderAddr;
        quint16 senderPort = 0;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(),
                                   &senderAddr, &senderPort);

        // Check if this is a discovery request (and we're hosting)
        if (datagram == DISCOVER_MAGIC && m_hosting) {
            // Respond with our game info
            broadcastPresence();
            continue;
        }

        // Check if this is a game announcement
        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (!doc.isObject())
            continue;

        QJsonObject obj = doc.object();
        if (obj["magic"].toString() != QString::fromLatin1(ANNOUNCE_MAGIC))
            continue;

        P2PGame game = P2PGame::fromJson(obj["game"].toObject());

        // Fill in sender IP if not set
        if (game.hostIp.isEmpty()) {
            game.hostIp = senderAddr.toString();
            // Remove IPv6 prefix if present
            if (game.hostIp.startsWith("::ffff:"))
                game.hostIp = game.hostIp.mid(7);
        }

        // Don't add our own game
        if (m_hosting && game.gameCode == m_hostedGame.gameCode)
            continue;

        // Update or add to discovered games
        bool found = false;
        for (int i = 0; i < m_discoveredGames.size(); ++i) {
            if (m_discoveredGames[i].gameCode == game.gameCode) {
                m_discoveredGames[i] = game;
                found = true;
                break;
            }
        }
        if (!found) {
            m_discoveredGames.append(game);
            emit gameFound(game);
        }
    }
}

QVector<P2PGame> P2PDiscovery::availableGames() const
{
    return m_discoveredGames;
}

void P2PDiscovery::broadcastMessage(const QJsonObject& msg)
{
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QString text = QString::fromUtf8(data);
    for (QWebSocket* peer : m_peers) {
        peer->sendTextMessage(text);
    }
}

void P2PDiscovery::sendToPeer(QWebSocket* peer, const QJsonObject& msg)
{
    if (peer) {
        QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
        peer->sendTextMessage(QString::fromUtf8(data));
    }
}
