#include "AuctionGame.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRandomGenerator>

AuctionGame::AuctionGame(BettingEngine* engine, QWidget* parent)
    : GameWidget("auction", "Auction", engine, parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &AuctionGame::tick);
    setupUI();
}

void AuctionGame::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\xF0\x9F\x8F\x86 AUCTION"));
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #facc15;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Item display
    auto* itemBox = new QGroupBox("Current Item");
    itemBox->setStyleSheet(
        "QGroupBox { border: 1px solid rgba(250,204,21,0.3); border-radius: 8px; "
        "margin-top: 10px; padding: 16px; padding-top: 20px; color: #facc15; font-weight: bold; }");
    auto* itemLayout = new QVBoxLayout(itemBox);

    // Image placeholder
    auto* imgPlaceholder = new QLabel("[Item Image]");
    imgPlaceholder->setFixedHeight(80);
    imgPlaceholder->setStyleSheet("background: rgba(26,31,46,0.8); color: #666; font-size: 16px; "
        "border-radius: 8px; border: 2px dashed rgba(250,204,21,0.2);");
    imgPlaceholder->setAlignment(Qt::AlignCenter);
    itemLayout->addWidget(imgPlaceholder);

    m_itemNameLabel = new QLabel("No auction active");
    m_itemNameLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #facc15;");
    m_itemNameLabel->setAlignment(Qt::AlignCenter);
    itemLayout->addWidget(m_itemNameLabel);

    m_itemDescLabel = new QLabel("Start a new auction to begin bidding");
    m_itemDescLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    m_itemDescLabel->setAlignment(Qt::AlignCenter);
    m_itemDescLabel->setWordWrap(true);
    itemLayout->addWidget(m_itemDescLabel);
    layout->addWidget(itemBox);

    // Current bid + Timer
    auto* infoLayout = new QHBoxLayout;
    m_currentBidLabel = new QLabel("Current Bid: ---");
    m_currentBidLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #10eb04;");
    m_timerLabel = new QLabel("Time: --");
    m_timerLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #eb0404;");
    infoLayout->addWidget(m_currentBidLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_timerLabel);
    layout->addLayout(infoLayout);

    // Bid controls
    auto* bidLayout = new QHBoxLayout;
    m_bidSpin = new QDoubleSpinBox;
    m_bidSpin->setRange(0, 1000000);
    m_bidSpin->setDecimals(2);
    m_bidSpin->setPrefix("+ ");
    m_bidSpin->setValue(10.0);
    m_bidSpin->setStyleSheet(
        "QDoubleSpinBox { background: #1a1f2e; color: #e0e0e0; border: 1px solid rgba(250,204,21,0.3); "
        "padding: 8px; border-radius: 4px; font-size: 14px; }");
    bidLayout->addWidget(m_bidSpin);

    m_bidBtn = new QPushButton("BID");
    m_bidBtn->setFixedHeight(40);
    m_bidBtn->setEnabled(false);
    m_bidBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #facc15, stop:1 #f59e0b); "
        "color: #0f1419; font-size: 16px; font-weight: bold; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: #fbbf24; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    connect(m_bidBtn, &QPushButton::clicked, this, &AuctionGame::placeBid);
    bidLayout->addWidget(m_bidBtn);
    layout->addLayout(bidLayout);

    // Quick bid buttons
    auto* quickLayout = new QHBoxLayout;
    QString qStyle = "QPushButton { background: rgba(250,204,21,0.08); color: #facc15; "
        "border: 1px solid rgba(250,204,21,0.2); padding: 4px 8px; border-radius: 4px; font-size: 11px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.15); }";
    for (double inc : {5.0, 10.0, 25.0, 50.0, 100.0}) {
        auto* btn = new QPushButton(QString("+%1").arg(inc, 0, 'f', 0));
        btn->setStyleSheet(qStyle);
        connect(btn, &QPushButton::clicked, this, [this, inc]() { m_bidSpin->setValue(inc); });
        quickLayout->addWidget(btn);
    }
    layout->addLayout(quickLayout);

    m_resultLabel = new QLabel("");
    m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setWordWrap(true);
    layout->addWidget(m_resultLabel);

    // Bid history
    auto* histBox = new QGroupBox("Bid History");
    histBox->setStyleSheet(
        "QGroupBox { border: 1px solid rgba(250,204,21,0.15); border-radius: 6px; "
        "margin-top: 6px; padding-top: 14px; color: #facc15; font-size: 11px; }");
    auto* histLayout = new QVBoxLayout(histBox);
    m_historyList = new QListWidget;
    m_historyList->setMaximumHeight(100);
    m_historyList->setStyleSheet(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { color: #aaa; padding: 2px; font-size: 11px; }");
    histLayout->addWidget(m_historyList);
    layout->addWidget(histBox);

    m_newAuctionBtn = new QPushButton("NEW AUCTION");
    m_newAuctionBtn->setFixedHeight(40);
    m_newAuctionBtn->setStyleSheet(
        "QPushButton { background: rgba(250,204,21,0.15); color: #facc15; "
        "font-size: 14px; font-weight: bold; border: 1px solid rgba(250,204,21,0.3); border-radius: 8px; }"
        "QPushButton:hover { background: rgba(250,204,21,0.25); }");
    connect(m_newAuctionBtn, &QPushButton::clicked, this, &AuctionGame::newAuction);
    layout->addWidget(m_newAuctionBtn);

    layout->addStretch();
}

void AuctionGame::newAuction()
{
    static const QVector<QPair<QString, QString>> items = {
        {"Rare BTC NFT #1337", "Limited edition Bitcoin genesis block artwork"},
        {"OmniBus Gold Pass", "Lifetime premium access to OmniBus ecosystem"},
        {"Crypto Hardware Wallet", "Air-gapped cold storage with biometric lock"},
        {"1 ETH Gift Card", "Redeemable Ethereum voucher"},
        {"VIP Trading Bot License", "AI-powered HFT bot for 12 months"},
        {"Mining Rig Share", "10% share of a 1 PH/s mining operation"},
        {"DeFi Yield Bundle", "Pre-configured 15% APY yield farm position"},
        {"Exclusive Domain Name", "Premium .crypto domain for your project"},
    };

    int idx = QRandomGenerator::global()->bounded(items.size());
    double startBid = 10.0 + QRandomGenerator::global()->bounded(90);

    m_currentItem = {
        items[idx].first,
        items[idx].second,
        startBid,
        startBid,
        "---",
        45 // seconds
    };

    m_auctionActive = true;
    m_playerBidCount = 0;
    m_historyList->clear();
    m_resultLabel->setText("");

    m_itemNameLabel->setText(m_currentItem.name);
    m_itemDescLabel->setText(m_currentItem.description);
    m_currentBidLabel->setText(QString("Current: %1 EUR").arg(m_currentItem.currentBid, 0, 'f', 2));
    m_timerLabel->setText(QString("%1s").arg(m_currentItem.timeLeft));
    m_bidSpin->setValue(qMax(5.0, m_currentItem.currentBid * 0.1));
    m_bidBtn->setEnabled(true);
    m_newAuctionBtn->setEnabled(false);

    m_timer->start(1000);
}

void AuctionGame::placeBid()
{
    if (!m_auctionActive) return;

    double increment = m_bidSpin->value();
    double newBid = m_currentItem.currentBid + increment;

    m_currentItem.currentBid = newBid;
    m_currentItem.currentBidder = "You";
    m_playerBidCount++;

    m_currentBidLabel->setText(QString("Current: %1 EUR (You)").arg(newBid, 0, 'f', 2));
    m_historyList->insertItem(0, QString("You bid %1 EUR").arg(newBid, 0, 'f', 2));

    // Extend timer if < 10s (anti-snipe)
    if (m_currentItem.timeLeft < 10)
        m_currentItem.timeLeft = 15;
}

void AuctionGame::tick()
{
    m_currentItem.timeLeft--;
    m_timerLabel->setText(QString("%1s").arg(m_currentItem.timeLeft));

    // Color based on time
    if (m_currentItem.timeLeft <= 10)
        m_timerLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #eb0404;");
    else
        m_timerLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #facc15;");

    // AI bidding (random chance)
    if (m_currentItem.timeLeft > 3 && QRandomGenerator::global()->bounded(100) < 25) {
        aiBid();
    }

    if (m_currentItem.timeLeft <= 0) {
        m_timer->stop();
        endAuction();
    }
}

void AuctionGame::aiBid()
{
    static const QStringList aiNames = {"CryptoKing", "WhaleBot", "DeFiDegen", "NFT_Hunter", "SatoshiFan"};
    QString aiName = aiNames[QRandomGenerator::global()->bounded(aiNames.size())];

    double increment = 5.0 + QRandomGenerator::global()->bounded(20);
    m_currentItem.currentBid += increment;
    m_currentItem.currentBidder = aiName;

    m_currentBidLabel->setText(QString("Current: %1 EUR (%2)")
        .arg(m_currentItem.currentBid, 0, 'f', 2).arg(aiName));
    m_historyList->insertItem(0, QString("%1 bid %2 EUR")
        .arg(aiName).arg(m_currentItem.currentBid, 0, 'f', 2));

    // Extend if < 5s
    if (m_currentItem.timeLeft < 5)
        m_currentItem.timeLeft = 8;
}

void AuctionGame::endAuction()
{
    m_auctionActive = false;
    m_bidBtn->setEnabled(false);
    m_newAuctionBtn->setEnabled(true);

    bool won = (m_currentItem.currentBidder == "You");

    if (won) {
        m_resultLabel->setText(QString("YOU WON! %1 for %2 EUR")
            .arg(m_currentItem.name).arg(m_currentItem.currentBid, 0, 'f', 2));
        m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #10eb04;");
        // Deduct from balance (the total bid amount minus what engine already took)
        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, true);
    } else {
        m_resultLabel->setText(QString("%1 won by %2 for %3 EUR")
            .arg(m_currentItem.name, m_currentItem.currentBidder)
            .arg(m_currentItem.currentBid, 0, 'f', 2));
        m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #eb0404;");
        auto bets = m_engine->allBets();
        if (!bets.isEmpty())
            m_engine->settleBet(bets.last().id, false);
    }

    emit gameFinished(won, 0);
}

void AuctionGame::startGame() { newAuction(); }
void AuctionGame::resetGame()
{
    m_timer->stop();
    m_auctionActive = false;
    m_itemNameLabel->setText("No auction active");
    m_itemDescLabel->setText("Start a new auction to begin bidding");
    m_currentBidLabel->setText("Current Bid: ---");
    m_timerLabel->setText("Time: --");
    m_resultLabel->setText("");
    m_historyList->clear();
    m_bidBtn->setEnabled(false);
    m_newAuctionBtn->setEnabled(true);
}
