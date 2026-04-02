#pragma once
#include "ui/GameWidget.h"
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QVector>

class CrashGame : public GameWidget {
    Q_OBJECT
public:
    explicit CrashGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void tick();
    void cashOut();
    void generateCrashPoint();
    void updateHistoryLabel();

    QLabel* m_multiplierLabel;
    QPushButton* m_cashOutBtn;
    QPushButton* m_startBtn;
    QLabel* m_historyLabel;
    QTimer* m_timer;

    double m_currentMultiplier = 1.0;
    double m_crashPoint = 0.0;
    bool m_running = false;
    bool m_cashedOut = false;
    QVector<double> m_crashHistory;
    int m_tickCount = 0;
};
