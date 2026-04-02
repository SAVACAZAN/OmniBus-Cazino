#pragma once
#include "ui/GameWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTimer>
#include <QDoubleSpinBox>

struct AuctionItem {
    QString name;
    QString description;
    double startingBid;
    double currentBid;
    QString currentBidder;
    int timeLeft; // seconds
};

class AuctionGame : public GameWidget {
    Q_OBJECT
public:
    explicit AuctionGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

private:
    void setupUI();
    void placeBid();
    void tick();
    void newAuction();
    void endAuction();
    void aiBid();

    QLabel* m_itemNameLabel;
    QLabel* m_itemDescLabel;
    QLabel* m_currentBidLabel;
    QLabel* m_timerLabel;
    QLabel* m_resultLabel;
    QDoubleSpinBox* m_bidSpin;
    QPushButton* m_bidBtn;
    QPushButton* m_newAuctionBtn;
    QListWidget* m_historyList;
    QTimer* m_timer;

    AuctionItem m_currentItem;
    bool m_auctionActive = false;
    int m_playerBidCount = 0;
};
