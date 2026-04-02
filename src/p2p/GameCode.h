#pragma once
#include <QString>
#include <QPair>

/**
 * GameCode — Encode/decode IP:port into shareable game codes.
 *
 * Format: "OMNI-<encoded>" where encoded is base36 of IP octets + port.
 * Example: 192.168.1.130:9999 → "OMNI-5G4W1Q3M7PR"
 *
 * Also supports random codes via generate() for lobby display.
 */
class GameCode {
public:
    /// Generate a random short game code: "OMNI-XXXX" (4 alphanumeric chars)
    static QString generate();

    /// Encode IP:port into a deterministic game code
    static QString encode(const QString& ip, quint16 port);

    /// Decode game code back to IP:port pair
    static QPair<QString, quint16> decode(const QString& code);

    /// Validate code format (starts with "OMNI-", valid base36 chars)
    static bool isValid(const QString& code);

private:
    static const QString BASE36_CHARS;
    static QString toBase36(quint32 value, int minDigits = 1);
    static quint32 fromBase36(const QString& str);
};
