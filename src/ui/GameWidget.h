#pragma once
#include <QWidget>
#include "engine/BettingEngine.h"

class GameWidget : public QWidget {
    Q_OBJECT
public:
    explicit GameWidget(const QString& gameId, const QString& gameName,
                        BettingEngine* engine, QWidget* parent = nullptr)
        : QWidget(parent), m_gameId(gameId), m_gameName(gameName), m_engine(engine) {}

    virtual void startGame() = 0;
    virtual void resetGame() = 0;
    QString gameId() const { return m_gameId; }
    QString gameName() const { return m_gameName; }

signals:
    void gameFinished(bool won, double payout);

protected:
    QString m_gameId;
    QString m_gameName;
    BettingEngine* m_engine;
};
