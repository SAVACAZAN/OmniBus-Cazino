#include "P2PLobbyWidget.h"
#include "p2p/P2PDiscovery.h"
#include "p2p/GameCode.h"
#include "p2p/MentalPoker.h"
#include "p2p/ReputationSystem.h"
#include <QScrollArea>
#include <QRandomGenerator>
#include <QHeaderView>
#include <QFrame>
#include <QNetworkInterface>
#include <QDateTime>
#include <QMessageBox>

// --- Style constants ---
static const char* GOLD_GROUP_STYLE =
    "QGroupBox {"
    "  background: rgba(20,26,36,0.9);"
    "  border: 1px solid rgba(250,204,21,0.2);"
    "  border-radius: 10px;"
    "  margin-top: 14px;"
    "  padding: 16px;"
    "  color: #facc15;"
    "  font-size: 13px;"
    "  font-weight: bold;"
    "}"
    "QGroupBox::title {"
    "  subcontrol-origin: margin;"
    "  left: 12px;"
    "  padding: 0 6px;"
    "  color: #facc15;"
    "}";

static const char* INPUT_STYLE =
    "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {"
    "  background: rgba(15,20,25,0.95);"
    "  color: #e0e0e0;"
    "  border: 1px solid rgba(250,204,21,0.25);"
    "  border-radius: 6px;"
    "  padding: 6px 10px;"
    "  font-size: 12px;"
    "}"
    "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {"
    "  border-color: #facc15;"
    "}";

static const char* GOLD_BTN_STYLE =
    "QPushButton {"
    "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #facc15, stop:1 #d4a50a);"
    "  color: #0f1419;"
    "  border: none;"
    "  border-radius: 6px;"
    "  padding: 10px 20px;"
    "  font-weight: 800;"
    "  font-size: 13px;"
    "}"
    "QPushButton:hover {"
    "  background: #facc15;"
    "}"
    "QPushButton:disabled {"
    "  background: rgba(250,204,21,0.3);"
    "  color: rgba(15,20,25,0.5);"
    "}";

static const char* RED_BTN_STYLE =
    "QPushButton {"
    "  background: rgba(235,4,4,0.2);"
    "  color: #eb4444;"
    "  border: 1px solid rgba(235,4,4,0.4);"
    "  border-radius: 6px;"
    "  padding: 8px 16px;"
    "  font-weight: 700;"
    "  font-size: 12px;"
    "}"
    "QPushButton:hover { background: rgba(235,4,4,0.35); }";

static const char* GREEN_BTN_STYLE =
    "QPushButton {"
    "  background: rgba(16,235,4,0.15);"
    "  color: #10eb04;"
    "  border: 1px solid rgba(16,235,4,0.3);"
    "  border-radius: 6px;"
    "  padding: 8px 16px;"
    "  font-weight: 700;"
    "  font-size: 12px;"
    "}"
    "QPushButton:hover { background: rgba(16,235,4,0.25); }";

static const char* TABLE_STYLE =
    "QTableWidget {"
    "  background: rgba(15,20,25,0.95);"
    "  color: #e0e0e0;"
    "  border: 1px solid rgba(250,204,21,0.15);"
    "  border-radius: 6px;"
    "  gridline-color: rgba(250,204,21,0.1);"
    "  font-size: 11px;"
    "}"
    "QTableWidget::item { padding: 4px 8px; }"
    "QTableWidget::item:selected {"
    "  background: rgba(250,204,21,0.15);"
    "  color: #facc15;"
    "}"
    "QHeaderView::section {"
    "  background: rgba(250,204,21,0.08);"
    "  color: #facc15;"
    "  border: 1px solid rgba(250,204,21,0.1);"
    "  padding: 6px;"
    "  font-weight: bold;"
    "  font-size: 11px;"
    "}";

// --- Constructor ---

