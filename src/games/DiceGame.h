#pragma once
#include "ui/GameWidget.h"
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

class DiceGame : public GameWidget {
    Q_OBJECT
public:
    explicit DiceGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void roll(bool over);
    void updateMultiplier();

    QSlider* m_targetSlider;
    QLabel* m_targetLabel;
    QLabel* m_multiplierLabel;
    QLabel* m_winChanceLabel;
    QLabel* m_resultLabel;
    QPushButton* m_rollOverBtn;
    QPushButton* m_rollUnderBtn;

    int m_target = 50;
    int m_lastRoll = -1;
    bool m_lastOver = true;
    bool m_rolling = false;
    int m_animFrame = 0;
    QTimer* m_animTimer;
};
