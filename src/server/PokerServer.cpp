#include "PokerServer.h"
#include "PokerProtocol.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>

PokerServer::PokerServer(quint16 port, QObject* parent)
    : QObject(parent)
    , m_port(port)
    , m_server(nullptr)
{
}

PokerServer::~PokerServer()
{
    stop();
}

bool PokerServer::start()
{
    if (m_server) return false; // already running

    m_server = new QWebSocketServer("OmniBus Poker Server",
                                     QWebSocketServer::NonSecureMode, this);

    if (!m_server->listen(QHostAddress::Any, m_port)) {
        emit logMessage(QString("Failed to start server on port %1: %2")
                            .arg(m_port).arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QWebSocketServer::newConnection,
            this, &PokerServer::onNewConnection);

    emit logMessage(QString("Poker server started on port %1").arg(m_port));
    emit serverStarted(m_port);

    // Create a default table
    auto* defaultTable = new PokerTable("t1", "Welcome Table", 5, 10, 100, 1000, 9, this);
    connectTableSignals(defaultTable);
    m_tables["t1"] = defaultTable;
    m_nextTableId = 2;

    // Create preset tournaments
    createPresetTournaments();

    return true;
}

bool PokerServer::startSecure(const QString& certPath, const QString& keyPath)
{
    if (m_server) return false;

    // Load SSL certificate
    QFile certFile(certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        emit logMessage("SSL: Cannot open certificate file: " + certPath);
        return start(); // Fallback to non-secure
    }
    QSslCertificate certificate(&certFile, QSsl::Pem);
    certFile.close();

    if (certificate.isNull()) {
        emit logMessage("SSL: Invalid certificate");
        return start();
    }

    // Load private key
    QFile keyFile(keyPath);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        emit logMessage("SSL: Cannot open key file: " + keyPath);
        return start();
    }
    QSslKey sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
    if (sslKey.isNull())
        sslKey = QSslKey(&keyFile, QSsl::Ec, QSsl::Pem); // Try EC key
    keyFile.close();

    if (sslKey.isNull()) {
        emit logMessage("SSL: Invalid private key");
        return start();
    }

    // Create SECURE WebSocket server
    m_server = new QWebSocketServer("OmniBus Poker Server (Secure)",
                                     QWebSocketServer::SecureMode, this);

    // Configure SSL
    QSslConfiguration sslConfig;
    sslConfig.setLocalCertificate(certificate);
    sslConfig.setPrivateKey(sslKey);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone); // Don't verify client certs
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    m_server->setSslConfiguration(sslConfig);

    if (!m_server->listen(QHostAddress::Any, m_port)) {
        emit logMessage(QString("SSL: Failed to start secure server on port %1: %2")
                            .arg(m_port).arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        // Fallback to non-secure
        emit logMessage("SSL: Falling back to non-secure mode");
        return start();
    }

    connect(m_server, &QWebSocketServer::newConnection,
            this, &PokerServer::onNewConnection);

    emit logMessage(QString("SSL: Secure poker server started on port %1 (WSS)").arg(m_port));
    emit serverStarted(m_port);

    // Create default table + tournaments
    auto* defaultTable = new PokerTable("t1", "Welcome Table", 5, 10, 100, 1000, 9, this);
    connectTableSignals(defaultTable);
    m_tables["t1"] = defaultTable;
    m_nextTableId = 2;
    createPresetTournaments();

    return true;
}

void PokerServer::stop()
{
    if (!m_server) return;

    // Disconnect all clients
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        if (it.key())
            it.key()->close();
    }
    m_players.clear();

    // Delete tournaments
    qDeleteAll(m_tournaments);
    m_tournaments.clear();

    // Delete tables
    qDeleteAll(m_tables);
    m_tables.clear();

    m_server->close();
    delete m_server;
    m_server = nullptr;

    emit logMessage("Poker server stopped");
    emit serverStopped();
}

bool PokerServer::isRunning() const
{
    return m_server && m_server->isListening();
}

int PokerServer::playerCount() const
{
    return m_players.size();
}

int PokerServer::tableCount() const
{
    return m_tables.size();
}

// ─── Connection Handling ──────────────────────────────────────────────────────

void PokerServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QWebSocket* socket = m_server->nextPendingConnection();
        if (!socket) continue;

        connect(socket, &QWebSocket::textMessageReceived,
                this, &PokerServer::onTextMessage);
        connect(socket, &QWebSocket::disconnected,
                this, &PokerServer::onDisconnected);

        // Register as unidentified player for now
        ConnectedPlayer cp;
        cp.socket = socket;
        m_players[socket] = cp;

        emit logMessage(QString("New connection from %1").arg(socket->peerAddress().toString()));
    }
}

void PokerServer::onTextMessage(const QString& message)
{
    QWebSocket* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    QJsonObject msg = PokerProtocol::fromJson(message.toUtf8());
    if (msg.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Invalid JSON"}});
        return;
    }

    QString type = PokerProtocol::messageType(msg);

    if (type == "login")             handleLogin(socket, msg);
    else if (type == "list_tables")  handleListTables(socket);
    else if (type == "create_table") handleCreateTable(socket, msg);
    else if (type == "join_table")   handleJoinTable(socket, msg);
    else if (type == "leave_table")  handleLeaveTable(socket);
    else if (type == "action")       handleAction(socket, msg);
    else if (type == "chat")         handleChat(socket, msg);
    else if (type == "add_funds")    handleAddFunds(socket, msg);
    else if (type == "choose_seat")  handleChooseSeat(socket, msg);
    else if (type == "client_seed")  handleClientSeed(socket, msg);
    else if (type == "verify_hand")           handleVerifyHand(socket, msg);
    else if (type == "create_tournament")     handleCreateTournament(socket, msg);
    else if (type == "register_tournament")   handleRegisterTournament(socket, msg);
    else if (type == "unregister_tournament") handleUnregisterTournament(socket, msg);
    else if (type == "list_tournaments")      handleListTournaments(socket);
    else {
        sendTo(socket, {{"type", "error"}, {"message", QString("Unknown message type: %1").arg(type)}});
    }
}

void PokerServer::onDisconnected()
{
    QWebSocket* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    auto it = m_players.find(socket);
    if (it != m_players.end()) {
        ConnectedPlayer& cp = it.value();
        QString playerName = cp.name;

        // Remove from table if seated
        if (!cp.currentTableId.isEmpty() && m_tables.contains(cp.currentTableId)) {
            m_tables[cp.currentTableId]->removePlayer(cp.playerId);

            // Broadcast to table that player left
            QJsonObject leftMsg;
            leftMsg["type"] = "player_left";
            leftMsg["name"] = cp.name;
            broadcastToTable(cp.currentTableId, leftMsg, socket);
        }

        m_players.erase(it);
        emit playerDisconnected(playerName);
        emit logMessage(QString("Player disconnected: %1").arg(playerName));
    }

    socket->deleteLater();
}

// ─── Message Handlers ─────────────────────────────────────────────────────────

void PokerServer::handleLogin(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp) return;

    cp->name = msg["name"].toString("Player");
    cp->balance = msg["balance"].toDouble(5000);
    cp->playerId = QString("p%1").arg(m_nextPlayerId++);

    QJsonObject reply;
    reply["type"] = "login_ok";
    reply["player_id"] = cp->playerId;
    reply["balance"] = cp->balance;
    sendTo(socket, reply);

    emit playerConnected(cp->name);
    emit logMessage(QString("Player logged in: %1 (id: %2, balance: %3)")
                        .arg(cp->name, cp->playerId)
                        .arg(cp->balance));
}

void PokerServer::handleListTables(QWebSocket* socket)
{
    QJsonArray tablesArr;
    for (auto it = m_tables.begin(); it != m_tables.end(); ++it) {
        tablesArr.append(it.value()->tableInfo());
    }

    QJsonObject reply;
    reply["type"] = "table_list";
    reply["tables"] = tablesArr;
    sendTo(socket, reply);
}