P2PLobbyWidget::P2PLobbyWidget(P2PDiscovery* discovery,
                                MentalPoker* mentalPoker,
                                ReputationSystem* reputation,
                                QWidget* parent)
    : QWidget(parent)
    , m_discovery(discovery)
    , m_mentalPoker(mentalPoker)
    , m_reputation(reputation)
{
    setupUI();

    // Connect discovery signals
    if (m_discovery) {
        connect(m_discovery, &P2PDiscovery::gameHosted,
                this, [this](const QString& code) {
            updateGameCodeDisplay(code);
            addLogMessage("Game hosted: " + code, "#10eb04");
            emit hostedGame(code);
        });
        connect(m_discovery, &P2PDiscovery::gameFound,
                this, &P2PLobbyWidget::onGameFound);
        connect(m_discovery, &P2PDiscovery::lanScanComplete,
                this, &P2PLobbyWidget::onLanScanComplete);
        connect(m_discovery, &P2PDiscovery::connectionEstablished,
                this, &P2PLobbyWidget::onConnectionEstablished);
        connect(m_discovery, &P2PDiscovery::connectionFailed,
                this, &P2PLobbyWidget::onConnectionFailed);
        connect(m_discovery, &P2PDiscovery::playerJoined,
                this, &P2PLobbyWidget::onPlayerJoined);
        connect(m_discovery, &P2PDiscovery::playerLeft,
                this, &P2PLobbyWidget::onPlayerLeft);
    }
}

// --- UI Setup ---

void P2PLobbyWidget::setupUI()
{
    setStyleSheet("background: #0f1419;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 16, 24, 16);
    mainLayout->setSpacing(12);

    // Title
    auto* title = new QLabel("OMNIBUS P2P POKER");
    title->setStyleSheet(
        "color: #facc15; font-size: 26px; font-weight: 900; "
        "letter-spacing: 2px; background: transparent;");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    auto* subtitle = new QLabel("Decentralized | Mental Poker Protocol | No Central Server");
    subtitle->setStyleSheet("color: rgba(250,204,21,0.6); font-size: 12px; background: transparent;");
    subtitle->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtitle);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background: rgba(250,204,21,0.2); max-height: 1px;");
    mainLayout->addWidget(sep);

    // Player info bar
    mainLayout->addWidget(createPlayerInfoBar());

    // Main content: two columns
    auto* columns = new QHBoxLayout;
    columns->setSpacing(16);

    // Left column: HOST + Players
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(12);
    leftCol->addWidget(createHostSection());
    leftCol->addWidget(createPlayersSection());
    leftCol->addStretch();
    columns->addLayout(leftCol, 1);

    // Right column: JOIN + LAN Games + Log
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(12);
    rightCol->addWidget(createJoinSection());
    rightCol->addWidget(createLANGamesSection());

    // Log area
    m_logArea = new QTextEdit;
    m_logArea->setReadOnly(true);
    m_logArea->setMaximumHeight(120);
    m_logArea->setStyleSheet(
        "QTextEdit {"
        "  background: rgba(10,14,20,0.9);"
        "  color: #aaa;"
        "  border: 1px solid rgba(250,204,21,0.1);"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  font-family: 'Consolas', monospace;"
        "  font-size: 11px;"
        "}");
    rightCol->addWidget(m_logArea);
    rightCol->addStretch();

    columns->addLayout(rightCol, 1);

    mainLayout->addLayout(columns, 1);

    // Apply input styles globally to this widget
    setStyleSheet(styleSheet() + INPUT_STYLE);

    addLogMessage("P2P Poker ready. Host a game or join one!", "#facc15");
}

QWidget* P2PLobbyWidget::createPlayerInfoBar()
{
    auto* bar = new QWidget;
    bar->setStyleSheet(
        "background: rgba(20,26,36,0.8); border: 1px solid rgba(250,204,21,0.15); "
        "border-radius: 8px; padding: 8px;");

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(12);

    auto* nameLabel = new QLabel("Player Name:");
    nameLabel->setStyleSheet("color: #aaa; font-size: 12px; background: transparent;");
    layout->addWidget(nameLabel);

    m_playerNameEdit = new QLineEdit("Player_" + QString::number(QRandomGenerator::global()->bounded(9999)));
    m_playerNameEdit->setFixedWidth(160);
    layout->addWidget(m_playerNameEdit);

    layout->addSpacing(20);

    auto* repLabel = new QLabel("Reputation:");
    repLabel->setStyleSheet("color: #aaa; font-size: 12px; background: transparent;");
    layout->addWidget(repLabel);

    m_reputationLabel = new QLabel("100.0 (Trusted)");
    m_reputationLabel->setStyleSheet("color: #10eb04; font-size: 13px; font-weight: bold; background: transparent;");
    layout->addWidget(m_reputationLabel);

    layout->addStretch();

    return bar;
}

