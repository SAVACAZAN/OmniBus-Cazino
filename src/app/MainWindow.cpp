#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QStatusBar>
#include <QGridLayout>
#include <QFrame>
#include <QApplication>
#include <QTimer>
#include <QFile>

// ── OmniBus Cloud Server ──
// Players auto-connect to this VPS server for multiplayer
static const QString OMNIBUS_CLOUD_SERVER = "ws://193.219.97.147:9999";

// Games
#include "games/DiceGame.h"
#include "games/CrashGame.h"
#include "games/CoinFlipGame.h"
#include "games/BlackjackGame.h"
#include "games/PokerGame.h"
#include "games/PokerRoom.h"
#include "games/SlotsGame.h"
#include "games/SportsBettingGame.h"
#include "games/BinaryOptionsGame.h"
#include "games/RaceBettingGame.h"
#include "games/AuctionGame.h"

// Multiplayer poker
#include "server/PokerServer.h"
#include "network/PokerClient.h"
#include "ui/LobbyWidget.h"

// P2P Decentralized poker
#include "p2p/GameCode.h"
#include "p2p/P2PDiscovery.h"
#include "p2p/MentalPoker.h"
#include "p2p/ReputationSystem.h"
#include "ui/P2PLobbyWidget.h"

// Tournament system
#include "ui/TournamentLobbyWidget.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_engine = new BettingEngine(this);
    m_engine->loadConfig("config.json");

    // Account system
    m_accountSystem = new AccountSystem(this);

    // Create poker server and client
    m_pokerServer = new PokerServer(9999, this);
    m_pokerClient = new PokerClient(this);

    // Create P2P systems
    m_p2pDiscovery = new P2PDiscovery(this);
    m_mentalPoker = new MentalPoker();
    m_reputationSystem = new ReputationSystem(this);
    m_reputationSystem->load("reputation.json");

    setWindowTitle("OmniBus Casino v1.0");
    setMinimumSize(1200, 800);
    resize(1400, 900);

    // Root stack: Login (0) → Casino (1)
    m_rootStack = new QStackedWidget;
    setCentralWidget(m_rootStack);

    // Login screen
    m_loginWidget = new LoginWidget(m_accountSystem);
    m_rootStack->addWidget(m_loginWidget); // index 0

    // Casino UI (built but hidden until login)
    m_casinoWidget = new QWidget;
    auto* casinoLayout = new QVBoxLayout(m_casinoWidget);
    casinoLayout->setContentsMargins(0, 0, 0, 0);
    casinoLayout->setSpacing(0);

    // Build casino inside casinoWidget
    auto* topBar = createTopBar();
    casinoLayout->addWidget(topBar);

    auto* bodyLayout = new QHBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    createSidebar();
    bodyLayout->addWidget(m_sidebar);
    createContent();
    bodyLayout->addWidget(m_stack, 1);
    casinoLayout->addLayout(bodyLayout, 1);

    m_rootStack->addWidget(m_casinoWidget); // index 1

    createStatusBar();

    // Show login first
    m_rootStack->setCurrentIndex(0);

    // When login succeeds → show casino
    connect(m_loginWidget, &LoginWidget::loginSuccess, this, [this](const QString& uid, const QString& username) {
        Q_UNUSED(uid);
        showCasino();

        // Auto-connect to OmniBus Cloud Server for multiplayer
        if (!m_pokerClient->isConnected()) {
            m_pokerClient->connectToServer(OMNIBUS_CLOUD_SERVER);
            // Auto-login with casino username and balance
            connect(m_pokerClient, &PokerClient::connected, this, [this, username]() {
                m_pokerClient->login(username, m_engine->getPlayer().balance("EUR"));
                statusBar()->showMessage("Connected to OmniBus Cloud Server!", 5000);
            }, Qt::SingleShotConnection);
        }
    });

    connect(m_engine, &BettingEngine::balanceChanged, this, [this]() {
        updateBalanceDisplay();
    });

    // Auto-start poker server — try SSL first, fallback to plain WS
    if (QFile::exists("cert.pem") && QFile::exists("key.pem")) {
        m_pokerServer->startSecure("cert.pem", "key.pem");
    } else {
        m_pokerServer->start();
    }
    updateServerStatus();
}

