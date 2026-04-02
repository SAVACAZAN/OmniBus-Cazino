#pragma once
#include "ui/GameWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVector>

class BinaryOptionsGame : public GameWidget {
    Q_OBJECT
public:
    explicit BinaryOptionsGame(BettingEngine* engine, QWidget* parent = nullptr);
    void startGame() override;
    void resetGame() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void predict(bool up);
    void tick();
    void generatePrice();
    void resolve();

    QLabel* m_priceLabel;
    QLabel* m_timerLabel;
    QLabel* m_directionLabel;
    QLabel* m_resultLabel;
    QPushButton* m_upBtn;
    QPushButton* m_downBtn;
    QTimer* m_timer;

    QVector<double> m_prices;
    double m_entryPrice = 0.0;
    bool m_predictedUp = true;
    bool m_active = false;
    int m_countdown = 30;
    int m_maxPrices = 40;
};
