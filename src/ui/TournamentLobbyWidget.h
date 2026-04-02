#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QStackedWidget>
#include <QTimer>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialog>
#include <QScrollArea>
#include <QGroupBox>
#include <QProgressBar>
#include "server/Tournament.h"

class PokerServer;

class TournamentLobbyWidget : public QWidget {
    Q_OBJECT
public:
    explicit TournamentLobbyWidget(QWidget* parent = nullptr);
    ~TournamentLobbyWidget() override;

    void setPokerServer(PokerServer* server) { m_server = server; }
    void setPlayerId(const QString& id) { m_playerId = id; }
    void setPlayerName(const QString& name) { m_playerName = name; }

    void addTournament(Tournament* t);
    void removeTournament(const QString& tournamentId);
    void refreshAll();

signals:
    void createTournamentRequested(const TournamentConfig& config);
    void registerRequested(const QString& tournamentId);
    void unregisterRequested(const QString& tournamentId);

private slots:
    void onCreateClicked();
    void onRegisterClicked();
    void onRefreshTimer();

private:
    void buildUI();
    void buildTournamentListSection(QVBoxLayout* parent);
    void buildRunningPanel(QVBoxLayout* parent);
    void buildResultsPanel(QVBoxLayout* parent);
    void updateTournamentTable();
    void updateRunningPanel();
    void updateResultsPanel();
    void showCreateDialog();
    QString statusToString(TournamentStatus s) const;
    QString typeToString(TournamentType t) const;

    // Style helpers
    static QString goldButtonStyle();
    static QString greenButtonStyle();
    static QString tableStyle();
    static QString panelStyle();
    static QString headerStyle();
    static QString labelStyle();
    static QString inputStyle();

    PokerServer* m_server = nullptr;
    QString m_playerId;
    QString m_playerName;
    QVector<Tournament*> m_tournaments;
    Tournament* m_activeTournament = nullptr; // the one we're registered in

    // UI elements
    QTableWidget* m_tournamentTable = nullptr;
    QWidget* m_runningPanel = nullptr;
    QLabel* m_blindLevelLabel = nullptr;
    QLabel* m_playersRemainingLabel = nullptr;
    QLabel* m_yourPositionLabel = nullptr;
    QLabel* m_yourStackLabel = nullptr;
    QLabel* m_prizePoolLabel = nullptr;
    QTableWidget* m_leaderboardTable = nullptr;
    QWidget* m_resultsPanel = nullptr;
    QLabel* m_resultPositionLabel = nullptr;
    QLabel* m_resultPrizeLabel = nullptr;
    QTableWidget* m_resultsTable = nullptr;
    QTimer* m_refreshTimer = nullptr;
};