QGroupBox* P2PLobbyWidget::createHostSection()
{
    auto* group = new QGroupBox("HOST A GAME");
    group->setStyleSheet(GOLD_GROUP_STYLE);

    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    // Table name
    auto* nameRow = new QHBoxLayout;
    auto* nameLabel = new QLabel("Table Name:");
    nameLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    nameRow->addWidget(nameLabel);
    m_tableNameEdit = new QLineEdit("OmniBus Poker");
    nameRow->addWidget(m_tableNameEdit);
    layout->addLayout(nameRow);

    // Blinds row
    auto* blindsRow = new QHBoxLayout;
    auto* sbLabel = new QLabel("Small Blind:");
    sbLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    blindsRow->addWidget(sbLabel);
    m_smallBlindSpin = new QDoubleSpinBox;
    m_smallBlindSpin->setRange(0.01, 10000.0);
    m_smallBlindSpin->setValue(1.0);
    m_smallBlindSpin->setDecimals(2);
    blindsRow->addWidget(m_smallBlindSpin);

    auto* bbLabel = new QLabel("Big Blind:");
    bbLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    blindsRow->addWidget(bbLabel);
    m_bigBlindSpin = new QDoubleSpinBox;
    m_bigBlindSpin->setRange(0.02, 20000.0);
    m_bigBlindSpin->setValue(2.0);
    m_bigBlindSpin->setDecimals(2);
    blindsRow->addWidget(m_bigBlindSpin);
    layout->addLayout(blindsRow);

    // Buy-in row
    auto* buyInRow = new QHBoxLayout;
    auto* minLabel = new QLabel("Min Buy-In:");
    minLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    buyInRow->addWidget(minLabel);
    m_minBuyInSpin = new QDoubleSpinBox;
    m_minBuyInSpin->setRange(1.0, 100000.0);
    m_minBuyInSpin->setValue(100.0);
    m_minBuyInSpin->setDecimals(2);
    buyInRow->addWidget(m_minBuyInSpin);

    auto* maxLabel = new QLabel("Max Buy-In:");
    maxLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    buyInRow->addWidget(maxLabel);
    m_maxBuyInSpin = new QDoubleSpinBox;
    m_maxBuyInSpin->setRange(10.0, 1000000.0);
    m_maxBuyInSpin->setValue(1000.0);
    m_maxBuyInSpin->setDecimals(2);
    buyInRow->addWidget(m_maxBuyInSpin);
    layout->addLayout(buyInRow);

    // Seats + currency row
    auto* seatsRow = new QHBoxLayout;
    auto* seatsLabel = new QLabel("Max Seats:");
    seatsLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    seatsRow->addWidget(seatsLabel);
    m_maxSeatsSpin = new QSpinBox;
    m_maxSeatsSpin->setRange(2, 9);
    m_maxSeatsSpin->setValue(9);
    seatsRow->addWidget(m_maxSeatsSpin);

    auto* currLabel = new QLabel("Currency:");
    currLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    seatsRow->addWidget(currLabel);
    m_currencyCombo = new QComboBox;
    m_currencyCombo->addItems({"EUR", "BTC", "ETH", "LCX", "USDT", "OMNI"});
    m_currencyCombo->setStyleSheet(
        "QComboBox { background: rgba(15,20,25,0.95); color: #e0e0e0; "
        "border: 1px solid rgba(250,204,21,0.25); border-radius: 6px; padding: 6px 10px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #0f1419; color: #e0e0e0; "
        "selection-background-color: rgba(250,204,21,0.2); }");
    seatsRow->addWidget(m_currencyCombo);
    layout->addLayout(seatsRow);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    m_hostBtn = new QPushButton("HOST GAME");
    m_hostBtn->setStyleSheet(GOLD_BTN_STYLE);
    connect(m_hostBtn, &QPushButton::clicked, this, &P2PLobbyWidget::onHostClicked);
    btnRow->addWidget(m_hostBtn);

    m_stopHostBtn = new QPushButton("STOP HOSTING");
    m_stopHostBtn->setStyleSheet(RED_BTN_STYLE);
    m_stopHostBtn->setVisible(false);
    connect(m_stopHostBtn, &QPushButton::clicked, this, &P2PLobbyWidget::onStopHostClicked);
    btnRow->addWidget(m_stopHostBtn);
    layout->addLayout(btnRow);

    // Game code display
    m_gameCodeLabel = new QLabel("Your Game Code:");
    m_gameCodeLabel->setStyleSheet("color: #aaa; font-size: 12px; background: transparent;");
    m_gameCodeLabel->setVisible(false);
    layout->addWidget(m_gameCodeLabel);

    auto* codeRow = new QHBoxLayout;
    m_gameCodeValue = new QLabel("---");
    m_gameCodeValue->setStyleSheet(
        "color: #facc15; font-size: 22px; font-weight: 900; "
        "letter-spacing: 3px; background: rgba(250,204,21,0.08); "
        "border: 2px solid rgba(250,204,21,0.3); border-radius: 8px; "
        "padding: 10px 20px;");
    m_gameCodeValue->setAlignment(Qt::AlignCenter);
    m_gameCodeValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_gameCodeValue->setVisible(false);
    codeRow->addWidget(m_gameCodeValue, 1);

    m_copyCodeBtn = new QPushButton("COPY");
    m_copyCodeBtn->setStyleSheet(GREEN_BTN_STYLE);
    m_copyCodeBtn->setFixedWidth(80);
    m_copyCodeBtn->setVisible(false);
    connect(m_copyCodeBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_gameCodeValue->text());
        addLogMessage("Game code copied to clipboard!", "#10eb04");
    });
    codeRow->addWidget(m_copyCodeBtn);
    layout->addLayout(codeRow);

    auto* shareLabel = new QLabel("Share this code with friends to join your game!");
    shareLabel->setStyleSheet("color: rgba(250,204,21,0.5); font-size: 11px; "
                               "font-style: italic; background: transparent;");
    shareLabel->setAlignment(Qt::AlignCenter);
    shareLabel->setObjectName("shareLabel");
    shareLabel->setVisible(false);
    layout->addWidget(shareLabel);

    return group;
}