MainWindow::~MainWindow()
{
    m_pokerServer->stop();
    m_pokerClient->disconnect();
    m_p2pDiscovery->stopHosting();
    m_reputationSystem->save("reputation.json");
    m_engine->saveState();
    delete m_mentalPoker;
}

void MainWindow::setupUI()
{
    // Setup is now done in constructor (rootStack + login + casino)
}

void MainWindow::showLoginScreen()
{
    m_rootStack->setCurrentIndex(0);
}

void MainWindow::showCasino()
{
    m_rootStack->setCurrentIndex(1);
    updateBalanceDisplay();

    // Show welcome with player name
    if (m_playerNameLabel && m_accountSystem->isLoggedIn()) {
        QString name = m_accountSystem->displayName();
        QString avatar = m_accountSystem->avatarEmoji();
        m_playerNameLabel->setText(avatar + " " + name);
    }
}

QWidget* MainWindow::createTopBar()
{
    auto* topBar = new QWidget;
    topBar->setFixedHeight(48);
    topBar->setStyleSheet(
        "background: rgba(14,22,40,0.95); border-bottom: 2px solid rgba(250,204,21,0.3);");

    auto* layout = new QHBoxLayout(topBar);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(10);

    // Logo
    auto* logo = new QLabel("OMNIBUS CASINO");
    logo->setStyleSheet("color: #facc15; font-size: 15px; font-weight: 800; letter-spacing: 1px; background: transparent;");
    layout->addWidget(logo);

    // Separator
    auto* sep1 = new QWidget;
    sep1->setFixedSize(1, 24);
    sep1->setStyleSheet("background: rgba(250,204,21,0.2);");
    layout->addWidget(sep1);

    // Player name
    m_playerNameLabel = new QLabel("---");
    m_playerNameLabel->setStyleSheet("color: #facc15; font-size: 12px; font-weight: 800; background: transparent;");
    layout->addWidget(m_playerNameLabel);

    auto* sep2 = new QWidget;
    sep2->setFixedSize(1, 24);
    sep2->setStyleSheet("background: rgba(250,204,21,0.2);");
    layout->addWidget(sep2);

    // Balance display
    m_balanceLabel = new QLabel("---");
    m_balanceLabel->setStyleSheet("color: #facc15; font-size: 11px; font-weight: 700; background: transparent;");
    layout->addWidget(m_balanceLabel);

    layout->addStretch();

    // Server toggle button
    m_serverToggleBtn = new QPushButton("Server: ON");
    m_serverToggleBtn->setStyleSheet(
        "QPushButton { background: rgba(16,235,4,0.15); color: #10eb04; "
        "border: 1px solid rgba(16,235,4,0.3); padding: 6px 14px; border-radius: 4px; "
        "font-weight: 700; font-size: 10px; }"
        "QPushButton:hover { background: rgba(16,235,4,0.25); }");
    connect(m_serverToggleBtn, &QPushButton::clicked, this, [this]() {
        if (m_pokerServer->isRunning()) {
            m_pokerServer->stop();
        } else {
            m_pokerServer->start();
        }
        updateServerStatus();
    });
    layout->addWidget(m_serverToggleBtn);

    // Wallet button
    auto* walletBtn = new QPushButton("Wallet");
    walletBtn->setStyleSheet(
        "QPushButton { background: rgba(250,204,21,0.12); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.25); padding: 6px 16px; border-radius: 4px; "
        "font-weight: 700; font-size: 11px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.25); }");
    layout->addWidget(walletBtn);

    // ADD FUNDS button
    auto* addFundsBtn = new QPushButton("+ ADD FUNDS");
    addFundsBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #10eb04, stop:1 #0cc800); "
        "color: #0f1419; border: none; padding: 6px 18px; border-radius: 4px; "
        "font-weight: 800; font-size: 11px; }"
        "QPushButton:hover { background: #10eb04; }");
    connect(addFundsBtn, &QPushButton::clicked, this, [this]() {
        m_engine->setBalance("EUR", m_engine->getPlayer().balance("EUR") + 5000.0);
        m_engine->setBalance("BTC", m_engine->getPlayer().balance("BTC") + 0.05);
        m_engine->setBalance("ETH", m_engine->getPlayer().balance("ETH") + 1.0);
        m_engine->setBalance("LCX", m_engine->getPlayer().balance("LCX") + 50000.0);
        updateBalanceDisplay();
        m_engine->saveState();
    });
    layout->addWidget(addFundsBtn);

    return topBar;
}

