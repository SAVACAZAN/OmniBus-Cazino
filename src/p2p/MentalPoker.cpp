#include "MentalPoker.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <openssl/rand.h>
#include <algorithm>
#include <cstring>

MentalPoker::MentalPoker()
{
}

MentalPoker::PlayerKey MentalPoker::generateKeys(const QString& playerId)
{
    PlayerKey key;
    key.playerId = playerId;

    // Generate 256-bit random key using OpenSSL CSPRNG
    key.privateKey.resize(KEY_SIZE);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(key.privateKey.data()), KEY_SIZE) != 1) {
        // Fallback to Qt secure RNG if OpenSSL fails
        QRandomGenerator rng = QRandomGenerator::securelySeeded();
        for (int i = 0; i < KEY_SIZE; ++i) {
            key.privateKey[i] = static_cast<char>(rng.bounded(256));
        }
    }

    // For XOR-based commutative encryption, public key == private key
    // In a real implementation, this would use RSA/ECC commutative scheme
    key.publicKey = key.privateKey;

    return key;
}

QVector<QByteArray> MentalPoker::createInitialDeck()
{
    QVector<QByteArray> deck;
    deck.reserve(DECK_SIZE);
    for (int i = 0; i < DECK_SIZE; ++i) {
        deck.append(padCard(i));
    }
    return deck;
}

QByteArray MentalPoker::padCard(int cardIndex)
{
    // Create a 32-byte array with the card index embedded
    // Fill with deterministic padding based on index (prevents trivial analysis)
    QByteArray padded(CARD_SIZE, 0);

    // First byte: card index
    padded[0] = static_cast<char>(cardIndex & 0xFF);

    // Bytes 1-3: verification pattern
    padded[1] = static_cast<char>(0xCA);  // "CA" for card
    padded[2] = static_cast<char>(0xFE);
    padded[3] = static_cast<char>((cardIndex * 7 + 13) & 0xFF);  // Simple checksum

    // Remaining bytes: deterministic fill from SHA-256 of index
    QByteArray indexHash = QCryptographicHash::hash(
        QByteArray::number(cardIndex), QCryptographicHash::Sha256);
    for (int i = 4; i < CARD_SIZE && (i - 4) < indexHash.size(); ++i) {
        padded[i] = indexHash[i - 4];
    }

    return padded;
}

int MentalPoker::unpadCard(const QByteArray& data)
{
    if (data.size() < 4)
        return -1;

    int cardIndex = static_cast<unsigned char>(data[0]);

    // Verify checksum
    if (static_cast<unsigned char>(data[1]) != 0xCA ||
        static_cast<unsigned char>(data[2]) != 0xFE) {
        return -1;  // Corrupted
    }

    unsigned char expectedCheck = static_cast<unsigned char>((cardIndex * 7 + 13) & 0xFF);
    if (static_cast<unsigned char>(data[3]) != expectedCheck)
        return -1;

    if (cardIndex < 0 || cardIndex >= DECK_SIZE)
        return -1;

    return cardIndex;
}

QByteArray MentalPoker::xorBytes(const QByteArray& data, const QByteArray& key)
{
    QByteArray result(data.size(), 0);
    for (int i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    return result;
}

QVector<QByteArray> MentalPoker::encryptDeck(const QVector<QByteArray>& deck,
                                              const QByteArray& key)
{
    QVector<QByteArray> encrypted;
    encrypted.reserve(deck.size());
    for (const auto& card : deck) {
        encrypted.append(xorBytes(card, key));
    }
    return encrypted;
}

QVector<QByteArray> MentalPoker::shuffleDeck(const QVector<QByteArray>& encryptedDeck)
{
    QVector<QByteArray> shuffled = encryptedDeck;

    // Fisher-Yates shuffle using cryptographic RNG
    auto* rng = QRandomGenerator::global();
    for (int i = shuffled.size() - 1; i > 0; --i) {
        int j = rng->bounded(i + 1);
        shuffled.swapItemsAt(i, j);
    }

    return shuffled;
}

int MentalPoker::decryptCard(const QByteArray& encryptedCard, const QByteArray& key)
{
    QByteArray decrypted = xorBytes(encryptedCard, key);
    return unpadCard(decrypted);
}

int MentalPoker::decryptCardMultiKey(const QByteArray& encryptedCard,
                                      const QVector<QByteArray>& keys)
{
    // XOR is commutative and self-inverse, so we just XOR with all keys
    QByteArray current = encryptedCard;
    for (const auto& key : keys) {
        current = xorBytes(current, key);
    }
    return unpadCard(current);
}

bool MentalPoker::verifyDeck(const QVector<QByteArray>& originalEncrypted,
                              const QVector<PlayerKey>& allKeys,
                              const QVector<int>& expectedCards)
{
    if (originalEncrypted.size() != expectedCards.size())
        return false;

    // Collect all keys
    QVector<QByteArray> keys;
    keys.reserve(allKeys.size());
    for (const auto& pk : allKeys) {
        keys.append(pk.privateKey);
    }

    // Decrypt each card and verify it matches expected
    for (int i = 0; i < originalEncrypted.size(); ++i) {
        int decrypted = decryptCardMultiKey(originalEncrypted[i], keys);
        if (decrypted != expectedCards[i])
            return false;
    }

    // Verify all 52 cards are present (no duplicates)
    QVector<bool> seen(DECK_SIZE, false);
    for (int card : expectedCards) {
        if (card < 0 || card >= DECK_SIZE)
            return false;
        if (seen[card])
            return false;  // Duplicate card
        seen[card] = true;
    }

    return true;
}

QString MentalPoker::combinedSeed(const QStringList& playerSeeds)
{
    QString combined = playerSeeds.join(':');
    QByteArray hash = QCryptographicHash::hash(
        combined.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

QString MentalPoker::commitSeed(const QString& seed)
{
    QByteArray hash = QCryptographicHash::hash(
        seed.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

bool MentalPoker::verifyCommitment(const QString& seed, const QString& commitment)
{
    return commitSeed(seed) == commitment;
}

QString MentalPoker::cardName(int index)
{
    if (index < 0 || index >= DECK_SIZE)
        return QStringLiteral("??");

    static const char* suits[] = {"Spades", "Hearts", "Diamonds", "Clubs"};
    static const char* ranks[] = {
        "2", "3", "4", "5", "6", "7", "8", "9", "10",
        "Jack", "Queen", "King", "Ace"
    };

    int suit = index / 13;
    int rank = index % 13;

    return QString("%1 of %2").arg(ranks[rank], suits[suit]);
}
