#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTableWidget>
#include <QGroupBox>
#include <QTextEdit>
#include <QClipboard>
#include <QApplication>

class P2PDiscovery;
class MentalPoker;
class ReputationSystem;
struct P2PGame;

/**
 * P2PLobbyWidget — Full P2P lobby UI for decentralized poker.
 *
 * Two modes: HOST or JOIN.
 * HOST: configure table, generate game code, see connected players.
 * JOIN: enter game code, scan LAN, or direct IP connect.
 * Gold casino theme (#facc15 accent, #0f1419 bg).
 */
class P2PLobbyWidget : public QWidget {
    Q_OBJECT
public:
    explicit P2PLobbyWidget(P2PDiscovery* discovery,
                            MentalPoker* mentalPoker,
                            ReputationSystem* reputation,
                            QWidget* parent = nullptr);

signals:
    void joinedGame(const QString& gameCode);
    void hostedGame(const QString& gameCode);
    void gameStarting();

private slots:
    void onHostClicked();
    void onStopHostClicked();
    void onJoinByCodeClicked();
    void onJoinByIPClicked();
    void onScanLANClicked();
    void onGameFound(const P2PGame& game);
    void onLanScanComplete(const QVector<P2PGame>& games);
    void onConnectionEstablished(const QString& gameCode);
    void onConnectionFailed(const QString& reason);
    void onPlayerJoined(const QString& name);
    void onPlayerLeft(const QString& name);

private:
    void setupUI();
    QGroupBox* createHostSection();
    QGroupBox* createJoinSection();
    QGroupBox* createLANGamesSection();
    QGroupBox* createPlayersSection();
    QWidget* createPlayerInfoBar();
    void addLogMessage(const QString& msg, const QString& color = "#aaa");
    void updateGameCodeDisplay(const QString& code);
    void updateLANTable(const QVector<P2PGame>& games);

    // Systems
    P2PDiscovery* m_discovery;
    MentalPoker* m_mentalPoker;
    ReputationSystem* m_reputation;

    // Host section
    QLineEdit* m_tableNameEdit;
    QDoubleSpinBox* m_smallBlindSpin;
    QDoubleSpinBox* m_bigBlindSpin;
    QDoubleSpinBox* m_minBuyInSpin;
    QDoubleSpinBox* m_maxBuyInSpin;
    QSpinBox* m_maxSeatsSpin;
    QComboBox* m_currencyCombo;
    QPushButton* m_hostBtn;
    QPushButton* m_stopHostBtn;
    QLabel* m_gameCodeLabel;
    QLabel* m_gameCodeValue;
    QPushButton* m_copyCodeBtn;

    // Join section
    QLineEdit* m_joinCodeEdit;
    QPushButton* m_joinBtn;
    QLineEdit* m_directIPEdit;
    QLineEdit* m_directPortEdit;
    QPushButton* m_joinIPBtn;
    QPushButton* m_scanLANBtn;

    // LAN games table
    QTableWidget* m_lanTable;

    // Connected players
    QTableWidget* m_playersTable;

    // Log
    QTextEdit* m_logArea;

    // Player info
    QLineEdit* m_playerNameEdit;
    QLabel* m_reputationLabel;
};
