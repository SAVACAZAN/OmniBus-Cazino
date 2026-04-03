#include "LobbyWidget.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QScrollArea>

LobbyWidget::LobbyWidget(PokerClient* client, QWidget* parent)
    : QWidget(parent)
    , m_client(client)
{
    setupUI();

    // Connect client signals
    connect(m_client, &PokerClient::connected, this, &LobbyWidget::onConnected);
    connect(m_client, &PokerClient::disconnected, this, &LobbyWidget::onDisconnected);
    connect(m_client, &PokerClient::loginOk, this, &LobbyWidget::onLoginOk);
    connect(m_client, &PokerClient::tableListReceived, this, &LobbyWidget::onTableListReceived);
    connect(m_client, &PokerClient::tableJoined, this, &LobbyWidget::onTableJoined);
    connect(m_client, &PokerClient::errorReceived, this, &LobbyWidget::onError);

    // Auto-refresh timer
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000);
    connect(m_refreshTimer, &QTimer::timeout, this, &LobbyWidget::refreshTables);
}

void LobbyWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    setStyleSheet("background: #0f1419; color: #fff;");

    // ─── Title ────────────────────────────────────────────────────────────────
    m_titleLabel = new QLabel("POKER LOBBY");
    m_titleLabel->setStyleSheet(
        "font-size: 28px; font-weight: 900; color: #facc15; "
        "letter-spacing: 3px; padding: 8px 0;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    auto* subLabel = new QLabel("Multiplayer Texas Hold'em - WebSocket Server");
    subLabel->setStyleSheet("font-size: 12px; color: #888; letter-spacing: 1px;");
    subLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subLabel);

    // ─── Connection bar ───────────────────────────────────────────────────────
    auto* connFrame = new QWidget;
    connFrame->setStyleSheet(
        "background: rgba(26,31,46,0.8); border: 1px solid rgba(250,204,21,0.2); "
        "border-radius: 8px; padding: 8px;");
    auto* connLayout = new QHBoxLayout(connFrame);
    connLayout->setSpacing(10);

    QString inputStyle = "QLineEdit { background: rgba(0,0,0,0.4); color: #facc15; border: 1px solid rgba(250,204,21,0.3); "
        "border-radius: 4px; padding: 5px 8px; font-size: 11px; font-weight: bold; }";
    QString lblStyle = "color: rgba(250,204,21,0.5); font-weight: 700; font-size: 9px; background: transparent;";

    // Server address — CONNECT TO ANY IP
    auto* serverLabel = new QLabel("SERVER");
    serverLabel->setStyleSheet(lblStyle);
    connLayout->addWidget(serverLabel);

    m_serverEdit = new QLineEdit("ws://193.219.97.147:9999");
    m_serverEdit->setFixedWidth(200);
    m_serverEdit->setStyleSheet(inputStyle);
    m_serverEdit->setPlaceholderText("ws://193.219.97.147:9999");
    connLayout->addWidget(m_serverEdit);

    // Player name
    auto* nameLabel = new QLabel("NAME");
    nameLabel->setStyleSheet(lblStyle);
    connLayout->addWidget(nameLabel);

    m_nameEdit = new QLineEdit("Player1");
    m_nameEdit->setFixedWidth(100);
    m_nameEdit->setStyleSheet(inputStyle);
    connLayout->addWidget(m_nameEdit);

    // Password
    auto* passLabel = new QLabel("PASS");
    passLabel->setStyleSheet(lblStyle);
    connLayout->addWidget(passLabel);

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setFixedWidth(80);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("optional");
    m_passwordEdit->setStyleSheet(inputStyle);
    connLayout->addWidget(m_passwordEdit);

    // Balance
    auto* balLabel = new QLabel("BUY-IN");
    balLabel->setStyleSheet(lblStyle);
    connLayout->addWidget(balLabel);

    m_balanceSpin = new QDoubleSpinBox;
    m_balanceSpin->setRange(100, 1000000);
    m_balanceSpin->setValue(10000);
    m_balanceSpin->setDecimals(0);
    m_balanceSpin->setPrefix("$ ");
    m_balanceSpin->setFixedWidth(100);
    m_balanceSpin->setStyleSheet(
        "QDoubleSpinBox { background: rgba(0,0,0,0.4); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.3); border-radius: 4px; padding: 5px 8px; "
        "font-size: 11px; font-weight: bold; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; }");
    connLayout->addWidget(m_balanceSpin);

    connLayout->addStretch();

    m_connectBtn = new QPushButton("CONNECT");
    m_connectBtn->setFixedSize(120, 36);
    m_connectBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #10eb04, stop:1 #0cc800); "
        "color: #0f1419; border: none; border-radius: 6px; font-weight: 900; font-size: 12px; letter-spacing: 1px; }"
        "QPushButton:hover { background: #10eb04; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_connectBtn, &QPushButton::clicked, this, &LobbyWidget::connectToServer);
    connLayout->addWidget(m_connectBtn);

    mainLayout->addWidget(connFrame);

    // ─── Player info + status ─────────────────────────────────────────────────
    auto* infoBar = new QHBoxLayout;

    m_playerInfoLabel = new QLabel("Not connected");
    m_playerInfoLabel->setStyleSheet("color: #888; font-size: 12px; font-weight: bold;");
    infoBar->addWidget(m_playerInfoLabel);

    infoBar->addStretch();

    m_statusLabel = new QLabel("");
    m_statusLabel->setStyleSheet("color: #888; font-size: 11px;");
    infoBar->addWidget(m_statusLabel);

    mainLayout->addLayout(infoBar);

    // ─── Table list ───────────────────────────────────────────────────────────
    auto* tableFrame = new QWidget;
    tableFrame->setStyleSheet(
        "background: rgba(26,31,46,0.6); border: 1px solid rgba(250,204,21,0.15); "
        "border-radius: 8px;");
    auto* tableLayout = new QVBoxLayout(tableFrame);

    auto* tableHeader = new QHBoxLayout;
    auto* tableTitle = new QLabel("Available Tables");
    tableTitle->setStyleSheet("color: #facc15; font-size: 14px; font-weight: 800; background: transparent;");
    tableHeader->addWidget(tableTitle);

    tableHeader->addStretch();

    m_refreshBtn = new QPushButton("Refresh");
    m_refreshBtn->setFixedSize(80, 30);
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setStyleSheet(
        "QPushButton { background: rgba(250,204,21,0.12); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.25); border-radius: 4px; "
        "font-weight: 700; font-size: 11px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.25); }"
        "QPushButton:disabled { background: #222; color: #555; border-color: #333; }");
    connect(m_refreshBtn, &QPushButton::clicked, this, &LobbyWidget::refreshTables);
    tableHeader->addWidget(m_refreshBtn);

    m_createTableBtn = new QPushButton("+ Create Table");
    m_createTableBtn->setFixedSize(120, 30);
    m_createTableBtn->setEnabled(false);
    m_createTableBtn->setStyleSheet(
        "QPushButton { background: rgba(250,204,21,0.15); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.3); border-radius: 4px; "
        "font-weight: 800; font-size: 11px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.3); }"
        "QPushButton:disabled { background: #222; color: #555; border-color: #333; }");
    connect(m_createTableBtn, &QPushButton::clicked, this, &LobbyWidget::showCreateTableDialog);
    tableHeader->addWidget(m_createTableBtn);

    tableLayout->addLayout(tableHeader);

    // Table widget
    m_tableList = new QTableWidget;
    m_tableList->setColumnCount(6);
    m_tableList->setHorizontalHeaderLabels({"Table Name", "Blinds", "Players", "Avg Pot", "Buy-In", ""});
    m_tableList->horizontalHeader()->setStretchLastSection(false);
    m_tableList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableList->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableList->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tableList->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_tableList->horizontalHeader()->resizeSection(5, 90);
    m_tableList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableList->verticalHeader()->setVisible(false);
    m_tableList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableList->setMinimumHeight(250);

    m_tableList->setStyleSheet(
        "QTableWidget { background: rgba(0,0,0,0.3); border: none; color: #ccc; "
        "gridline-color: rgba(250,204,21,0.1); font-size: 12px; }"
        "QTableWidget::item { padding: 8px; border-bottom: 1px solid rgba(250,204,21,0.08); }"
        "QTableWidget::item:selected { background: rgba(250,204,21,0.15); color: #facc15; }"
        "QHeaderView::section { background: rgba(250,204,21,0.08); color: #facc15; "
        "border: none; border-bottom: 2px solid rgba(250,204,21,0.3); padding: 8px; "
        "font-weight: 800; font-size: 11px; letter-spacing: 1px; }");

    tableLayout->addWidget(m_tableList);
    mainLayout->addWidget(tableFrame, 1);
}

