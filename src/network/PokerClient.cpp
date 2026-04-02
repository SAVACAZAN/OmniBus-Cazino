#include "PokerClient.h"
#include "server/PokerProtocol.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QRandomGenerator>

PokerClient::PokerClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    connect(m_socket, &QWebSocket::connected,
            this, &PokerClient::onConnected);
    connect(m_socket, &QWebSocket::disconnected,
            this, &PokerClient::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived,
            this, &PokerClient::onTextMessage);
}

PokerClient::~PokerClient()
{
    disconnect();
}

void PokerClient::connectToServer(const QString& url)
{
    m_socket->open(QUrl(url));
}

void PokerClient::disconnect()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->close();
    }
}

bool PokerClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

// ─── Actions ──────────────────────────────────────────────────────────────────

void PokerClient::login(const QString& name, double balance)
{
    QJsonObject msg = PokerProtocol::makeLogin(name, balance);
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::requestTableList()
{
    QJsonObject msg = PokerProtocol::makeListTables();
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::createTable(const QString& name, double sb, double bb,
                               double minBuy, double maxBuy, int seats)
{
    QJsonObject msg = PokerProtocol::makeCreateTable(name, sb, bb, minBuy, maxBuy, seats);
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::joinTable(const QString& tableId, double buyIn)
{
    QJsonObject msg = PokerProtocol::makeJoinTable(tableId, buyIn);
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::leaveTable()
{
    QJsonObject msg = PokerProtocol::makeLeaveTable();
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
    m_currentTableId.clear();
}

void PokerClient::sendAction(const QString& action, double amount)
{
    QJsonObject msg = PokerProtocol::makeAction(action, amount);
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::sendChat(const QString& message)
{
    QJsonObject msg = PokerProtocol::makeChat(message);
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::addFunds(double amount)
{
    QJsonObject msg = PokerProtocol::makeAddFunds(amount);
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::chooseSeat(int seat, double buyIn)
{
    QJsonObject msg = PokerProtocol::makeChooseSeat(seat);
    if (buyIn > 0) msg["buy_in"] = buyIn;
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::sendClientSeed(const QString& seed)
{
    QJsonObject msg = PokerProtocol::makeClientSeed(seed);
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

void PokerClient::verifyHand(int handNumber)
{
    QJsonObject msg = PokerProtocol::makeVerifyHand(handNumber);
    m_socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void PokerClient::onConnected()
{
    emit connected();
}

void PokerClient::onDisconnected()
{
    m_loggedIn = false;
    m_currentTableId.clear();
    emit disconnected();
}

void PokerClient::onTextMessage(const QString& message)
{
    QJsonObject msg = PokerProtocol::fromJson(message.toUtf8());
    if (msg.isEmpty()) return;

    QString type = PokerProtocol::messageType(msg);

    if (type == "login_ok") {
        m_playerId = msg["player_id"].toString();
        m_balance = msg["balance"].toDouble();
        m_loggedIn = true;
        emit loginOk(m_playerId, m_balance);
    }
    else if (type == "table_list") {
        emit tableListReceived(msg["tables"].toArray());
    }
    else if (type == "table_joined") {
        m_currentTableId = msg["table_id"].toString();
        int seat = msg["seat"].toInt(-1);
        QJsonObject state = msg["table_state"].toObject();
        emit tableJoined(m_currentTableId, seat, state);
    }
    else if (type == "table_state") {
        emit tableStateUpdated(msg);
    }
    else if (type == "deal") {
        QJsonArray cardsArr = msg["your_cards"].toArray();
        QStringList cards;
        for (const auto& c : cardsArr)
            cards.append(c.toString());
        emit cardsDealt(cards);
    }
    else if (type == "your_turn") {
        double toCall = msg["to_call"].toDouble();
        double minRaise = msg["min_raise"].toDouble();
        double pot = msg["pot"].toDouble();
        emit yourTurn(toCall, minRaise, pot);
    }
    else if (type == "action_result") {
        int seat = msg["seat"].toInt();
        QString action = msg["action"].toString();
        double amount = msg["amount"].toDouble();
        emit actionResult(seat, action, amount);
    }
    else if (type == "showdown") {
        emit showdownResult(msg);
    }
    else if (type == "chat_msg") {
        emit chatReceived(msg["from"].toString(), msg["message"].toString());
    }
    else if (type == "player_joined") {
        emit playerJoinedTable(msg["seat"].toInt(), msg["name"].toString(), msg["stack"].toDouble());
    }
    else if (type == "player_left") {
        emit playerLeftTable(msg["seat"].toInt(-1), msg["name"].toString());
    }
    else if (type == "balance_update") {
        m_balance = msg["balance"].toDouble();
        emit balanceUpdated(m_balance);
    }
    else if (type == "error") {
        emit errorReceived(msg["message"].toString());
    }
    // ─── New message types ───────────────────────────────────────────────
    else if (type == "seat_chosen") {
        emit seatChosenResult(msg["seat"].toInt(), msg["player"].toString());
    }
    else if (type == "seat_rejected") {
        emit seatRejectedResult(msg["seat"].toInt(), msg["reason"].toString());
    }
    else if (type == "hand_start") {
        emit handStartReceived(msg["hand_number"].toInt(),
                               msg["server_seed_hash"].toString(),
                               msg["dealer_seat"].toInt());
    }
    else if (type == "seed_request") {
        // Auto-generate and send a client seed
        QByteArray randomBytes(16, 0);
        for (int i = 0; i < 16; ++i)
            randomBytes[i] = static_cast<char>(QRandomGenerator::securelySeeded().bounded(256));
        QString clientSeed = QString::fromLatin1(randomBytes.toHex());
        sendClientSeed(clientSeed);
        emit seedRequested();
    }
    else if (type == "timer_update") {
        emit timerUpdated(msg["seat"].toInt(), msg["seconds_left"].toInt());
    }
    else if (type == "hand_complete") {
        emit handCompleted(msg["hand_number"].toInt(),
                           msg["server_seed"].toString(),
                           msg["metadata"].toObject());
    }
    else if (type == "hand_history") {
        emit handHistoryReceived(msg["hand_number"].toInt(), msg);
    }
}