QGroupBox* P2PLobbyWidget::createJoinSection()
{
    auto* group = new QGroupBox("JOIN A GAME");
    group->setStyleSheet(GOLD_GROUP_STYLE);

    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    // Join by code
    auto* codeLabel = new QLabel("Enter Game Code:");
    codeLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    layout->addWidget(codeLabel);

    auto* codeRow = new QHBoxLayout;
    m_joinCodeEdit = new QLineEdit;
    m_joinCodeEdit->setPlaceholderText("OMNI-XXXXXXXX");
    m_joinCodeEdit->setStyleSheet(
        "QLineEdit { background: rgba(15,20,25,0.95); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.3); border-radius: 6px; "
        "padding: 8px 12px; font-size: 14px; font-weight: bold; letter-spacing: 2px; }");
    codeRow->addWidget(m_joinCodeEdit, 1);

    m_joinBtn = new QPushButton("JOIN");
    m_joinBtn->setStyleSheet(GOLD_BTN_STYLE);
    m_joinBtn->setFixedWidth(100);
    connect(m_joinBtn, &QPushButton::clicked, this, &P2PLobbyWidget::onJoinByCodeClicked);
    codeRow->addWidget(m_joinBtn);
    layout->addLayout(codeRow);

    // Separator
    auto* orLabel = new QLabel("--- OR ---");
    orLabel->setStyleSheet("color: rgba(255,255,255,0.3); font-size: 11px; background: transparent;");
    orLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(orLabel);

    // Direct IP
    auto* ipLabel = new QLabel("Direct IP Connection:");
    ipLabel->setStyleSheet("color: #ccc; font-size: 12px; background: transparent;");
    layout->addWidget(ipLabel);

    auto* ipRow = new QHBoxLayout;
    m_directIPEdit = new QLineEdit;
    m_directIPEdit->setPlaceholderText("192.168.1.100");
    ipRow->addWidget(m_directIPEdit, 1);

    auto* colonLabel = new QLabel(":");
    colonLabel->setStyleSheet("color: #facc15; font-size: 14px; font-weight: bold; background: transparent;");
    ipRow->addWidget(colonLabel);

    m_directPortEdit = new QLineEdit("9999");
    m_directPortEdit->setFixedWidth(70);
    ipRow->addWidget(m_directPortEdit);

    m_joinIPBtn = new QPushButton("CONNECT");
    m_joinIPBtn->setStyleSheet(GREEN_BTN_STYLE);
    m_joinIPBtn->setFixedWidth(100);
    connect(m_joinIPBtn, &QPushButton::clicked, this, &P2PLobbyWidget::onJoinByIPClicked);
    ipRow->addWidget(m_joinIPBtn);
    layout->addLayout(ipRow);

    // LAN scan button
    auto* scanRow = new QHBoxLayout;
    scanRow->addStretch();
    m_scanLANBtn = new QPushButton("SCAN LAN");
    m_scanLANBtn->setStyleSheet(
        "QPushButton {"
        "  background: rgba(100,100,255,0.15);"
        "  color: #8888ff;"
        "  border: 1px solid rgba(100,100,255,0.3);"
        "  border-radius: 6px;"
        "  padding: 8px 20px;"
        "  font-weight: 700;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background: rgba(100,100,255,0.25); }");
    connect(m_scanLANBtn, &QPushButton::clicked, this, &P2PLobbyWidget::onScanLANClicked);
    scanRow->addWidget(m_scanLANBtn);
    scanRow->addStretch();
    layout->addLayout(scanRow);

    return group;
}