void LobbyWidget::connectToServer()
{
    if (m_connected) {
        m_client->disconnect();
        return;
    }

    QString serverAddr = m_serverEdit->text().trimmed();
    if (serverAddr.isEmpty()) serverAddr = "ws://193.219.97.147:9999";
    if (!serverAddr.startsWith("ws://") && !serverAddr.startsWith("wss://"))
        serverAddr = "ws://" + serverAddr;
    if (!serverAddr.contains(":") || serverAddr.count(":") < 2)
        serverAddr += ":9999";

    updateStatus("Connecting to " + serverAddr + "...", "#facc15");
    m_connectBtn->setEnabled(false);
    m_client->connectToServer(serverAddr);
}

void LobbyWidget::onConnected()
{
    m_connected = true;
    m_connectBtn->setText("DISCONNECT");
    m_connectBtn->setEnabled(true);
    m_connectBtn->setStyleSheet(
        "QPushButton { background: rgba(235,4,4,0.8); color: #fff; border: none; "
        "border-radius: 6px; font-weight: 900; font-size: 12px; letter-spacing: 1px; }"
        "QPushButton:hover { background: #eb0404; }");

    updateStatus("Connected! Logging in...", "#10eb04");

    // Auto-login
    m_client->login(m_nameEdit->text(), m_balanceSpin->value());
}

