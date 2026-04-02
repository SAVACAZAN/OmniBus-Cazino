#include "TournamentLobbyWidget.h"
#include "server/PokerServer.h"
#include <QMessageBox>

// ─── Style Helpers ───────────────────────────────────────────────────────────────

QString TournamentLobbyWidget::goldButtonStyle()
{
    return "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, "
           "stop:0 #facc15, stop:1 #d4a017); color: #0f1419; border: none; "
           "padding: 8px 20px; border-radius: 6px; font-weight: 800; font-size: 12px; }"
           "QPushButton:hover { background: #facc15; }"
           "QPushButton:disabled { background: #444; color: #888; }";
}

QString TournamentLobbyWidget::greenButtonStyle()
{
    return "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, "
           "stop:0 #10eb04, stop:1 #0cc800); color: #0f1419; border: none; "
           "padding: 8px 20px; border-radius: 6px; font-weight: 800; font-size: 12px; }"
           "QPushButton:hover { background: #10eb04; }"
           "QPushButton:disabled { background: #444; color: #888; }";
}

QString TournamentLobbyWidget::tableStyle()
{
    return "QTableWidget { background: rgba(14,22,40,0.95); color: #e0e0e0; "
           "border: 1px solid rgba(250,204,21,0.2); gridline-color: rgba(250,204,21,0.1); "
           "font-size: 12px; selection-background-color: rgba(250,204,21,0.15); }"
           "QTableWidget::item { padding: 10px 8px; }"
           "QHeaderView::section { background: rgba(10,14,20,0.95); color: #facc15; "
           "border: 1px solid rgba(250,204,21,0.15); padding: 6px; font-weight: 700; font-size: 11px; }";
}

QString TournamentLobbyWidget::panelStyle()
{
    return "background: rgba(14,22,40,0.85); border: 1px solid rgba(250,204,21,0.2); border-radius: 8px;";
}

QString TournamentLobbyWidget::headerStyle()
{
    return "color: #facc15; font-size: 22px; font-weight: 800; letter-spacing: 1px; background: transparent;";
}

QString TournamentLobbyWidget::labelStyle()
{
    return "color: #e0e0e0; font-size: 13px; background: transparent;";
}

QString TournamentLobbyWidget::inputStyle()
{
    return "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { "
           "background: rgba(10,14,20,0.95); color: #e0e0e0; "
           "border: 1px solid rgba(250,204,21,0.25); border-radius: 6px; "
           "padding: 12px 14px; font-size: 13px; min-height: 18px; }"
           "QComboBox::drop-down { border: none; width: 24px; }"
           "QComboBox QAbstractItemView { background: #0f1419; color: #e0e0e0; "
           "selection-background-color: rgba(250,204,21,0.2); }";
}

// ─── Constructor / Destructor ────────────────────────────────────────────────────

TournamentLobbyWidget::TournamentLobbyWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUI();

    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &TournamentLobbyWidget::onRefreshTimer);
    m_refreshTimer->start(1000); // 1s refresh
}

TournamentLobbyWidget::~TournamentLobbyWidget()
{
    m_refreshTimer->stop();
}

// ─── Build UI ────────────────────────────────────────────────────────────────────

void TournamentLobbyWidget::buildUI()
{
    setStyleSheet("background: #0f1419;");

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: #0f1419; }"
                          "QScrollBar:vertical { background: #0f1419; width: 6px; }"
                          "QScrollBar::handle:vertical { background: rgba(250,204,21,0.3); border-radius: 3px; }");

    auto* content = new QWidget;
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // Header
    auto* headerLayout = new QHBoxLayout;
    auto* titleLabel = new QLabel(QString::fromUtf8("\xF0\x9F\x8F\x86  TOURNAMENTS"));
    titleLabel->setStyleSheet(headerStyle());
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    auto* createBtn = new QPushButton("+ CREATE TOURNAMENT");
    createBtn->setStyleSheet(goldButtonStyle());
    connect(createBtn, &QPushButton::clicked, this, &TournamentLobbyWidget::showCreateDialog);
    headerLayout->addWidget(createBtn);

    mainLayout->addLayout(headerLayout);

    // Separator
    auto* sep = new QWidget;
    sep->setFixedHeight(2);
    sep->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:1,y2:0, "
                       "stop:0 transparent, stop:0.2 rgba(250,204,21,0.4), "
                       "stop:0.8 rgba(250,204,21,0.4), stop:1 transparent);");
    mainLayout->addWidget(sep);

    // Tournament list section
    buildTournamentListSection(mainLayout);

    // Running tournament panel (hidden until registered)
    buildRunningPanel(mainLayout);

    // Results panel (hidden until tournament finishes)
    buildResultsPanel(mainLayout);

    mainLayout->addStretch();

    scroll->setWidget(content);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scroll);
}

