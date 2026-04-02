#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * MentalPoker — Distributed card shuffling protocol.
 *
 * Ensures no single player can cheat the shuffle:
 *   1. Each player generates encryption keys
 *   2. Deck is created as card indices 0-51
 *   3. Each player encrypts the entire deck with their key
 *   4. Each player shuffles the encrypted deck
 *   5. Cards are decrypted when dealt or at showdown
 *   6. All keys revealed at end for verification
 *
 * Uses XOR-based commutative encryption:
 *   E(k1, E(k2, x)) == E(k2, E(k1, x))
 *
 * Combined with SHA-256 seed commitments for provable fairness.
 */
class MentalPoker {
public:
    MentalPoker();

    struct PlayerKey {
        QByteArray publicKey;   // For XOR scheme, same as privateKey (symmetric)
        QByteArray privateKey;  // 256-bit random key
        QString playerId;
    };

    /// Phase 1: Generate a 256-bit encryption key pair using OpenSSL CSPRNG
    PlayerKey generateKeys(const QString& playerId = QString());

    /// Phase 2: Create initial deck — 52 cards as padded byte arrays (indices 0-51)
    QVector<QByteArray> createInitialDeck();

    /// Phase 3: Encrypt entire deck with player's key (XOR each card with key)
    QVector<QByteArray> encryptDeck(const QVector<QByteArray>& deck,
                                     const QByteArray& key);

    /// Phase 4: Fisher-Yates shuffle of encrypted deck
    QVector<QByteArray> shuffleDeck(const QVector<QByteArray>& encryptedDeck);

    /// Phase 5: Decrypt a single card — returns card index 0-51
    int decryptCard(const QByteArray& encryptedCard, const QByteArray& key);

    /// Decrypt a card through multiple keys (peel layers)
    int decryptCardMultiKey(const QByteArray& encryptedCard,
                            const QVector<QByteArray>& keys);

    /// Phase 6: Verify entire deck — all keys revealed, verify card assignments
    bool verifyDeck(const QVector<QByteArray>& originalEncrypted,
                    const QVector<PlayerKey>& allKeys,
                    const QVector<int>& expectedCards);

    /// Combined seed from all players: SHA256(seed1:seed2:seed3:...)
    static QString combinedSeed(const QStringList& playerSeeds);

    /// Hash commitment: player commits hash before revealing seed
    static QString commitSeed(const QString& seed);

    /// Verify that a seed matches its previously shared commitment
    static bool verifyCommitment(const QString& seed, const QString& commitment);

    /// Convert card index (0-51) to human-readable string
    static QString cardName(int index);

private:
    /// XOR two byte arrays (commutative encryption primitive)
    static QByteArray xorBytes(const QByteArray& data, const QByteArray& key);

    /// Pad a card index to fixed-size byte array
    static QByteArray padCard(int cardIndex);

    /// Extract card index from padded byte array
    static int unpadCard(const QByteArray& data);

    static constexpr int KEY_SIZE = 32;   // 256 bits
    static constexpr int CARD_SIZE = 32;  // Pad cards to key size for XOR
    static constexpr int DECK_SIZE = 52;
};
