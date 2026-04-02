#pragma once
#include <QString>
#include <QByteArray>

class ProvablyFair {
public:
    static QString generateServerSeed();
    static QString generateHash(const QString& serverSeed, const QString& clientSeed, int nonce);
    static int hashToNumber(const QString& hash, int min, int max);
    static double hashToFloat(const QString& hash);
    static bool verifyResult(const QString& serverSeed, const QString& clientSeed, int nonce, const QString& expectedHash);
    static QString sha256(const QString& input);

private:
    static QByteArray hmacSha256(const QByteArray& key, const QByteArray& data);
};