void PokerServer::handleCreateTable(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->playerId.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Must login first"}});
        return;
    }

    QString tableName = msg["name"].toString("New Table");
    double sb = msg["small_blind"].toDouble(5);
    double bb = msg["big_blind"].toDouble(10);
    double minBuy = msg["min_buy_in"].toDouble(100);
    double maxBuy = msg["max_buy_in"].toDouble(1000);
    int seats = msg["max_seats"].toInt(9);

    if (seats < 2) seats = 2;
    if (seats > 9) seats = 9;
    if (sb <= 0) sb = 5;
    if (bb <= 0) bb = sb * 2;
    if (minBuy < bb * 10) minBuy = bb * 10;
    if (maxBuy < minBuy) maxBuy = minBuy * 10;

    QString tableId = QString("t%1").arg(m_nextTableId++);
    auto* table = new PokerTable(tableId, tableName, sb, bb, minBuy, maxBuy, seats, this);
    connectTableSignals(table);
    m_tables[tableId] = table;

    emit logMessage(QString("Table created: %1 (%2) by %3").arg(tableName, tableId, cp->name));

    // Send table created confirmation (player must choose seat separately)
    QJsonObject reply;
    reply["type"] = "table_joined";
    reply["table_id"] = tableId;
    reply["seat"] = -1;  // not seated yet, must choose seat
    reply["table_state"] = table->fullState(cp->playerId);
    cp->currentTableId = tableId;
    sendTo(socket, reply);
}

void PokerServer::handleJoinTable(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->playerId.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Must login first"}});
        return;
    }

    // Leave current table first
    if (!cp->currentTableId.isEmpty()) {
        handleLeaveTable(socket);
    }

    QString tableId = msg["table_id"].toString();
    double buyIn = msg["buy_in"].toDouble();

    if (!m_tables.contains(tableId)) {
        sendTo(socket, {{"type", "error"}, {"message", "Table not found"}});
        return;
    }

    auto* table = m_tables[tableId];

    if (buyIn < table->minBuyIn() || buyIn > table->maxBuyIn()) {
        sendTo(socket, {{"type", "error"},
                        {"message", QString("Buy-in must be between %1 and %2")
                                        .arg(table->minBuyIn()).arg(table->maxBuyIn())}});
        return;
    }

    if (buyIn > cp->balance) {
        sendTo(socket, {{"type", "error"}, {"message", "Not enough balance"}});
        return;
    }

    // In online mode, player joins the table but must choose a seat
    // We store the buy-in intent and let them pick a seat via choose_seat
    cp->currentTableId = tableId;

    QJsonObject reply;
    reply["type"] = "table_joined";
    reply["table_id"] = tableId;
    reply["seat"] = -1;  // not seated yet
    reply["buy_in"] = buyIn;
    reply["table_state"] = table->fullState(cp->playerId);
    sendTo(socket, reply);

    emit logMessage(QString("[%1] %2 joined table (choosing seat)").arg(tableId, cp->name));
}

void PokerServer::handleChooseSeat(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->playerId.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Must login first"}});
        return;
    }

    if (cp->currentTableId.isEmpty() || !m_tables.contains(cp->currentTableId)) {
        sendTo(socket, {{"type", "error"}, {"message", "Not at a table"}});
        return;
    }

    int requestedSeat = msg["seat"].toInt(-1);
    double buyIn = msg["buy_in"].toDouble(0);

    // If no buy_in provided, use a default (min buy-in)
    auto* table = m_tables[cp->currentTableId];
    if (buyIn <= 0) buyIn = table->minBuyIn();

    // Validate buy-in
    if (buyIn < table->minBuyIn() || buyIn > table->maxBuyIn()) {
        sendTo(socket, {{"type", "error"},
                        {"message", QString("Buy-in must be between %1 and %2")
                                        .arg(table->minBuyIn()).arg(table->maxBuyIn())}});
        return;
    }

    if (buyIn > cp->balance) {
        sendTo(socket, {{"type", "error"}, {"message", "Not enough balance"}});
        return;
    }

    int seat = table->chooseSeat(cp->playerId, cp->name, buyIn, requestedSeat);

    if (seat >= 0) {
        cp->balance -= buyIn;

        QJsonObject reply;
        reply["type"] = "seat_chosen";
        reply["seat"] = seat;
        reply["player"] = cp->name;
        sendTo(socket, reply);

        // Broadcast to others
        QJsonObject joinMsg;
        joinMsg["type"] = "seat_chosen";
        joinMsg["seat"] = seat;
        joinMsg["player"] = cp->name;
        broadcastToTable(cp->currentTableId, joinMsg, socket);

        emit logMessage(QString("[%1] %2 chose seat %3").arg(cp->currentTableId, cp->name).arg(seat));
    } else {
        // seatRejected signal was already emitted by table
        QJsonObject rejectMsg;
        rejectMsg["type"] = "seat_rejected";
        rejectMsg["seat"] = requestedSeat;
        rejectMsg["reason"] = "Seat not available";
        sendTo(socket, rejectMsg);
    }
}

