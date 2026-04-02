#pragma once
#include <QWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include "engine/BettingEngine.h"

class BetPanel : public QWidget {
    Q_OBJECT
public:
    explicit BetPanel(BettingEngine* engine, QWidget* parent = nullptr);

    double betAmount() const;
    QString currency() const;
    void setEnabled(bool enabled);
    void updateBalance();

signals:
    void betRequested(double amount, const QString& currency);

private:
    void setupUI();

    BettingEngine* m_engine;
    QDoubleSpinBox* m_amountSpin;
    QComboBox* m_currencyCombo;
    QLabel* m_balanceLabel;
    QPushButton* m_betButton;
    QPushButton* m_minBtn;
    QPushButton* m_halfBtn;
    QPushButton* m_doubleBtn;
    QPushButton* m_maxBtn;
};