void TournamentLobbyWidget::buildTournamentListSection(QVBoxLayout* parent)
{
    auto* sectionLabel = new QLabel("Upcoming & Running Tournaments");
    sectionLabel->setStyleSheet("color: #facc15; font-size: 15px; font-weight: 700; background: transparent;");
    parent->addWidget(sectionLabel);

    m_tournamentTable = new QTableWidget(0, 7);
    m_tournamentTable->setHorizontalHeaderLabels({"Name", "Type", "Buy-In", "Players", "Prize Pool", "Status", "Action"});
    m_tournamentTable->setStyleSheet(tableStyle());
    m_tournamentTable->horizontalHeader()->setStretchLastSection(true);
    m_tournamentTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tournamentTable->verticalHeader()->setVisible(false);
    m_tournamentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tournamentTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tournamentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tournamentTable->setMinimumHeight(200);
    m_tournamentTable->setColumnWidth(1, 80);
    m_tournamentTable->setColumnWidth(2, 80);
    m_tournamentTable->setColumnWidth(3, 80);
    m_tournamentTable->setColumnWidth(4, 100);
    m_tournamentTable->setColumnWidth(5, 100);
    m_tournamentTable->setColumnWidth(6, 120);
    parent->addWidget(m_tournamentTable);
}

void TournamentLobbyWidget::buildRunningPanel(QVBoxLayout* parent)
{
    m_runningPanel = new QWidget;
    m_runningPanel->setStyleSheet(panelStyle());
    m_runningPanel->setVisible(false);

    auto* layout = new QVBoxLayout(m_runningPanel);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto* panelTitle = new QLabel("Your Tournament");
    panelTitle->setStyleSheet("color: #facc15; font-size: 16px; font-weight: 800; background: transparent;");
    layout->addWidget(panelTitle);

    // Info row
    auto* infoRow = new QHBoxLayout;

    // Blind level
    auto* blindBox = new QWidget;
    blindBox->setStyleSheet("background: rgba(10,14,20,0.8); border: 1px solid rgba(250,204,21,0.15); border-radius: 6px;");
    auto* blindLayout = new QVBoxLayout(blindBox);
    blindLayout->setContentsMargins(12, 8, 12, 8);
    auto* blindTitle = new QLabel("Blind Level");
    blindTitle->setStyleSheet("color: #888; font-size: 10px; font-weight: 700; background: transparent;");
    blindTitle->setAlignment(Qt::AlignCenter);
    blindLayout->addWidget(blindTitle);
    m_blindLevelLabel = new QLabel("Level 1: 10/20 | Next in 5:00");
    m_blindLevelLabel->setStyleSheet("color: #facc15; font-size: 14px; font-weight: 800; background: transparent;");
    m_blindLevelLabel->setAlignment(Qt::AlignCenter);
    blindLayout->addWidget(m_blindLevelLabel);
    infoRow->addWidget(blindBox);

    // Players remaining
    auto* playersBox = new QWidget;
    playersBox->setStyleSheet("background: rgba(10,14,20,0.8); border: 1px solid rgba(250,204,21,0.15); border-radius: 6px;");
    auto* playersLayout = new QVBoxLayout(playersBox);
    playersLayout->setContentsMargins(12, 8, 12, 8);
    auto* playersTitle = new QLabel("Players");
    playersTitle->setStyleSheet("color: #888; font-size: 10px; font-weight: 700; background: transparent;");
    playersTitle->setAlignment(Qt::AlignCenter);
    playersLayout->addWidget(playersTitle);
    m_playersRemainingLabel = new QLabel("9/9 players");
    m_playersRemainingLabel->setStyleSheet("color: #e0e0e0; font-size: 14px; font-weight: 700; background: transparent;");
    m_playersRemainingLabel->setAlignment(Qt::AlignCenter);
    playersLayout->addWidget(m_playersRemainingLabel);
    infoRow->addWidget(playersBox);

    // Your position
    auto* posBox = new QWidget;
    posBox->setStyleSheet("background: rgba(10,14,20,0.8); border: 1px solid rgba(250,204,21,0.15); border-radius: 6px;");
    auto* posLayout = new QVBoxLayout(posBox);
    posLayout->setContentsMargins(12, 8, 12, 8);
    auto* posTitle = new QLabel("Your Position");
    posTitle->setStyleSheet("color: #888; font-size: 10px; font-weight: 700; background: transparent;");
    posTitle->setAlignment(Qt::AlignCenter);
    posLayout->addWidget(posTitle);
    m_yourPositionLabel = new QLabel("#1 of 9");
    m_yourPositionLabel->setStyleSheet("color: #10eb04; font-size: 14px; font-weight: 800; background: transparent;");
    m_yourPositionLabel->setAlignment(Qt::AlignCenter);
    posLayout->addWidget(m_yourPositionLabel);
    infoRow->addWidget(posBox);

    // Your stack
    auto* stackBox = new QWidget;
    stackBox->setStyleSheet("background: rgba(10,14,20,0.8); border: 1px solid rgba(250,204,21,0.15); border-radius: 6px;");
    auto* stackLayout = new QVBoxLayout(stackBox);
    stackLayout->setContentsMargins(12, 8, 12, 8);
    auto* stackTitle = new QLabel("Your Stack");
    stackTitle->setStyleSheet("color: #888; font-size: 10px; font-weight: 700; background: transparent;");
    stackTitle->setAlignment(Qt::AlignCenter);
    stackLayout->addWidget(stackTitle);
    m_yourStackLabel = new QLabel("5,000 chips");
    m_yourStackLabel->setStyleSheet("color: #facc15; font-size: 14px; font-weight: 800; background: transparent;");
    m_yourStackLabel->setAlignment(Qt::AlignCenter);
    stackLayout->addWidget(m_yourStackLabel);
    infoRow->addWidget(stackBox);

    // Prize pool
    auto* prizeBox = new QWidget;
    prizeBox->setStyleSheet("background: rgba(10,14,20,0.8); border: 1px solid rgba(250,204,21,0.15); border-radius: 6px;");
    auto* prizeLayout = new QVBoxLayout(prizeBox);
    prizeLayout->setContentsMargins(12, 8, 12, 8);
    auto* prizeTitle = new QLabel("Prize Pool");
    prizeTitle->setStyleSheet("color: #888; font-size: 10px; font-weight: 700; background: transparent;");
    prizeTitle->setAlignment(Qt::AlignCenter);
    prizeLayout->addWidget(prizeTitle);
    m_prizePoolLabel = new QLabel("4,500");
    m_prizePoolLabel->setStyleSheet("color: #10eb04; font-size: 14px; font-weight: 800; background: transparent;");
    m_prizePoolLabel->setAlignment(Qt::AlignCenter);
    prizeLayout->addWidget(m_prizePoolLabel);
    infoRow->addWidget(prizeBox);

    layout->addLayout(infoRow);

    // Leaderboard
    auto* lbTitle = new QLabel("Leaderboard (Top 5)");
    lbTitle->setStyleSheet("color: #facc15; font-size: 13px; font-weight: 700; background: transparent;");
    layout->addWidget(lbTitle);

    m_leaderboardTable = new QTableWidget(0, 3);
    m_leaderboardTable->setHorizontalHeaderLabels({"Rank", "Player", "Chips"});
    m_leaderboardTable->setStyleSheet(tableStyle());
    m_leaderboardTable->horizontalHeader()->setStretchLastSection(true);
    m_leaderboardTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_leaderboardTable->verticalHeader()->setVisible(false);
    m_leaderboardTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_leaderboardTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_leaderboardTable->setFixedHeight(180);
    m_leaderboardTable->setColumnWidth(0, 60);
    m_leaderboardTable->setColumnWidth(2, 120);
    layout->addWidget(m_leaderboardTable);

    parent->addWidget(m_runningPanel);
}