void PokerServer::handleClientSeed(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->currentTableId.isEmpty()) return;

    if (!m_tables.contains(cp->currentTableId)) return;

    QString seed = msg["seed"].toString();
    m_tables[cp->currentTableId]->submitClientSeed(cp->playerId, seed);
}

void PokerServer::handleVerifyHand(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->currentTableId.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Not at a table"}});
        return;
    }

    if (!m_tables.contains(cp->currentTableId)) {
        sendTo(socket, {{"type", "error"}, {"message", "Table not found"}});
        return;
    }

    int handNumber = msg["hand_number"].toInt();
    QJsonObject history = m_tables[cp->currentTableId]->getHandHistory(handNumber);

    if (history.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", QString("Hand #%1 not found").arg(handNumber)}});
        return;
    }

    QJsonObject reply;
    reply["type"] = "hand_history";
    reply["hand_number"] = handNumber;
    reply["actions"] = history["actions"];
    reply["result"] = history["final_state"];
    reply["server_seed"] = history["server_seed"];
    reply["server_seed_hash"] = history["server_seed_hash"];
    reply["combined_seed"] = history["combined_seed"];
    reply["client_seeds"] = history["client_seeds"];
    reply["result_hash"] = history["result_hash"];
    sendTo(socket, reply);
}

void PokerServer::handleLeaveTable(QWebSocket* socket)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->currentTableId.isEmpty()) return;

    QString tableId = cp->currentTableId;
    if (!m_tables.contains(tableId)) {
        cp->currentTableId.clear();
        return;
    }

    auto* table = m_tables[tableId];
    auto* serverPlayer = table->findPlayer(cp->playerId);

    // Return remaining stack to balance
    if (serverPlayer) {
        cp->balance += serverPlayer->stack;
    }

    table->removePlayer(cp->playerId);

    // Notify others
    QJsonObject leftMsg;
    leftMsg["type"] = "player_left";
    leftMsg["name"] = cp->name;
    broadcastToTable(tableId, leftMsg, socket);

    cp->currentTableId.clear();

    // Send updated balance
    QJsonObject balMsg;
    balMsg["type"] = "balance_update";
    balMsg["balance"] = cp->balance;
    sendTo(socket, balMsg);

    // Remove empty tables (except the default)
    if (table->playerCount() == 0 && tableId != "t1") {
        m_tables.remove(tableId);
        table->deleteLater();
    }
}

void PokerServer::handleAction(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->currentTableId.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Not at a table"}});
        return;
    }

    if (!m_tables.contains(cp->currentTableId)) {
        sendTo(socket, {{"type", "error"}, {"message", "Table not found"}});
        return;
    }

    auto* table = m_tables[cp->currentTableId];
    QString action = msg["action"].toString();
    double amount = msg["amount"].toDouble(0);

    table->handleAction(cp->playerId, action, amount);
}

void PokerServer::handleChat(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->currentTableId.isEmpty()) return;

    QString message = msg["message"].toString();
    if (message.isEmpty()) return;

    QJsonObject chatMsg;
    chatMsg["type"] = "chat_msg";
    chatMsg["from"] = cp->name;
    chatMsg["message"] = message;
    broadcastToTable(cp->currentTableId, chatMsg);
}

void PokerServer::handleAddFunds(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp) return;

    double amount = msg["amount"].toDouble(0);
    if (amount <= 0) {
        sendTo(socket, {{"type", "error"}, {"message", "Invalid amount"}});
        return;
    }

    cp->balance += amount;

    QJsonObject reply;
    reply["type"] = "balance_update";
    reply["balance"] = cp->balance;
    sendTo(socket, reply);

    emit logMessage(QString("%1 added %2 funds (new balance: %3)")
                        .arg(cp->name).arg(amount).arg(cp->balance));
}

// ─── Utilities ────────────────────────────────────────────────────────────────

