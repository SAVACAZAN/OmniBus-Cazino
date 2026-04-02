#pragma once
#include "ui/GameWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVector>
#include <QProgressBar>

struct Racer {
    QString name;
    double odds;
    double progress = 0.0;
    double speed = 0.0;
    int position = 0;
};

class RaceBettingGame : public GameWidget {
    Q_OBJECT
public:
    explicit RaceBettingGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void tick();
    void finishRace();

    QLabel* m_resultLabel;
    QLabel* m_selectedLabel;
    QPushButton* m_startBtn;
    QVector<QPushButton*> m_racerBtns;
    QTimer* m_raceTimer;

    QVector<Racer> m_racers;
    int m_selectedRacer = -1;
    bool m_racing = false;
    bool m_betOnPlace = false; // false=win, true=place (top 3)
    QPushButton* m_winBtn;
    QPushButton* m_placeBtn;
};