void TournamentLobbyWidget::buildResultsPanel(QVBoxLayout* parent)
{
    m_resultsPanel = new QWidget;
    m_resultsPanel->setStyleSheet(panelStyle());
    m_resultsPanel->setVisible(false);

    auto* layout = new QVBoxLayout(m_resultsPanel);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto* resultTitle = new QLabel("Tournament Finished!");
    resultTitle->setStyleSheet("color: #facc15; font-size: 18px; font-weight: 800; background: transparent;");
    resultTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(resultTitle);

    m_resultPositionLabel = new QLabel("Your Position: #1");
    m_resultPositionLabel->setStyleSheet("color: #e0e0e0; font-size: 16px; font-weight: 700; background: transparent;");
    m_resultPositionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_resultPositionLabel);

    m_resultPrizeLabel = new QLabel("Prize Won: 2,250");
    m_resultPrizeLabel->setStyleSheet("color: #10eb04; font-size: 18px; font-weight: 800; background: transparent;");
    m_resultPrizeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_resultPrizeLabel);

    m_resultsTable = new QTableWidget(0, 3);
    m_resultsTable->setHorizontalHeaderLabels({"Position", "Player", "Prize"});
    m_resultsTable->setStyleSheet(tableStyle());
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultsTable->verticalHeader()->setVisible(false);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_resultsTable->setFixedHeight(250);
    m_resultsTable->setColumnWidth(0, 80);
    m_resultsTable->setColumnWidth(2, 120);
    layout->addWidget(m_resultsTable);

    parent->addWidget(m_resultsPanel);
}