void PokerServer::broadcastToTable(const QString& tableId, const QJsonObject& msg, QWebSocket* except)
{
    QByteArray data = PokerProtocol::toJson(msg);
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        if (it.value().currentTableId == tableId && it.key() != except) {
            it.key()->sendTextMessage(QString::fromUtf8(data));
        }
    }
}

void PokerServer::sendTo(QWebSocket* socket, const QJsonObject& msg)
{
    if (socket && socket->isValid()) {
        socket->sendTextMessage(QString::fromUtf8(PokerProtocol::toJson(msg)));
    }
}

ConnectedPlayer* PokerServer::findPlayer(QWebSocket* socket)
{
    auto it = m_players.find(socket);
    if (it != m_players.end())
        return &it.value();
    return nullptr;
}

ConnectedPlayer* PokerServer::findPlayerById(const QString& id)
{
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        if (it.value().playerId == id)
            return &it.value();
    }
    return nullptr;
}

void PokerServer::connectTableSignals(PokerTable* table)
{
    // When state changes, broadcast full state to all players at the table
    connect(table, &PokerTable::stateChanged, this, [this](const QString& tableId) {
        if (!m_tables.contains(tableId)) return;
        auto* t = m_tables[tableId];
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().currentTableId == tableId) {
                QJsonObject state = t->fullState(it.value().playerId);
                sendTo(it.key(), state);
            }
        }
    });

    // When a hand starts, notify all players of their hole cards
    connect(table, &PokerTable::dealCards, this,
            [this](const QString& tableId, const QString& playerId, const QVector<Card>& cards) {
        auto* cp = findPlayerById(playerId);
        if (!cp || !cp->socket) return;

        QJsonObject dealMsg;
        dealMsg["type"] = "deal";
        QJsonArray cardsArr;
        for (const auto& c : cards)
            cardsArr.append(c.toString());
        dealMsg["your_cards"] = cardsArr;
        sendTo(cp->socket, dealMsg);
    });

    // When it is a player's turn, send them a your_turn message
    connect(table, &PokerTable::turnChanged, this,
            [this](const QString& tableId, const QString& playerId, double toCall, double minRaise) {
        auto* cp = findPlayerById(playerId);
        if (!cp || !cp->socket) return;
        if (!m_tables.contains(tableId)) return;

        auto* t = m_tables[tableId];
        QJsonObject turnMsg;
        turnMsg["type"] = "your_turn";
        turnMsg["to_call"] = toCall;
        turnMsg["min_raise"] = minRaise;
        turnMsg["pot"] = t->fullState(playerId)["pot"].toDouble();
        turnMsg["seconds_left"] = t->secondsLeft();
        sendTo(cp->socket, turnMsg);
    });

    // When an action is performed, broadcast it
    connect(table, &PokerTable::playerActed, this,
            [this](const QString& tableId, int seat, const QString& action, double amount) {
        QJsonObject actionMsg;
        actionMsg["type"] = "action_result";
        actionMsg["seat"] = seat;
        actionMsg["action"] = action;
        actionMsg["amount"] = amount;
        broadcastToTable(tableId, actionMsg);
    });

    // When showdown, broadcast results
    connect(table, &PokerTable::showdownResult, this,
            [this](const QString& tableId, const QJsonObject& result) {
        broadcastToTable(tableId, result);
    });

    // Player joined/left broadcasts
    connect(table, &PokerTable::playerJoinedTable, this,
            [this](const QString& tableId, int seat, const QString& name, double stack) {
        emit logMessage(QString("[%1] %2 joined seat %3 with %4")
                            .arg(tableId, name).arg(seat).arg(stack));
    });

    connect(table, &PokerTable::playerLeftTable, this,
            [this](const QString& tableId, int seat, const QString& name) {
        emit logMessage(QString("[%1] %2 left seat %3").arg(tableId, name).arg(seat));
    });

    // ─── New provably fair + timer signals ───────────────────────────────────

    // Broadcast hand_start with server seed hash commitment
    connect(table, &PokerTable::handStartBroadcast, this,
            [this](const QString& tableId, int handNumber, const QString& serverSeedHash, int dealerSeat) {
        QJsonObject msg;
        msg["type"] = "hand_start";
        msg["hand_number"] = handNumber;
        msg["server_seed_hash"] = serverSeedHash;
        msg["dealer_seat"] = dealerSeat;
        broadcastToTable(tableId, msg);
    });

    // Request client seeds from all players at the table
    connect(table, &PokerTable::seedRequest, this,
            [this](const QString& tableId) {
        QJsonObject msg;
        msg["type"] = "seed_request";
        broadcastToTable(tableId, msg);
    });

    // Timer countdown broadcast
    connect(table, &PokerTable::timerUpdate, this,
            [this](const QString& tableId, int seat, int secsLeft) {
        QJsonObject msg;
        msg["type"] = "timer_update";
        msg["seat"] = seat;
        msg["seconds_left"] = secsLeft;
        broadcastToTable(tableId, msg);
    });

    // Hand complete with server seed reveal
    connect(table, &PokerTable::handComplete, this,
            [this](const QString& tableId, int handNumber, const QString& serverSeed, const QJsonObject& metadata) {
        QJsonObject msg;
        msg["type"] = "hand_complete";
        msg["hand_number"] = handNumber;
        msg["server_seed"] = serverSeed;
        msg["metadata"] = metadata;
        broadcastToTable(tableId, msg);
    });

    // Seat chosen/rejected signals
    connect(table, &PokerTable::seatChosen, this,
            [this](const QString& tableId, int seat, const QString& playerName) {
        Q_UNUSED(tableId); Q_UNUSED(seat); Q_UNUSED(playerName);
        // Already handled in handleChooseSeat
    });

    connect(table, &PokerTable::seatRejected, this,
            [this](const QString& tableId, int seat, const QString& reason) {
        Q_UNUSED(tableId); Q_UNUSED(seat); Q_UNUSED(reason);
        // Already handled in handleChooseSeat
    });
}