QGroupBox* P2PLobbyWidget::createLANGamesSection()
{
    auto* group = new QGroupBox("LAN GAMES FOUND");
    group->setStyleSheet(GOLD_GROUP_STYLE);

    auto* layout = new QVBoxLayout(group);

    m_lanTable = new QTableWidget(0, 7);
    m_lanTable->setHorizontalHeaderLabels(
        {"Code", "Host", "Blinds", "Buy-In", "Seats", "Currency", "Join"});
    m_lanTable->setStyleSheet(TABLE_STYLE);
    m_lanTable->horizontalHeader()->setStretchLastSection(true);
    m_lanTable->verticalHeader()->setVisible(false);
    m_lanTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_lanTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lanTable->setMinimumHeight(120);
    m_lanTable->setMaximumHeight(180);
    layout->addWidget(m_lanTable);

    return group;
}

QGroupBox* P2PLobbyWidget::createPlayersSection()
{
    auto* group = new QGroupBox("CONNECTED PLAYERS");
    group->setStyleSheet(GOLD_GROUP_STYLE);

    auto* layout = new QVBoxLayout(group);

    m_playersTable = new QTableWidget(0, 4);
    m_playersTable->setHorizontalHeaderLabels(
        {"Player", "Reputation", "Status", "Hands"});
    m_playersTable->setStyleSheet(TABLE_STYLE);
    m_playersTable->horizontalHeader()->setStretchLastSection(true);
    m_playersTable->verticalHeader()->setVisible(false);
    m_playersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_playersTable->setMinimumHeight(100);
    m_playersTable->setMaximumHeight(200);
    layout->addWidget(m_playersTable);

    return group;
}

// --- Slots ---