// ─── Tournament Management ───────────────────────────────────────────────────────

void TournamentLobbyWidget::addTournament(Tournament* t)
{
    m_tournaments.append(t);

    connect(t, &Tournament::tournamentStarted, this, [this](const QString&) {
        updateTournamentTable();
    });
    connect(t, &Tournament::blindsIncreased, this, [this](int, double, double) {
        updateRunningPanel();
    });
    connect(t, &Tournament::playerEliminated, this, [this](const QString&, int, double) {
        updateRunningPanel();
        updateTournamentTable();
    });
    connect(t, &Tournament::tournamentFinished, this, [this](const QString&, const QVector<TournamentPlayer>&) {
        updateResultsPanel();
        updateTournamentTable();
    });
    connect(t, &Tournament::statusChanged, this, [this](TournamentStatus) {
        updateTournamentTable();
        updateRunningPanel();
    });

    updateTournamentTable();
}

void TournamentLobbyWidget::removeTournament(const QString& tournamentId)
{
    for (int i = 0; i < m_tournaments.size(); ++i) {
        if (m_tournaments[i]->id() == tournamentId) {
            if (m_activeTournament == m_tournaments[i])
                m_activeTournament = nullptr;
            m_tournaments.removeAt(i);
            break;
        }
    }
    updateTournamentTable();
}

void TournamentLobbyWidget::refreshAll()
{
    updateTournamentTable();
    updateRunningPanel();
    updateResultsPanel();
}

// ─── UI Updates ──────────────────────────────────────────────────────────────────

void TournamentLobbyWidget::updateTournamentTable()
{
    m_tournamentTable->setRowCount(0);

    for (int i = 0; i < m_tournaments.size(); ++i) {
        Tournament* t = m_tournaments[i];
        int row = m_tournamentTable->rowCount();
        m_tournamentTable->insertRow(row);

        auto setItem = [&](int col, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            m_tournamentTable->setItem(row, col, item);
        };

        TournamentConfig cfg = t->config();
        setItem(0, cfg.name);
        setItem(1, typeToString(cfg.type));
        setItem(2, QString::number(cfg.buyIn, 'f', 0));
        setItem(3, QString("%1/%2").arg(t->registeredCount()).arg(cfg.maxPlayers));
        setItem(4, QString::number(t->totalPrizePool(), 'f', 0));
        setItem(5, statusToString(t->status()));

        // Action button
        auto* actionBtn = new QPushButton;
        actionBtn->setFixedHeight(28);

        bool isRegistered = false;
        if (t->findPlayer(m_playerId))
            isRegistered = true;

        if (t->status() == TournamentStatus::Finished) {
            actionBtn->setText("FINISHED");
            actionBtn->setEnabled(false);
            actionBtn->setStyleSheet(goldButtonStyle());
        } else if (isRegistered) {
            actionBtn->setText("UNREGISTER");
            actionBtn->setStyleSheet(
                "QPushButton { background: rgba(235,4,4,0.2); color: #eb0404; "
                "border: 1px solid rgba(235,4,4,0.3); padding: 4px 12px; border-radius: 4px; "
                "font-weight: 700; font-size: 11px; }"
                "QPushButton:hover { background: rgba(235,4,4,0.3); }");
            QString tid = t->id();
            connect(actionBtn, &QPushButton::clicked, this, [this, tid]() {
                emit unregisterRequested(tid);
            });
        } else if (t->status() == TournamentStatus::Registering ||
                   (t->status() == TournamentStatus::Running && t->config().allowLateReg)) {
            actionBtn->setText("REGISTER");
            actionBtn->setStyleSheet(greenButtonStyle());
            QString tid = t->id();
            connect(actionBtn, &QPushButton::clicked, this, [this, tid]() {
                emit registerRequested(tid);
            });
        } else {
            actionBtn->setText("RUNNING");
            actionBtn->setEnabled(false);
            actionBtn->setStyleSheet(goldButtonStyle());
        }

        m_tournamentTable->setCellWidget(row, 6, actionBtn);
    }
}