// ─── Tournament Management ───────────────────────────────────────────────────────

Tournament* PokerServer::createTournament(const TournamentConfig& config)
{
    QString tid = QString("tour%1").arg(m_nextTournamentId++);
    auto* t = new Tournament(tid, config, this);
    connectTournamentSignals(t);
    m_tournaments[tid] = t;

    emit logMessage(QString("Tournament created: %1 (%2)").arg(config.name, tid));
    emit tournamentCreated(t);
    return t;
}

bool PokerServer::registerForTournament(const QString& playerId, const QString& tournamentId)
{
    if (!m_tournaments.contains(tournamentId))
        return false;

    auto* cp = findPlayerById(playerId);
    if (!cp)
        return false;

    auto* t = m_tournaments[tournamentId];

    if (cp->balance < t->config().buyIn) {
        if (cp->socket) {
            sendTo(cp->socket, {{"type", "error"}, {"message", "Not enough balance for tournament buy-in"}});
        }
        return false;
    }

    QString name = cp->name;
    if (!t->registerPlayer(playerId, name))
        return false;

    cp->balance -= t->config().buyIn;
    cp->currentTournamentId = tournamentId;

    emit logMessage(QString("%1 registered for tournament %2").arg(name, tournamentId));

    // Notify the player
    if (cp->socket) {
        QJsonObject reply;
        reply["type"] = "tournament_registered";
        reply["tournament_id"] = tournamentId;
        reply["tournament"] = t->toJson();
        sendTo(cp->socket, reply);
    }

    // Broadcast updated tournament info to all
    QJsonObject updateMsg;
    updateMsg["type"] = "tournament_update";
    updateMsg["tournament"] = t->toJson();
    broadcastToAll(updateMsg);

    // Auto-start SNG when full
    if (t->config().type == TournamentType::SitAndGo && t->isFull() &&
        t->status() == TournamentStatus::Registering) {
        t->start();
    }

    return true;
}

bool PokerServer::unregisterFromTournament(const QString& playerId, const QString& tournamentId)
{
    if (!m_tournaments.contains(tournamentId))
        return false;

    auto* t = m_tournaments[tournamentId];
    if (t->status() != TournamentStatus::Registering)
        return false;

    auto* cp = findPlayerById(playerId);
    if (!cp)
        return false;

    t->unregisterPlayer(playerId);
    cp->balance += t->config().buyIn; // refund
    cp->currentTournamentId.clear();

    emit logMessage(QString("%1 unregistered from tournament %2").arg(cp->name, tournamentId));

    if (cp->socket) {
        QJsonObject reply;
        reply["type"] = "tournament_unregistered";
        reply["tournament_id"] = tournamentId;
        reply["balance"] = cp->balance;
        sendTo(cp->socket, reply);
    }

    // Broadcast update
    QJsonObject updateMsg;
    updateMsg["type"] = "tournament_update";
    updateMsg["tournament"] = t->toJson();
    broadcastToAll(updateMsg);

    return true;
}

