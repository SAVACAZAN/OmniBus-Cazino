#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include "engine/BettingEngine.h"
#include "engine/AccountSystem.h"
#include "ui/GameWidget.h"
#include "ui/BetPanel.h"
#include "ui/LoginWidget.h"

class PokerServer;
class PokerClient;
class LobbyWidget;
class P2PDiscovery;
class MentalPoker;
class ReputationSystem;
class P2PLobbyWidget;
class TournamentLobbyWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    void setupUI();
    void showLoginScreen();
    void showCasino();
    QWidget* createTopBar();
    void createSidebar();
    void createContent();
    void createStatusBar();
    void createLobbyPage();
    void addGamePage(GameWidget* game);
    void switchToPage(int index);
    void updateBalanceDisplay();
    void updateServerStatus();

    // Root stack: 0=Login, 1=Casino
    QStackedWidget* m_rootStack;
    LoginWidget* m_loginWidget;

    // Account system
    AccountSystem* m_accountSystem;

    BettingEngine* m_engine;
    QStackedWidget* m_stack;
    QWidget* m_sidebar;
    QWidget* m_casinoWidget;  // the full casino UI
    QLabel* m_balanceLabel;
    QLabel* m_playerNameLabel;
    QVector<QPushButton*> m_sidebarBtns;
    QVector<GameWidget*> m_games;
    QVector<BetPanel*> m_betPanels;
    int m_currentPage = 0;

    // Multiplayer poker (centralized)
    PokerServer* m_pokerServer;
    PokerClient* m_pokerClient;
    LobbyWidget* m_lobbyWidget;
    QPushButton* m_serverToggleBtn;
    QLabel* m_serverStatusLabel;
    int m_pokerLobbyPageIndex = -1;

    // P2P Decentralized poker
    P2PDiscovery* m_p2pDiscovery;
    MentalPoker* m_mentalPoker;
    ReputationSystem* m_reputationSystem;
    P2PLobbyWidget* m_p2pLobbyWidget;
    int m_p2pLobbyPageIndex = -1;

    // Tournament system
    TournamentLobbyWidget* m_tournamentWidget = nullptr;
    int m_tournamentPageIndex = -1;
};
