#pragma once
#include "ui/GameWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVector>

class SlotsGame : public GameWidget {
    Q_OBJECT
public:
    explicit SlotsGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void spin();
    void stopReel(int reel);
    void checkWin();
    double symbolMultiplier(const QString& symbol) const;

    QPushButton* m_spinBtn;
    QLabel* m_resultLabel;
    QLabel* m_payoutLabel;
    QTimer* m_spinTimer;

    static constexpr int REELS = 5;
    static constexpr int ROWS = 3;
    QString m_grid[REELS][ROWS];
    int m_spinCountdown[REELS]; // ticks until reel stops
    bool m_spinning = false;
    int m_freeSpins = 0;

    static const QStringList s_symbols;
};