void TournamentLobbyWidget::updateRunningPanel()
{
    // Find active tournament for this player
    m_activeTournament = nullptr;
    for (Tournament* t : m_tournaments) {
        if (t->status() == TournamentStatus::Running || t->status() == TournamentStatus::FinalTable) {
            if (t->findPlayer(m_playerId)) {
                m_activeTournament = t;
                break;
            }
        }
    }

    if (!m_activeTournament) {
        m_runningPanel->setVisible(false);
        return;
    }

    m_runningPanel->setVisible(true);
    Tournament* t = m_activeTournament;

    // Blinds
    auto blinds = t->currentBlinds();
    int secsLeft = t->secondsUntilNextLevel();
    int mins = secsLeft / 60;
    int secs = secsLeft % 60;
    m_blindLevelLabel->setText(QString("Level %1: %2/%3 | Next in %4:%5")
                                   .arg(t->currentBlindLevel() + 1)
                                   .arg(blinds.first, 0, 'f', 0)
                                   .arg(blinds.second, 0, 'f', 0)
                                   .arg(mins, 2, 10, QChar('0'))
                                   .arg(secs, 2, 10, QChar('0')));

    // Players
    int remaining = t->playersRemaining();
    int total = t->registeredCount();
    m_playersRemainingLabel->setText(QString("%1/%2 players").arg(remaining).arg(total));

    // Your position and stack
    const TournamentPlayer* me = t->findPlayer(m_playerId);
    if (me && !me->eliminated) {
        auto lb = t->leaderboard();
        int myPos = 0;
        for (int i = 0; i < lb.size(); ++i) {
            if (lb[i].playerId == m_playerId) {
                myPos = i + 1;
                break;
            }
        }
        m_yourPositionLabel->setText(QString("#%1 of %2").arg(myPos).arg(remaining));
        m_yourStackLabel->setText(QString("%L1 chips").arg(static_cast<int>(me->chips)));
    } else if (me && me->eliminated) {
        m_yourPositionLabel->setText(QString("Eliminated #%1").arg(me->finishPosition));
        m_yourStackLabel->setText("0 chips");
        m_yourPositionLabel->setStyleSheet("color: #eb0404; font-size: 14px; font-weight: 800; background: transparent;");
    }

    // Prize pool
    m_prizePoolLabel->setText(QString("%L1").arg(static_cast<int>(t->totalPrizePool())));

    // Leaderboard (top 5)
    auto lb = t->leaderboard();
    int showCount = qMin(5, lb.size());
    m_leaderboardTable->setRowCount(showCount);
    for (int i = 0; i < showCount; ++i) {
        auto setItem = [&](int col, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            if (lb[i].playerId == m_playerId) {
                item->setForeground(QColor("#facc15"));
            }
            m_leaderboardTable->setItem(i, col, item);
        };
        setItem(0, QString("#%1").arg(i + 1));
        setItem(1, lb[i].name);
        setItem(2, QString("%L1").arg(static_cast<int>(lb[i].chips)));
    }
}