void P2PLobbyWidget::onHostClicked()
{
    if (!m_discovery) return;

    P2PGame config;
    config.hostName       = m_playerNameEdit->text();
    config.smallBlind     = m_smallBlindSpin->value();
    config.bigBlind       = m_bigBlindSpin->value();
    config.minBuyIn       = m_minBuyInSpin->value();
    config.maxBuyIn       = m_maxBuyInSpin->value();
    config.maxSeats       = m_maxSeatsSpin->value();
    config.currency       = m_currencyCombo->currentText();

    // DON'T start P2PDiscovery server — PokerServer already runs on 9999
    // Just generate a game code for sharing and emit hostedGame signal
    QString localIp = "localhost";
    const auto interfaces = QNetworkInterface::allAddresses();
    for (const auto& addr : interfaces) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol &&
            !addr.isLoopback() && addr.toString().startsWith("192.168")) {
            localIp = addr.toString();
            break;
        }
    }

    QString code = GameCode::encode(localIp, 9999);
    if (code.isEmpty()) code = "OMNI-LOCAL";

    updateGameCodeDisplay(code);
    addLogMessage("Game hosted! Code: " + code, "#10eb04");
    addLogMessage("Share this code with friends!", "#facc15");

    m_hostBtn->setVisible(false);
    m_stopHostBtn->setVisible(true);

    // Emit signal — MainWindow will connect PokerClient to localhost:9999
    emit hostedGame(code);
}

void P2PLobbyWidget::onStopHostClicked()
{
    if (!m_discovery) return;

    m_discovery->stopHosting();
    m_hostBtn->setVisible(true);
    m_stopHostBtn->setVisible(false);

    m_gameCodeLabel->setVisible(false);
    m_gameCodeValue->setVisible(false);
    m_copyCodeBtn->setVisible(false);
    findChild<QLabel*>("shareLabel")->setVisible(false);

    m_playersTable->setRowCount(0);

    addLogMessage("Stopped hosting.", "#eb4444");
}

void P2PLobbyWidget::onJoinByCodeClicked()
{
    if (!m_discovery) return;

    QString code = m_joinCodeEdit->text().trimmed().toUpper();
    if (code.isEmpty()) {
        addLogMessage("Please enter a game code.", "#eb4444");
        return;
    }

    addLogMessage("Connecting to " + code + "...", "#8888ff");
    // Don't use P2PDiscovery's own WebSocket — emit signal directly
    // MainWindow will connect PokerClient to the decoded IP:port
    emit joinedGame(code);
}

void P2PLobbyWidget::onJoinByIPClicked()
{
    if (!m_discovery) return;

    QString ip = m_directIPEdit->text().trimmed();
    quint16 port = static_cast<quint16>(m_directPortEdit->text().toUInt());

    if (ip.isEmpty() || port == 0) {
        addLogMessage("Please enter valid IP and port.", "#eb4444");
        return;
    }

    addLogMessage(QString("Connecting to %1:%2...").arg(ip).arg(port), "#8888ff");
    // Encode IP:port as game code and emit directly
    // MainWindow will connect PokerClient
    QString code = GameCode::encode(ip, port);
    if (code.isEmpty()) code = QString("DIRECT-%1-%2").arg(ip).arg(port);
    emit joinedGame(code);
}

void P2PLobbyWidget::onScanLANClicked()
{
    if (!m_discovery) return;

    addLogMessage("Scanning LAN for games...", "#8888ff");
    m_scanLANBtn->setEnabled(false);
    m_scanLANBtn->setText("Scanning...");

    m_discovery->scanLAN();

    // Re-enable after scan completes
    QTimer::singleShot(3000, this, [this]() {
        m_scanLANBtn->setEnabled(true);
        m_scanLANBtn->setText("SCAN LAN");
    });
}