void MainWindow::createSidebar()
{
    m_sidebar = new QWidget;
    m_sidebar->setFixedWidth(200);
    m_sidebar->setStyleSheet("background: rgba(10,14,20,0.95); border-right: 1px solid rgba(250,204,21,0.15);");

    auto* outerLayout = new QVBoxLayout(m_sidebar);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: rgba(10,14,20,0.95); width: 4px; }"
        "QScrollBar::handle:vertical { background: rgba(250,204,21,0.2); border-radius: 2px; }");

    auto* scrollContent = new QWidget;
    auto* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    struct SidebarItem {
        QString icon;
        QString name;
    };

    QVector<SidebarItem> items = {
        {QString::fromUtf8("\xF0\x9F\x8F\xA0"), "Lobby"},
        {QString::fromUtf8("\xF0\x9F\x8E\xB2"), "Dice"},
        {QString::fromUtf8("\xF0\x9F\x9A\x80"), "Crash"},
        {QString::fromUtf8("\xF0\x9F\xAA\x99"), "Coin Flip"},
        {QString::fromUtf8("\xF0\x9F\x83\x8F"), "Blackjack"},
        {QString::fromUtf8("\xF0\x9F\x83\x8F"), "Poker"},
        {QString::fromUtf8("\xF0\x9F\x8E\xB0"), "Room"},
        {QString::fromUtf8("\xF0\x9F\x8C\x90"), "MP Room"},
        {QString::fromUtf8("\xF0\x9F\x8F\x86"), "Tournaments"},
        {QString::fromUtf8("\xF0\x9F\x8E\xB0"), "Slots"},
        {QString::fromUtf8("\xE2\x9A\xBD"), "Sports"},
        {QString::fromUtf8("\xF0\x9F\x93\x8A"), "Binary Options"},
        {QString::fromUtf8("\xF0\x9F\x8F\x87"), "Race"},
        {QString::fromUtf8("\xF0\x9F\x8F\x86"), "Auction"},
    };

    for (int i = 0; i < items.size(); ++i) {
        auto* btn = new QPushButton(QString("%1  %2").arg(items[i].icon, items[i].name));
        btn->setFixedHeight(40);
        btn->setStyleSheet(
            "QPushButton { background: transparent; color: rgba(255,255,255,0.6); "
            "text-align: left; padding: 8px 12px; border: none; border-radius: 6px; "
            "font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { background: rgba(250,204,21,0.1); color: #facc15; }"
            "QPushButton:checked { background: rgba(250,204,21,0.15); color: #facc15; "
            "border-left: 3px solid #facc15; }");
        btn->setCheckable(true);
        if (i == 0) btn->setChecked(true);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            switchToPage(i);
        });

        m_sidebarBtns.append(btn);
        layout->addWidget(btn);
    }

    layout->addStretch();

    // Settings button
    auto* settingsBtn = new QPushButton(QString::fromUtf8("\xE2\x9A\x99\xEF\xB8\x8F  Settings"));
    settingsBtn->setFixedHeight(40);
    settingsBtn->setStyleSheet(
        "QPushButton { background: transparent; color: rgba(255,255,255,0.4); "
        "text-align: left; padding: 8px 12px; border: none; border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.1); color: #facc15; }");
    layout->addWidget(settingsBtn);

    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea);
}