QVector<Tournament*> PokerServer::getTournaments() const
{
    QVector<Tournament*> result;
    for (auto* t : m_tournaments)
        result.append(t);
    return result;
}

Tournament* PokerServer::findTournament(const QString& tournamentId) const
{
    return m_tournaments.value(tournamentId, nullptr);
}

int PokerServer::tournamentCount() const
{
    return m_tournaments.size();
}

// ─── Tournament Message Handlers ─────────────────────────────────────────────────

void PokerServer::handleCreateTournament(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->playerId.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Must login first"}});
        return;
    }

    TournamentConfig cfg;
    cfg.name = msg["name"].toString("Tournament");
    cfg.type = (msg["type_str"].toString("SitAndGo") == "MTT") ? TournamentType::MTT : TournamentType::SitAndGo;
    cfg.maxPlayers = msg["max_players"].toInt(9);
    cfg.buyIn = msg["buy_in"].toDouble(500);
    cfg.startingStack = msg["starting_stack"].toDouble(5000);
    cfg.blindLevelMinutes = msg["blind_level_minutes"].toInt(5);
    cfg.allowLateReg = msg["allow_late_reg"].toBool(false);
    cfg.allowRebuys = msg["allow_rebuys"].toBool(false);
    cfg.ensureDefaults();

    auto* t = createTournament(cfg);

    QJsonObject reply;
    reply["type"] = "tournament_created";
    reply["tournament"] = t->toJson();
    sendTo(socket, reply);
}

void PokerServer::handleRegisterTournament(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->playerId.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Must login first"}});
        return;
    }

    QString tournamentId = msg["tournament_id"].toString();
    if (!registerForTournament(cp->playerId, tournamentId)) {
        sendTo(socket, {{"type", "error"}, {"message", "Failed to register for tournament"}});
    }
}

void PokerServer::handleUnregisterTournament(QWebSocket* socket, const QJsonObject& msg)
{
    auto* cp = findPlayer(socket);
    if (!cp || cp->playerId.isEmpty()) {
        sendTo(socket, {{"type", "error"}, {"message", "Must login first"}});
        return;
    }

    QString tournamentId = msg["tournament_id"].toString();
    if (!unregisterFromTournament(cp->playerId, tournamentId)) {
        sendTo(socket, {{"type", "error"}, {"message", "Cannot unregister (tournament may have started)"}});
    }
}

void PokerServer::handleListTournaments(QWebSocket* socket)
{
    QJsonArray arr;
    for (auto it = m_tournaments.begin(); it != m_tournaments.end(); ++it) {
        arr.append(it.value()->toJson());
    }

    QJsonObject reply;
    reply["type"] = "tournament_list";
    reply["tournaments"] = arr;
    sendTo(socket, reply);
}

// ─── Tournament Signal Wiring ────────────────────────────────────────────────────