void P2PLobbyWidget::onGameFound(const P2PGame& game)
{
    // Add to LAN table if not already there
    for (int r = 0; r < m_lanTable->rowCount(); ++r) {
        if (m_lanTable->item(r, 0) &&
            m_lanTable->item(r, 0)->text() == game.gameCode)
            return;  // Already listed
    }

    int row = m_lanTable->rowCount();
    m_lanTable->insertRow(row);
    m_lanTable->setItem(row, 0, new QTableWidgetItem(game.gameCode));
    m_lanTable->setItem(row, 1, new QTableWidgetItem(game.hostName));
    m_lanTable->setItem(row, 2, new QTableWidgetItem(
        QString("%1/%2").arg(game.smallBlind, 0, 'f', 2).arg(game.bigBlind, 0, 'f', 2)));
    m_lanTable->setItem(row, 3, new QTableWidgetItem(
        QString("%1-%2").arg(game.minBuyIn, 0, 'f', 0).arg(game.maxBuyIn, 0, 'f', 0)));
    m_lanTable->setItem(row, 4, new QTableWidgetItem(
        QString("%1/%2").arg(game.currentPlayers).arg(game.maxSeats)));
    m_lanTable->setItem(row, 5, new QTableWidgetItem(game.currency));

    // Join button
    auto* joinBtn = new QPushButton("JOIN");
    joinBtn->setStyleSheet(
        "QPushButton { background: rgba(16,235,4,0.2); color: #10eb04; "
        "border: 1px solid rgba(16,235,4,0.3); border-radius: 4px; "
        "padding: 4px 12px; font-weight: 700; font-size: 11px; }"
        "QPushButton:hover { background: rgba(16,235,4,0.35); }");
    connect(joinBtn, &QPushButton::clicked, this, [this, gameCode = game.gameCode]() {
        m_joinCodeEdit->setText(gameCode);
        onJoinByCodeClicked();
    });
    m_lanTable->setCellWidget(row, 6, joinBtn);

    addLogMessage("Found game: " + game.hostName + " (" + game.gameCode + ")", "#10eb04");
}

void P2PLobbyWidget::onLanScanComplete(const QVector<P2PGame>& games)
{
    if (games.isEmpty()) {
        addLogMessage("No games found on LAN.", "#aaa");
    } else {
        addLogMessage(QString("Found %1 game(s) on LAN.").arg(games.size()), "#10eb04");
    }
}

void P2PLobbyWidget::onConnectionEstablished(const QString& gameCode)
{
    addLogMessage("Connected to game: " + gameCode, "#10eb04");
    emit joinedGame(gameCode);
}

void P2PLobbyWidget::onConnectionFailed(const QString& reason)
{
    addLogMessage("Connection failed: " + reason, "#eb4444");
}

void P2PLobbyWidget::onPlayerJoined(const QString& name)
{
    int row = m_playersTable->rowCount();
    m_playersTable->insertRow(row);
    m_playersTable->setItem(row, 0, new QTableWidgetItem(name));

    double score = m_reputation ? m_reputation->fairPlayScore(name) : 100.0;
    auto* scoreItem = new QTableWidgetItem(QString::number(score, 'f', 1));
    if (score > 70.0)
        scoreItem->setForeground(QColor("#10eb04"));
    else if (score > 40.0)
        scoreItem->setForeground(QColor("#facc15"));
    else
        scoreItem->setForeground(QColor("#eb4444"));
    m_playersTable->setItem(row, 1, scoreItem);

    bool trusted = m_reputation ? m_reputation->isTrusted(name) : true;
    m_playersTable->setItem(row, 2, new QTableWidgetItem(trusted ? "Trusted" : "Unverified"));
    m_playersTable->setItem(row, 3, new QTableWidgetItem("0"));

    addLogMessage(name + " joined the game.", "#10eb04");
}

void P2PLobbyWidget::onPlayerLeft(const QString& name)
{
    for (int r = 0; r < m_playersTable->rowCount(); ++r) {
        if (m_playersTable->item(r, 0) &&
            m_playersTable->item(r, 0)->text().contains(name)) {
            m_playersTable->removeRow(r);
            break;
        }
    }
    addLogMessage(name + " left the game.", "#eb4444");
}

void P2PLobbyWidget::addLogMessage(const QString& msg, const QString& color)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_logArea->append(QString("<span style='color:%1'>[%2] %3</span>")
                       .arg(color, time, msg));
}

void P2PLobbyWidget::updateGameCodeDisplay(const QString& code)
{
    m_gameCodeLabel->setVisible(true);
    m_gameCodeValue->setText(code);
    m_gameCodeValue->setVisible(true);
    m_copyCodeBtn->setVisible(true);

    auto* shareLabel = findChild<QLabel*>("shareLabel");
    if (shareLabel) shareLabel->setVisible(true);
}

void P2PLobbyWidget::updateLANTable(const QVector<P2PGame>& games)
{
    m_lanTable->setRowCount(0);
    for (const auto& game : games) {
        onGameFound(game);
    }
}