void MainWindow::createContent()
{
    m_stack = new QStackedWidget;

    // Page 0: Lobby
    createLobbyPage();

    // Create all 10 games
    auto* dice = new DiceGame(m_engine);
    auto* crash = new CrashGame(m_engine);
    auto* coinflip = new CoinFlipGame(m_engine);
    auto* blackjack = new BlackjackGame(m_engine);
    auto* poker = new PokerGame(m_engine);
    auto* pokerRoom = new PokerRoom(m_engine);
    auto* slotsGame = new SlotsGame(m_engine);
    auto* sports = new SportsBettingGame(m_engine);
    auto* binary = new BinaryOptionsGame(m_engine);
    auto* race = new RaceBettingGame(m_engine);
    auto* auction = new AuctionGame(m_engine);

    addGamePage(dice);
    addGamePage(crash);
    addGamePage(coinflip);
    addGamePage(blackjack);
    addGamePage(poker);
    addGamePage(pokerRoom);

    // MP Room — PokerRoom in multiplayer mode (simplified, replaces P2P lobby)
    auto* mpPokerRoom = new PokerRoom(m_engine);
    mpPokerRoom->setPokerServer(m_pokerServer);
    mpPokerRoom->setAccountSystem(m_accountSystem);
    mpPokerRoom->setMultiplayerMode();
    m_p2pLobbyPageIndex = m_stack->count();
    {
        auto* mpPage = new QWidget;
        auto* mpLayout = new QHBoxLayout(mpPage);
        mpLayout->setContentsMargins(0, 0, 0, 0);
        mpLayout->addWidget(mpPokerRoom, 1);
        m_games.append(mpPokerRoom);
        m_stack->addWidget(mpPage);
    }

    // Keep the old centralized lobby for backward compat (hidden, not in sidebar)
    m_lobbyWidget = new LobbyWidget(m_pokerClient, nullptr);
    m_pokerLobbyPageIndex = -1;

    // P2P systems still exist but not wired to UI anymore
    m_p2pLobbyWidget = new P2PLobbyWidget(m_p2pDiscovery, m_mentalPoker, m_reputationSystem, nullptr);

    // Tournament Lobby page
    m_tournamentWidget = new TournamentLobbyWidget;
    m_tournamentWidget->setPokerServer(m_pokerServer);
    m_tournamentPageIndex = m_stack->count();
    {
        auto* tourPage = new QWidget;
        auto* tourLayout = new QHBoxLayout(tourPage);
        tourLayout->setContentsMargins(0, 0, 0, 0);
        tourLayout->addWidget(m_tournamentWidget, 1);
        m_stack->addWidget(tourPage);
    }

    // Load preset tournaments into the lobby widget
    for (Tournament* t : m_pokerServer->getTournaments()) {
        m_tournamentWidget->addTournament(t);
    }

    // Wire: when server creates a tournament, add it to the UI
    connect(m_pokerServer, &PokerServer::tournamentCreated, this, [this](Tournament* t) {
        m_tournamentWidget->addTournament(t);
    });

    // Wire: create tournament from UI
    connect(m_tournamentWidget, &TournamentLobbyWidget::createTournamentRequested,
            this, [this](const TournamentConfig& cfg) {
        m_pokerServer->createTournament(cfg);
    });

    // Wire: register/unregister from UI (local player uses a pseudo-id)
    connect(m_tournamentWidget, &TournamentLobbyWidget::registerRequested,
            this, [this](const QString& tournamentId) {
        Tournament* t = m_pokerServer->findTournament(tournamentId);
        if (!t) return;

        QString pid = "local_player";
        QString pname = m_accountSystem->isLoggedIn() ? m_accountSystem->displayName() : "Player";
        m_tournamentWidget->setPlayerId(pid);
        m_tournamentWidget->setPlayerName(pname);

        if (t->registerPlayer(pid, pname)) {
            m_tournamentWidget->refreshAll();
        }
    });

    connect(m_tournamentWidget, &TournamentLobbyWidget::unregisterRequested,
            this, [this](const QString& tournamentId) {
        Tournament* t = m_pokerServer->findTournament(tournamentId);
        if (!t) return;
        t->unregisterPlayer("local_player");
        m_tournamentWidget->refreshAll();
    });

    addGamePage(slotsGame);
    addGamePage(sports);
    addGamePage(binary);
    addGamePage(race);
    addGamePage(auction);
}

