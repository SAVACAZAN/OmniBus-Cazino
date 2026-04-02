#include "ProvablyFair.h"
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cmath>

QString ProvablyFair::generateServerSeed()
{
    QByteArray seed(32, 0);
    for (int i = 0; i < 32; ++i)
        seed[i] = static_cast<char>(QRandomGenerator::securelySeeded().bounded(256));
    return QString::fromLatin1(seed.toHex());
}

QByteArray ProvablyFair::hmacSha256(const QByteArray& key, const QByteArray& data)
{
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.constData(), key.size(),
         reinterpret_cast<const unsigned char*>(data.constData()), data.size(),
         result, &len);
    return QByteArray(reinterpret_cast<char*>(result), static_cast<int>(len));
}

QString ProvablyFair::generateHash(const QString& serverSeed, const QString& clientSeed, int nonce)
{
    QByteArray key = serverSeed.toUtf8();
    QByteArray data = (clientSeed + ":" + QString::number(nonce)).toUtf8();
    QByteArray hash = hmacSha256(key, data);
    return QString::fromLatin1(hash.toHex());
}

int ProvablyFair::hashToNumber(const QString& hash, int min, int max)
{
    if (max <= min) return min;
    // Take first 8 hex chars -> 32-bit value
    bool ok = false;
    quint32 val = hash.left(8).toUInt(&ok, 16);
    if (!ok) val = 0;
    double normalized = static_cast<double>(val) / 4294967295.0; // 0xFFFFFFFF
    return min + static_cast<int>(normalized * (max - min));
}

double ProvablyFair::hashToFloat(const QString& hash)
{
    bool ok = false;
    quint32 val = hash.left(8).toUInt(&ok, 16);
    if (!ok) val = 0;
    return static_cast<double>(val) / 4294967295.0;
}

bool ProvablyFair::verifyResult(const QString& serverSeed, const QString& clientSeed, int nonce, const QString& expectedHash)
{
    QString computed = generateHash(serverSeed, clientSeed, nonce);
    return computed == expectedHash;
}

QString ProvablyFair::sha256(const QString& input)
{
    QByteArray hash = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}