void TournamentLobbyWidget::updateResultsPanel()
{
    // Find finished tournament we were in
    Tournament* finishedT = nullptr;
    for (Tournament* t : m_tournaments) {
        if (t->status() == TournamentStatus::Finished && t->findPlayer(m_playerId)) {
            finishedT = t;
            break;
        }
    }

    if (!finishedT) {
        m_resultsPanel->setVisible(false);
        return;
    }

    m_resultsPanel->setVisible(true);

    const TournamentPlayer* me = finishedT->findPlayer(m_playerId);
    if (me) {
        m_resultPositionLabel->setText(QString("Your Position: #%1").arg(me->finishPosition));
        if (me->prizeWon > 0) {
            m_resultPrizeLabel->setText(QString("Prize Won: %L1").arg(static_cast<int>(me->prizeWon)));
            m_resultPrizeLabel->setStyleSheet("color: #10eb04; font-size: 18px; font-weight: 800; background: transparent;");
        } else {
            m_resultPrizeLabel->setText("No prize");
            m_resultPrizeLabel->setStyleSheet("color: #888; font-size: 18px; font-weight: 800; background: transparent;");
        }
    }

    // Full results table
    auto players = finishedT->players();
    // Sort by finish position
    std::sort(players.begin(), players.end(), [](const TournamentPlayer& a, const TournamentPlayer& b) {
        if (a.finishPosition == 0 && b.finishPosition == 0) return false;
        if (a.finishPosition == 0) return false;
        if (b.finishPosition == 0) return true;
        return a.finishPosition < b.finishPosition;
    });

    m_resultsTable->setRowCount(players.size());
    for (int i = 0; i < players.size(); ++i) {
        auto setItem = [&](int col, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            if (players[i].playerId == m_playerId)
                item->setForeground(QColor("#facc15"));
            if (players[i].prizeWon > 0)
                item->setForeground(QColor("#10eb04"));
            m_resultsTable->setItem(i, col, item);
        };
        setItem(0, QString("#%1").arg(players[i].finishPosition));
        setItem(1, players[i].name);
        setItem(2, players[i].prizeWon > 0 ? QString("%L1").arg(static_cast<int>(players[i].prizeWon)) : "-");
    }
}

// ─── Actions ─────────────────────────────────────────────────────────────────────

void TournamentLobbyWidget::onCreateClicked()
{
    showCreateDialog();
}

void TournamentLobbyWidget::onRegisterClicked()
{
    int row = m_tournamentTable->currentRow();
    if (row < 0 || row >= m_tournaments.size())
        return;
    emit registerRequested(m_tournaments[row]->id());
}

void TournamentLobbyWidget::onRefreshTimer()
{
    updateRunningPanel();
    // Light refresh of table status column every second
    for (int row = 0; row < m_tournamentTable->rowCount() && row < m_tournaments.size(); ++row) {
        Tournament* t = m_tournaments[row];
        auto* statusItem = m_tournamentTable->item(row, 5);
        if (statusItem)
            statusItem->setText(statusToString(t->status()));
        auto* playersItem = m_tournamentTable->item(row, 3);
        if (playersItem)
            playersItem->setText(QString("%1/%2").arg(t->registeredCount()).arg(t->config().maxPlayers));
        auto* prizeItem = m_tournamentTable->item(row, 4);
        if (prizeItem)
            prizeItem->setText(QString::number(t->totalPrizePool(), 'f', 0));
    }
}

