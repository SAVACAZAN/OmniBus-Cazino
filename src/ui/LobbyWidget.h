#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include "network/PokerClient.h"

class LobbyWidget : public QWidget {
    Q_OBJECT
public:
    explicit LobbyWidget(PokerClient* client, QWidget* parent = nullptr);

signals:
    void joinedTable(const QString& tableId, int seat, const QJsonObject& state);

private slots:
    void onLoginOk(const QString& playerId, double balance);
    void onTableListReceived(const QJsonArray& tables);
    void onTableJoined(const QString& tableId, int seat, const QJsonObject& state);
    void onError(const QString& message);
    void onConnected();
    void onDisconnected();

private:
    void setupUI();
    void connectToServer();
    void refreshTables();
    void showCreateTableDialog();
    void joinSelectedTable();
    void updateStatus(const QString& text, const QString& color = "#aaa");

    PokerClient* m_client;

    // UI elements
    QLabel* m_titleLabel;
    QLabel* m_statusLabel;
    QLabel* m_playerInfoLabel;
    QTableWidget* m_tableList;
    QPushButton* m_connectBtn;
    QPushButton* m_refreshBtn;
    QPushButton* m_createTableBtn;
    QLineEdit* m_nameEdit;
    QLineEdit* m_serverEdit;      // ws://IP:PORT
    QLineEdit* m_passwordEdit;    // account password
    QDoubleSpinBox* m_balanceSpin;

    QTimer* m_refreshTimer;
    bool m_connected = false;
};
