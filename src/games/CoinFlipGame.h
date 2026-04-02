#pragma once
#include "ui/GameWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QTimer>

class CoinFlipGame : public GameWidget {
    Q_OBJECT
public:
    explicit CoinFlipGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void flip(bool heads);

    QLabel* m_resultLabel;
    QLabel* m_streakLabel;
    QPushButton* m_headsBtn;
    QPushButton* m_tailsBtn;
    QPushButton* m_doubleOrNothingBtn;
    QTimer* m_animTimer;

    bool m_lastChoice = true; // true=heads
    bool m_lastResult = true;
    int m_streak = 0;
    int m_animFrame = 0;
    bool m_flipping = false;
    double m_pendingPayout = 0.0;
};