void MainWindow::createLobbyPage()
{
    auto* lobbyScroll = new QScrollArea;
    lobbyScroll->setWidgetResizable(true);
    lobbyScroll->setStyleSheet("QScrollArea { border: none; background: #0f1419; }");

    auto* lobby = new QWidget;
    auto* lobbyLayout = new QVBoxLayout(lobby);
    lobbyLayout->setContentsMargins(24, 24, 24, 24);

    auto* welcomeLabel = new QLabel(QString::fromUtf8(
        "\xF0\x9F\x8E\xB0 Welcome to OmniBus Casino"));
    welcomeLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #facc15;");
    welcomeLabel->setAlignment(Qt::AlignCenter);
    lobbyLayout->addWidget(welcomeLabel);

    auto* subLabel = new QLabel("Provably Fair | Multi-Currency | 10 Games");
    subLabel->setStyleSheet("font-size: 14px; color: #aaa;");
    subLabel->setAlignment(Qt::AlignCenter);
    lobbyLayout->addWidget(subLabel);

    lobbyLayout->addSpacing(20);

    // Game cards grid
    auto* grid = new QGridLayout;
    grid->setSpacing(16);

    struct GameCard {
        QString icon;
        QString name;
        QString desc;
        int pageIndex;
    };

    QVector<GameCard> cards = {
        {QString::fromUtf8("\xF0\x9F\x8E\xB2"), "Dice", "Roll over/under. Set your target.", 1},
        {QString::fromUtf8("\xF0\x9F\x9A\x80"), "Crash", "Cash out before the crash!", 2},
        {QString::fromUtf8("\xF0\x9F\xAA\x99"), "Coin Flip", "Heads or tails. Double or nothing.", 3},
        {QString::fromUtf8("\xF0\x9F\x83\x8F"), "Blackjack", "Beat the dealer to 21.", 4},
        {QString::fromUtf8("\xF0\x9F\x83\x8F"), "Poker", "Texas Hold'em vs AI.", 5},
        {QString::fromUtf8("\xF0\x9F\x8E\xB0"), "Poker Room", "Visual poker table. 9 seats.", 6},
        {QString::fromUtf8("\xF0\x9F\x8C\x90"), "MP Room", "Multiplayer poker. Host or join!", 7},
        {QString::fromUtf8("\xF0\x9F\x8F\x86"), "Tournaments", "SNG & MTT poker tournaments!", 8},
        {QString::fromUtf8("\xF0\x9F\x8E\xB0"), "Slots", "5-reel crypto slots. Wilds & scatters.", 9},
        {QString::fromUtf8("\xE2\x9A\xBD"), "Sports", "Bet on football matches.", 10},
        {QString::fromUtf8("\xF0\x9F\x93\x8A"), "Binary Options", "Predict price direction.", 11},
        {QString::fromUtf8("\xF0\x9F\x8F\x87"), "Race", "Pick a winner. 8 racers.", 12},
        {QString::fromUtf8("\xF0\x9F\x8F\x86"), "Auction", "Outbid the competition.", 13},
    };

    for (int i = 0; i < cards.size(); ++i) {
        auto* card = new QPushButton;
        card->setFixedSize(220, 140);
        card->setStyleSheet(
            "QPushButton { background: rgba(26,31,46,0.7); border: 1px solid rgba(250,204,21,0.15); "
            "border-radius: 12px; text-align: center; padding: 12px; }"
            "QPushButton:hover { background: rgba(250,204,21,0.08); border-color: rgba(250,204,21,0.4); "
            "transform: scale(1.02); }");

        auto* cardLayout = new QVBoxLayout(card);
        auto* icon = new QLabel(cards[i].icon);
        icon->setStyleSheet("font-size: 36px;");
        icon->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(icon);

        auto* name = new QLabel(cards[i].name);
        name->setStyleSheet("font-size: 16px; font-weight: bold; color: #facc15;");
        name->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(name);

        auto* desc = new QLabel(cards[i].desc);
        desc->setStyleSheet("font-size: 11px; color: #888;");
        desc->setAlignment(Qt::AlignCenter);
        desc->setWordWrap(true);
        cardLayout->addWidget(desc);

        int pageIdx = cards[i].pageIndex;
        connect(card, &QPushButton::clicked, this, [this, pageIdx]() {
            switchToPage(pageIdx);
        });

        grid->addWidget(card, i / 5, i % 5);
    }

    lobbyLayout->addLayout(grid);
    lobbyLayout->addStretch();

    lobbyScroll->setWidget(lobby);
    m_stack->addWidget(lobbyScroll);
}

void MainWindow::addGamePage(GameWidget* game)
{
    auto* page = new QWidget;
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // PokerRoom and PokerGame get FULL SCREEN — no BetPanel
    bool isPoker = (game->gameId() == "poker" || game->gameId() == "poker_room" || game->gameId() == "pokerRoom");

    if (isPoker) {
        // Poker: full width, no bet panel, no scroll wrapper
        layout->addWidget(game, 1);
    } else {
        // Other games: scrollable + bet panel on right
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea { border: none; }");
        scroll->setWidget(game);
        layout->addWidget(scroll, 1);

        // Separator
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet("background: rgba(250,204,21,0.15);");
        sep->setFixedWidth(1);
        layout->addWidget(sep);

        // Bet panel
        auto* betPanel = new BetPanel(m_engine);
        layout->addWidget(betPanel);

        connect(betPanel, &BetPanel::betRequested, this, [this, game](double amount, const QString& currency) {
            double bal = m_engine->getPlayer().balance(currency);
            if (amount > bal || amount < m_engine->minBet()) return;
            m_engine->placeBet(game->gameId(), amount, currency, 2.0);
            game->startGame();
        });

        m_betPanels.append(betPanel);
    }

    m_games.append(game);
    m_stack->addWidget(page);
}

