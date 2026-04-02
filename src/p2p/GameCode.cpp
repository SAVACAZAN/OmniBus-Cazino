#include "GameCode.h"
#include <QRandomGenerator>
#include <QStringList>

const QString GameCode::BASE36_CHARS = QStringLiteral("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");

QString GameCode::toBase36(quint32 value, int minDigits)
{
    if (value == 0 && minDigits <= 1)
        return QStringLiteral("0");

    QString result;
    quint32 v = value;
    while (v > 0) {
        result.prepend(BASE36_CHARS[static_cast<int>(v % 36)]);
        v /= 36;
    }
    // Pad to minimum digits
    while (result.size() < minDigits)
        result.prepend('0');

    return result;
}

quint32 GameCode::fromBase36(const QString& str)
{
    quint32 result = 0;
    for (QChar c : str) {
        int idx = BASE36_CHARS.indexOf(c.toUpper());
        if (idx < 0) return 0;
        result = result * 36 + static_cast<quint32>(idx);
    }
    return result;
}

QString GameCode::generate()
{
    QString code = QStringLiteral("OMNI-");
    auto* rng = QRandomGenerator::global();
    for (int i = 0; i < 4; ++i) {
        code.append(BASE36_CHARS[rng->bounded(36)]);
    }
    return code;
}

QString GameCode::encode(const QString& ip, quint16 port)
{
    // Split IP into octets
    QStringList parts = ip.split('.');
    if (parts.size() != 4)
        return QString();

    // Each octet (0-255) → 2-char base36 (max "73" for 255)
    QString encoded;
    for (const QString& part : parts) {
        quint32 octet = part.toUInt();
        encoded += toBase36(octet, 2);
    }

    // Port (0-65535) → 3-char base36 (max "1EKF" for 65535, but 3 chars covers up to 46655)
    // Use variable length for port to handle full range
    encoded += toBase36(static_cast<quint32>(port), 3);

    return QStringLiteral("OMNI-") + encoded;
}

QPair<QString, quint16> GameCode::decode(const QString& code)
{
    if (!isValid(code))
        return {QString(), 0};

    // Strip "OMNI-" prefix
    QString encoded = code.mid(5).toUpper();

    // Need at least 8 chars for IP (4 octets x 2 chars) + at least 1 for port
    if (encoded.size() < 9)
        return {QString(), 0};

    // First 8 chars = 4 octets (2 chars each)
    QStringList octets;
    for (int i = 0; i < 4; ++i) {
        quint32 val = fromBase36(encoded.mid(i * 2, 2));
        if (val > 255)
            return {QString(), 0};
        octets.append(QString::number(val));
    }

    // Remaining chars = port
    QString portStr = encoded.mid(8);
    quint32 portVal = fromBase36(portStr);
    if (portVal > 65535)
        return {QString(), 0};

    QString ip = octets.join('.');
    return {ip, static_cast<quint16>(portVal)};
}

bool GameCode::isValid(const QString& code)
{
    if (!code.startsWith(QStringLiteral("OMNI-"), Qt::CaseInsensitive))
        return false;

    QString payload = code.mid(5);
    if (payload.isEmpty())
        return false;

    for (QChar c : payload) {
        if (BASE36_CHARS.indexOf(c.toUpper()) < 0)
            return false;
    }
    return true;
}