void LobbyWidget::onDisconnected()
{
    m_connected = false;
    m_connectBtn->setText("CONNECT");
    m_connectBtn->setEnabled(true);
    m_connectBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #10eb04, stop:1 #0cc800); "
        "color: #0f1419; border: none; border-radius: 6px; font-weight: 900; font-size: 12px; letter-spacing: 1px; }"
        "QPushButton:hover { background: #10eb04; }");

    m_refreshBtn->setEnabled(false);
    m_createTableBtn->setEnabled(false);
    m_refreshTimer->stop();
    m_tableList->setRowCount(0);
    m_playerInfoLabel->setText("Not connected");
    updateStatus("Disconnected", "#eb0404");
}

void LobbyWidget::onLoginOk(const QString& playerId, double balance)
{
    m_playerInfoLabel->setText(QString("Player: %1 | ID: %2 | Balance: $%3")
                                  .arg(m_nameEdit->text(), playerId)
                                  .arg(balance, 0, 'f', 0));
    m_playerInfoLabel->setStyleSheet("color: #facc15; font-size: 12px; font-weight: bold;");

    m_refreshBtn->setEnabled(true);
    m_createTableBtn->setEnabled(true);

    updateStatus("Logged in successfully", "#10eb04");

    // Request table list
    refreshTables();
    m_refreshTimer->start();
}

void LobbyWidget::onTableListReceived(const QJsonArray& tables)
{
    m_tableList->setRowCount(0);

    for (int i = 0; i < tables.size(); ++i) {
        QJsonObject t = tables[i].toObject();
        int row = m_tableList->rowCount();
        m_tableList->insertRow(row);

        // Table name
        auto* nameItem = new QTableWidgetItem(t["name"].toString());
        nameItem->setData(Qt::UserRole, t["id"].toString()); // store table ID
        m_tableList->setItem(row, 0, nameItem);

        // Blinds
        auto* blindsItem = new QTableWidgetItem(
            QString("%1/%2").arg(t["small_blind"].toDouble(), 0, 'f', 0)
                .arg(t["big_blind"].toDouble(), 0, 'f', 0));
        blindsItem->setTextAlignment(Qt::AlignCenter);
        m_tableList->setItem(row, 1, blindsItem);

        // Players
        auto* playersItem = new QTableWidgetItem(
            QString("%1/%2").arg(t["players"].toInt()).arg(t["max_seats"].toInt()));
        playersItem->setTextAlignment(Qt::AlignCenter);
        m_tableList->setItem(row, 2, playersItem);

        // Avg pot
        auto* potItem = new QTableWidgetItem(
            QString("$%1").arg(t["avg_pot"].toDouble(), 0, 'f', 0));
        potItem->setTextAlignment(Qt::AlignCenter);
        m_tableList->setItem(row, 3, potItem);

        // Buy-in range
        auto* buyInItem = new QTableWidgetItem(
            QString("$%1-$%2").arg(t["min_buy_in"].toDouble(), 0, 'f', 0)
                .arg(t["max_buy_in"].toDouble(), 0, 'f', 0));
        buyInItem->setTextAlignment(Qt::AlignCenter);
        m_tableList->setItem(row, 4, buyInItem);

        // JOIN button
        auto* joinBtn = new QPushButton("JOIN");
        joinBtn->setFixedSize(80, 28);
        joinBtn->setStyleSheet(
            "QPushButton { background: rgba(250,204,21,0.2); color: #facc15; "
            "border: 1px solid rgba(250,204,21,0.4); border-radius: 4px; "
            "font-weight: 800; font-size: 11px; }"
            "QPushButton:hover { background: rgba(250,204,21,0.4); }");

        QString tableId = t["id"].toString();
        double minBuy = t["min_buy_in"].toDouble();
        double maxBuy = t["max_buy_in"].toDouble();
        int players = t["players"].toInt();
        int maxSeats = t["max_seats"].toInt();

        if (players >= maxSeats) {
            joinBtn->setText("FULL");
            joinBtn->setEnabled(false);
            joinBtn->setStyleSheet(
                "QPushButton { background: #333; color: #666; border: 1px solid #444; "
                "border-radius: 4px; font-weight: 800; font-size: 11px; }");
        }

        connect(joinBtn, &QPushButton::clicked, this, [this, tableId, minBuy, maxBuy]() {
            // Use max buy-in up to balance
            double buyIn = qMin(m_client->balance(), maxBuy);
            if (buyIn < minBuy) {
                updateStatus("Not enough balance for minimum buy-in!", "#eb0404");
                return;
            }
            m_client->joinTable(tableId, buyIn);
            updateStatus(QString("Joining table %1 with $%2...").arg(tableId).arg(buyIn, 0, 'f', 0), "#facc15");
        });

        m_tableList->setCellWidget(row, 5, joinBtn);
    }

    updateStatus(QString("%1 table(s) available").arg(tables.size()), "#aaa");
}