void MainWindow::switchToPage(int index)
{
    if (index < 0 || index >= m_stack->count()) return;

    m_currentPage = index;
    m_stack->setCurrentIndex(index);

    for (int i = 0; i < m_sidebarBtns.size(); ++i)
        m_sidebarBtns[i]->setChecked(i == index);
}

void MainWindow::updateBalanceDisplay()
{
    auto& p = m_engine->getPlayer();
    m_balanceLabel->setText(QString(
        "EUR: %1 | BTC: %2 | ETH: %3 | LCX: %4")
        .arg(p.balance("EUR"), 0, 'f', 2)
        .arg(p.balance("BTC"), 0, 'f', 6)
        .arg(p.balance("ETH"), 0, 'f', 4)
        .arg(p.balance("LCX"), 0, 'f', 0));
}

void MainWindow::createStatusBar()
{
    auto* sb = statusBar();
    sb->showMessage("OmniBus Casino v1.0 | Provably Fair | Connected");

    // Server status indicator
    m_serverStatusLabel = new QLabel("Server: OFF");
    m_serverStatusLabel->setStyleSheet("color: #666; font-size: 10px; padding: 0 8px; font-weight: bold;");
    sb->addPermanentWidget(m_serverStatusLabel);

    auto* pfLabel = new QLabel("SHA-256 Verified");
    pfLabel->setStyleSheet("color: #10eb04; font-size: 10px; padding: 0 8px;");
    sb->addPermanentWidget(pfLabel);

    auto* verLabel = new QLabel("v1.0.0");
    verLabel->setStyleSheet("color: rgba(255,255,255,0.3); font-size: 10px; padding: 0 8px;");
    sb->addPermanentWidget(verLabel);

    // Wire server signals to status updates
    connect(m_pokerServer, &PokerServer::serverStarted, this, [this](quint16 port) {
        updateServerStatus();
    });
    connect(m_pokerServer, &PokerServer::serverStopped, this, [this]() {
        updateServerStatus();
    });
    connect(m_pokerServer, &PokerServer::playerConnected, this, [this](const QString&) {
        updateServerStatus();
    });
    connect(m_pokerServer, &PokerServer::playerDisconnected, this, [this](const QString&) {
        updateServerStatus();
    });
}

void MainWindow::updateServerStatus()
{
    if (m_pokerServer->isRunning()) {
        m_serverStatusLabel->setText(
            QString("Poker Server: port 9999 | %1 players | %2 tables")
                .arg(m_pokerServer->playerCount())
                .arg(m_pokerServer->tableCount()));
        m_serverStatusLabel->setStyleSheet("color: #10eb04; font-size: 10px; padding: 0 8px; font-weight: bold;");
        m_serverToggleBtn->setText("Server: ON");
        m_serverToggleBtn->setStyleSheet(
            "QPushButton { background: rgba(16,235,4,0.15); color: #10eb04; "
            "border: 1px solid rgba(16,235,4,0.3); padding: 6px 14px; border-radius: 4px; "
            "font-weight: 700; font-size: 10px; }"
            "QPushButton:hover { background: rgba(16,235,4,0.25); }");
    } else {
        m_serverStatusLabel->setText("Poker Server: OFF");
        m_serverStatusLabel->setStyleSheet("color: #eb0404; font-size: 10px; padding: 0 8px; font-weight: bold;");
        m_serverToggleBtn->setText("Server: OFF");
        m_serverToggleBtn->setStyleSheet(
            "QPushButton { background: rgba(235,4,4,0.15); color: #eb0404; "
            "border: 1px solid rgba(235,4,4,0.3); padding: 6px 14px; border-radius: 4px; "
            "font-weight: 700; font-size: 10px; }"
            "QPushButton:hover { background: rgba(235,4,4,0.25); }");
    }
}
