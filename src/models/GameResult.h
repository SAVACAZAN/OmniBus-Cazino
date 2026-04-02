#pragma once
#include <QString>

struct GameResult {
    QString gameId;
    QString resultData; // game-specific JSON
    QString serverSeed;
    QString clientSeed;
    int nonce = 0;
    QString hash; // SHA-256 verification
    bool verified = false;
};