void LobbyWidget::onTableJoined(const QString& tableId, int seat, const QJsonObject& state)
{
    m_refreshTimer->stop();
    updateStatus(QString("Joined table %1 at seat %2").arg(tableId).arg(seat), "#10eb04");
    emit joinedTable(tableId, seat, state);
}

void LobbyWidget::onError(const QString& message)
{
    updateStatus(QString("Error: %1").arg(message), "#eb0404");
}

void LobbyWidget::refreshTables()
{
    if (m_client->isConnected() && m_client->isLoggedIn()) {
        m_client->requestTableList();
    }
}

void LobbyWidget::showCreateTableDialog()
{
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle("Create New Table");
    dialog->setFixedSize(360, 320);
    dialog->setStyleSheet(
        "QDialog { background: #1a1f2e; color: #fff; }"
        "QLabel { color: #aaa; font-weight: bold; font-size: 12px; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox { background: rgba(0,0,0,0.4); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.3); border-radius: 4px; padding: 6px; font-size: 12px; }");

    auto* form = new QFormLayout(dialog);
    form->setSpacing(10);
    form->setContentsMargins(20, 20, 20, 20);

    auto* nameEdit = new QLineEdit("My Table");
    form->addRow("Table Name:", nameEdit);

    auto* sbSpin = new QDoubleSpinBox;
    sbSpin->setRange(1, 10000);
    sbSpin->setValue(5);
    sbSpin->setDecimals(0);
    form->addRow("Small Blind:", sbSpin);

    auto* bbSpin = new QDoubleSpinBox;
    bbSpin->setRange(2, 20000);
    bbSpin->setValue(10);
    bbSpin->setDecimals(0);
    form->addRow("Big Blind:", bbSpin);

    auto* minBuySpin = new QDoubleSpinBox;
    minBuySpin->setRange(10, 100000);
    minBuySpin->setValue(200);
    minBuySpin->setDecimals(0);
    form->addRow("Min Buy-In:", minBuySpin);

    auto* maxBuySpin = new QDoubleSpinBox;
    maxBuySpin->setRange(100, 1000000);
    maxBuySpin->setValue(2000);
    maxBuySpin->setDecimals(0);
    form->addRow("Max Buy-In:", maxBuySpin);

    auto* seatsSpin = new QSpinBox;
    seatsSpin->setRange(2, 9);
    seatsSpin->setValue(9);
    form->addRow("Max Seats:", seatsSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->setStyleSheet(
        "QPushButton { background: rgba(250,204,21,0.2); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.4); border-radius: 4px; "
        "padding: 8px 20px; font-weight: 800; font-size: 12px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.4); }");
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    if (dialog->exec() == QDialog::Accepted) {
        m_client->createTable(
            nameEdit->text(),
            sbSpin->value(),
            bbSpin->value(),
            minBuySpin->value(),
            maxBuySpin->value(),
            seatsSpin->value()
        );
        updateStatus("Creating table...", "#facc15");
    }

    dialog->deleteLater();
}

void LobbyWidget::joinSelectedTable()
{
    int row = m_tableList->currentRow();
    if (row < 0) return;

    auto* item = m_tableList->item(row, 0);
    if (!item) return;

    QString tableId = item->data(Qt::UserRole).toString();
    // This is handled by the JOIN button per-row
}

void LobbyWidget::updateStatus(const QString& text, const QString& color)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(color));
}