void PokerServer::connectTournamentSignals(Tournament* t)
{
    connect(t, &Tournament::tournamentStarted, this, [this](const QString& tid) {
        emit logMessage(QString("Tournament %1 started!").arg(tid));
        QJsonObject msg;
        msg["type"] = "tournament_started";
        msg["tournament_id"] = tid;
        if (m_tournaments.contains(tid))
            msg["tournament"] = m_tournaments[tid]->toJson();
        broadcastToAll(msg);
    });

    connect(t, &Tournament::blindsIncreased, this, [this, t](int level, double sb, double bb) {
        QJsonObject msg;
        msg["type"] = "blinds_update";
        msg["tournament_id"] = t->id();
        msg["level"] = level;
        msg["small_blind"] = sb;
        msg["big_blind"] = bb;
        msg["seconds_until_next"] = t->secondsUntilNextLevel();
        broadcastToAll(msg);
    });

    connect(t, &Tournament::playerEliminated, this,
            [this, t](const QString& playerId, int position, double prize) {
        emit logMessage(QString("Tournament %1: %2 eliminated at #%3 (prize: %4)")
                            .arg(t->id(), playerId).arg(position).arg(prize));
        QJsonObject msg;
        msg["type"] = "tournament_player_eliminated";
        msg["tournament_id"] = t->id();
        msg["player_id"] = playerId;
        msg["position"] = position;
        msg["prize"] = prize;
        broadcastToAll(msg);
    });

    connect(t, &Tournament::tournamentFinished, this,
            [this, t](const QString& tid, const QVector<TournamentPlayer>& results) {
        emit logMessage(QString("Tournament %1 finished!").arg(tid));

        // Award prizes to connected players
        for (const auto& tp : results) {
            if (tp.prizeWon > 0) {
                auto* cp = findPlayerById(tp.playerId);
                if (cp) {
                    cp->balance += tp.prizeWon;
                    cp->currentTournamentId.clear();
                    if (cp->socket) {
                        QJsonObject balMsg;
                        balMsg["type"] = "balance_update";
                        balMsg["balance"] = cp->balance;
                        sendTo(cp->socket, balMsg);
                    }
                }
            }
        }

        QJsonObject msg;
        msg["type"] = "tournament_finished";
        msg["tournament_id"] = tid;
        msg["tournament"] = t->toJson();
        broadcastToAll(msg);
    });

    connect(t, &Tournament::statusChanged, this, [this, t](TournamentStatus) {
        QJsonObject msg;
        msg["type"] = "tournament_update";
        msg["tournament"] = t->toJson();
        broadcastToAll(msg);
    });
}

// ─── Broadcast to All ────────────────────────────────────────────────────────────

void PokerServer::broadcastToAll(const QJsonObject& msg, QWebSocket* except)
{
    QByteArray data = PokerProtocol::toJson(msg);
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        if (it.key() != except && it.key() && it.key()->isValid()) {
            it.key()->sendTextMessage(QString::fromUtf8(data));
        }
    }
}

// ─── Preset Tournaments ──────────────────────────────────────────────────────────

void PokerServer::createPresetTournaments()
{
    // 1. Quick SNG 6-Max
    {
        TournamentConfig cfg;
        cfg.name = "Quick SNG 6-Max";
        cfg.type = TournamentType::SitAndGo;
        cfg.maxPlayers = 6;
        cfg.buyIn = 100;
        cfg.startingStack = 1000;
        cfg.blindLevelMinutes = 3;
        cfg.allowLateReg = false;
        cfg.allowRebuys = false;
        cfg.prizeStructure = {50.0, 30.0, 20.0};
        cfg.ensureDefaults();
        createTournament(cfg);
    }

    // 2. Standard SNG 9-Max
    {
        TournamentConfig cfg;
        cfg.name = "Standard SNG 9-Max";
        cfg.type = TournamentType::SitAndGo;
        cfg.maxPlayers = 9;
        cfg.buyIn = 500;
        cfg.startingStack = 5000;
        cfg.blindLevelMinutes = 5;
        cfg.allowLateReg = false;
        cfg.allowRebuys = false;
        cfg.prizeStructure = {50.0, 30.0, 20.0};
        cfg.ensureDefaults();
        createTournament(cfg);
    }

    // 3. Mini MTT
    {
        TournamentConfig cfg;
        cfg.name = "Mini MTT";
        cfg.type = TournamentType::MTT;
        cfg.maxPlayers = 18;
        cfg.buyIn = 200;
        cfg.startingStack = 3000;
        cfg.blindLevelMinutes = 8;
        cfg.allowLateReg = true;
        cfg.lateRegLevels = 3;
        cfg.allowRebuys = false;
        cfg.ensureDefaults();
        createTournament(cfg);
    }

    // 4. Big Sunday MTT
    {
        TournamentConfig cfg;
        cfg.name = "Big Sunday MTT";
        cfg.type = TournamentType::MTT;
        cfg.maxPlayers = 45;
        cfg.buyIn = 1000;
        cfg.startingStack = 10000;
        cfg.blindLevelMinutes = 12;
        cfg.allowLateReg = true;
        cfg.lateRegLevels = 4;
        cfg.allowRebuys = true;
        cfg.rebuyLevels = 4;
        cfg.rebuyAmount = 1000;
        cfg.ensureDefaults();
        createTournament(cfg);
    }

    emit logMessage("4 preset tournaments created");
}