void TournamentLobbyWidget::showCreateDialog()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Create Tournament");
    dlg->setFixedSize(420, 480);
    dlg->setStyleSheet("QDialog { background: #0f1419; }");

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    auto* title = new QLabel("Create New Tournament");
    title->setStyleSheet("color: #facc15; font-size: 16px; font-weight: 800;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* form = new QWidget;
    form->setStyleSheet(inputStyle());
    auto* formLayout = new QVBoxLayout(form);
    formLayout->setSpacing(8);

    // Name
    auto* nameLabel = new QLabel("Tournament Name:");
    nameLabel->setStyleSheet(labelStyle());
    formLayout->addWidget(nameLabel);
    auto* nameEdit = new QLineEdit("My Tournament");
    formLayout->addWidget(nameEdit);

    // Type
    auto* typeLabel = new QLabel("Type:");
    typeLabel->setStyleSheet(labelStyle());
    formLayout->addWidget(typeLabel);
    auto* typeCombo = new QComboBox;
    typeCombo->addItem("Sit & Go", static_cast<int>(TournamentType::SitAndGo));
    typeCombo->addItem("Multi-Table Tournament (MTT)", static_cast<int>(TournamentType::MTT));
    formLayout->addWidget(typeCombo);

    // Max players
    auto* maxLabel = new QLabel("Max Players:");
    maxLabel->setStyleSheet(labelStyle());
    formLayout->addWidget(maxLabel);
    auto* maxCombo = new QComboBox;
    maxCombo->addItems({"6", "9", "18", "45"});
    maxCombo->setCurrentIndex(1); // default 9
    formLayout->addWidget(maxCombo);

    // Buy-in
    auto* buyInLabel = new QLabel("Buy-In:");
    buyInLabel->setStyleSheet(labelStyle());
    formLayout->addWidget(buyInLabel);
    auto* buyInSpin = new QDoubleSpinBox;
    buyInSpin->setRange(10, 100000);
    buyInSpin->setValue(500);
    buyInSpin->setDecimals(0);
    formLayout->addWidget(buyInSpin);

    // Starting stack
    auto* stackLabel = new QLabel("Starting Stack:");
    stackLabel->setStyleSheet(labelStyle());
    formLayout->addWidget(stackLabel);
    auto* stackSpin = new QDoubleSpinBox;
    stackSpin->setRange(100, 100000);
    stackSpin->setValue(5000);
    stackSpin->setDecimals(0);
    formLayout->addWidget(stackSpin);

    // Blind level time
    auto* blindLabel = new QLabel("Blind Level (minutes):");
    blindLabel->setStyleSheet(labelStyle());
    formLayout->addWidget(blindLabel);
    auto* blindSpin = new QSpinBox;
    blindSpin->setRange(1, 30);
    blindSpin->setValue(5);
    formLayout->addWidget(blindSpin);

    // Checkboxes row
    auto* checkRow = new QHBoxLayout;
    auto* lateRegCheck = new QCheckBox("Allow Late Registration");
    lateRegCheck->setStyleSheet("QCheckBox { color: #e0e0e0; font-size: 12px; } "
                                 "QCheckBox::indicator { width: 16px; height: 16px; }");
    checkRow->addWidget(lateRegCheck);

    auto* rebuyCheck = new QCheckBox("Allow Rebuys");
    rebuyCheck->setStyleSheet("QCheckBox { color: #e0e0e0; font-size: 12px; } "
                               "QCheckBox::indicator { width: 16px; height: 16px; }");
    checkRow->addWidget(rebuyCheck);
    formLayout->addLayout(checkRow);

    layout->addWidget(form);

    // Create button
    auto* createBtn = new QPushButton("CREATE");
    createBtn->setFixedHeight(36);
    createBtn->setStyleSheet(goldButtonStyle());
    layout->addWidget(createBtn);

    connect(createBtn, &QPushButton::clicked, dlg, [=]() {
        TournamentConfig cfg;
        cfg.name = nameEdit->text().trimmed();
        if (cfg.name.isEmpty()) cfg.name = "Tournament";
        cfg.type = static_cast<TournamentType>(typeCombo->currentData().toInt());
        cfg.maxPlayers = maxCombo->currentText().toInt();
        cfg.buyIn = buyInSpin->value();
        cfg.startingStack = stackSpin->value();
        cfg.blindLevelMinutes = blindSpin->value();
        cfg.allowLateReg = lateRegCheck->isChecked();
        cfg.allowRebuys = rebuyCheck->isChecked();
        cfg.ensureDefaults();

        emit createTournamentRequested(cfg);
        dlg->accept();
    });

    auto* cancelBtn = new QPushButton("CANCEL");
    cancelBtn->setFixedHeight(32);
    cancelBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #888; border: 1px solid #444; "
        "padding: 6px 16px; border-radius: 4px; font-weight: 700; font-size: 11px; }"
        "QPushButton:hover { color: #e0e0e0; border-color: #888; }");
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    layout->addWidget(cancelBtn);

    dlg->exec();
    dlg->deleteLater();
}

// ─── Helpers ─────────────────────────────────────────────────────────────────────

QString TournamentLobbyWidget::statusToString(TournamentStatus s) const
{
    switch (s) {
    case TournamentStatus::Registering: return "Registering";
    case TournamentStatus::Running:     return "Running";
    case TournamentStatus::FinalTable:  return "Final Table";
    case TournamentStatus::Finished:    return "Finished";
    }
    return "Unknown";
}

QString TournamentLobbyWidget::typeToString(TournamentType t) const
{
    switch (t) {
    case TournamentType::SitAndGo: return "SNG";
    case TournamentType::MTT:      return "MTT";
    }
    return "?";
}
