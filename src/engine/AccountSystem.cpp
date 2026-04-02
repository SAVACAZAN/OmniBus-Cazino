#include "AccountSystem.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDir>

// ===== PlayerAccount JSON serialization =====

QJsonObject PlayerAccount::toJson() const {
    QJsonObject obj;
    obj["uid"] = uid;
    obj["username"] = username;
    obj["password_hash"] = passwordHash;
    obj["salt"] = salt;
    obj["email"] = email;
    obj["avatar"] = avatarEmoji;
    obj["wallet"] = walletAddress;
    obj["display_name"] = displayName;
    obj["hands_played"] = handsPlayed;
    obj["hands_won"] = handsWon;
    obj["total_wagered"] = totalWagered;
    obj["total_won"] = totalWon;
    obj["total_profit"] = totalProfit;
    obj["tournament_wins"] = tournamentWins;
    obj["best_hand_pot"] = bestHandPot;
    obj["created_at"] = createdAt.toString(Qt::ISODate);
    obj["last_login"] = lastLogin.toString(Qt::ISODate);
    obj["is_guest"] = isGuest;
    obj["level"] = level;
    return obj;
}

PlayerAccount PlayerAccount::fromJson(const QJsonObject& obj) {
    PlayerAccount a;
    a.uid = obj["uid"].toString();
    a.username = obj["username"].toString();
    a.passwordHash = obj["password_hash"].toString();
    a.salt = obj["salt"].toString();
    a.email = obj["email"].toString();
    a.avatarEmoji = obj["avatar"].toString("😎");
    a.walletAddress = obj["wallet"].toString();
    a.displayName = obj["display_name"].toString(a.username);
    a.handsPlayed = obj["hands_played"].toInt();
    a.handsWon = obj["hands_won"].toInt();
    a.totalWagered = obj["total_wagered"].toDouble();
    a.totalWon = obj["total_won"].toDouble();
    a.totalProfit = obj["total_profit"].toDouble();
    a.tournamentWins = obj["tournament_wins"].toInt();
    a.bestHandPot = obj["best_hand_pot"].toDouble();
    a.createdAt = QDateTime::fromString(obj["created_at"].toString(), Qt::ISODate);
    a.lastLogin = QDateTime::fromString(obj["last_login"].toString(), Qt::ISODate);
    a.isGuest = obj["is_guest"].toBool();
    a.level = obj["level"].toInt(1);
    return a;
}

// ===== AccountSystem =====

AccountSystem::AccountSystem(QObject* parent)
    : QObject(parent)
{
    load();
}

QString AccountSystem::generateUID() {
    // 64-char hex UID (256 bits) — used for seed validation between players
    // Combines 2 UUIDs + timestamp + random bytes → SHA-256 hash
    // Result: 64 hex chars, collision-resistant, unique across all instances
    QByteArray entropy;
    entropy.append(QUuid::createUuid().toByteArray());
    entropy.append(QUuid::createUuid().toByteArray());
    entropy.append(QString::number(QDateTime::currentMSecsSinceEpoch()).toUtf8());

    // Add 32 bytes of OpenSSL CSPRNG randomness
    QByteArray rng(32, 0);
    QRandomGenerator gen = QRandomGenerator::securelySeeded();
    for (int i = 0; i < 32; ++i)
        rng[i] = static_cast<char>(gen.bounded(256));
    entropy.append(rng);

    // SHA-256 of everything → 64 hex chars
    return QCryptographicHash::hash(entropy, QCryptographicHash::Sha256).toHex();
}

QString AccountSystem::generateSalt() {
    QByteArray salt(16, 0);
    QRandomGenerator rng = QRandomGenerator::securelySeeded();
    for (int i = 0; i < 16; ++i)
        salt[i] = static_cast<char>(rng.bounded(256));
    return salt.toHex();
}

QString AccountSystem::hashPassword(const QString& password, const QString& salt) {
    QByteArray data = (password + salt).toUtf8();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

QString AccountSystem::getDeviceUID() {
    QString path = "device_id.txt";
    QFile file(path);

    // Read existing
    if (file.open(QIODevice::ReadOnly)) {
        QString uid = QString::fromUtf8(file.readAll()).trimmed();
        file.close();
        if (!uid.isEmpty()) return uid;
    }

    // Generate new
    QString uid = generateUID();
    if (file.open(QIODevice::WriteOnly)) {
        file.write(uid.toUtf8());
        file.close();
    }
    return uid;
}

// ── Registration ──

bool AccountSystem::registerAccount(const QString& username, const QString& password,
                                      const QString& email, const QString& avatar) {
    // Validate
    if (username.length() < 3) {
        emit registerFailed("Username must be at least 3 characters");
        return false;
    }
    if (password.length() < 4) {
        emit registerFailed("Password must be at least 4 characters");
        return false;
    }
    if (m_usernameToUid.contains(username.toLower())) {
        emit registerFailed("Username already taken");
        return false;
    }

    // Create account
    PlayerAccount account;
    account.uid = generateUID();
    account.username = username;
    account.displayName = username;
    account.email = email;
    account.avatarEmoji = avatar.isEmpty() ? "😎" : avatar;
    account.salt = generateSalt();
    account.passwordHash = hashPassword(password, account.salt);
    account.createdAt = QDateTime::currentDateTime();
    account.lastLogin = account.createdAt;
    account.isGuest = false;

    m_accounts[account.uid] = account;
    m_usernameToUid[username.toLower()] = account.uid;

    save();
    emit registerSuccess(account.uid);

    // Auto-login after register
    m_currentAccount = account;
    m_loggedIn = true;
    emit loginSuccess(account.uid, account.username);

    return true;
}

// ── Login ──

bool AccountSystem::login(const QString& username, const QString& password) {
    auto it = m_usernameToUid.find(username.toLower());
    if (it == m_usernameToUid.end()) {
        emit loginFailed("Username not found");
        return false;
    }

    auto& account = m_accounts[it.value()];
    if (hashPassword(password, account.salt) != account.passwordHash) {
        emit loginFailed("Wrong password");
        return false;
    }

    account.lastLogin = QDateTime::currentDateTime();
    m_currentAccount = account;
    m_loggedIn = true;
    save();

    emit loginSuccess(account.uid, account.username);
    return true;
}

void AccountSystem::loginAsGuest(const QString& displayName) {
    PlayerAccount guest;
    guest.uid = "guest_" + generateUID().left(8);
    guest.username = "guest_" + QString::number(QRandomGenerator::global()->bounded(9999));
    guest.displayName = displayName.isEmpty() ? guest.username : displayName;
    guest.avatarEmoji = "🎭";
    guest.isGuest = true;
    guest.createdAt = QDateTime::currentDateTime();
    guest.lastLogin = guest.createdAt;

    m_currentAccount = guest;
    m_loggedIn = true;

    emit loginSuccess(guest.uid, guest.displayName);
}

void AccountSystem::logout() {
    m_loggedIn = false;
    m_currentAccount = PlayerAccount();
    emit loggedOut();
}

// ── Profile ──

void AccountSystem::setAvatar(const QString& emoji) {
    m_currentAccount.avatarEmoji = emoji;
    if (!m_currentAccount.isGuest && m_accounts.contains(m_currentAccount.uid)) {
        m_accounts[m_currentAccount.uid].avatarEmoji = emoji;
        save();
    }
    emit accountUpdated();
}

void AccountSystem::setWalletAddress(const QString& addr) {
    m_currentAccount.walletAddress = addr;
    if (!m_currentAccount.isGuest && m_accounts.contains(m_currentAccount.uid)) {
        m_accounts[m_currentAccount.uid].walletAddress = addr;
        save();
    }
    emit accountUpdated();
}

void AccountSystem::setDisplayName(const QString& name) {
    m_currentAccount.displayName = name;
    if (!m_currentAccount.isGuest && m_accounts.contains(m_currentAccount.uid)) {
        m_accounts[m_currentAccount.uid].displayName = name;
        save();
    }
    emit accountUpdated();
}

// ── Stats ──

void AccountSystem::recordHandPlayed() {
    m_currentAccount.handsPlayed++;
    if (!m_currentAccount.isGuest && m_accounts.contains(m_currentAccount.uid))
        m_accounts[m_currentAccount.uid].handsPlayed++;
}

void AccountSystem::recordHandWon(double potAmount) {
    m_currentAccount.handsWon++;
    m_currentAccount.totalWon += potAmount;
    m_currentAccount.totalProfit += potAmount;
    if (potAmount > m_currentAccount.bestHandPot)
        m_currentAccount.bestHandPot = potAmount;

    if (!m_currentAccount.isGuest && m_accounts.contains(m_currentAccount.uid)) {
        auto& a = m_accounts[m_currentAccount.uid];
        a.handsWon++;
        a.totalWon += potAmount;
        a.totalProfit += potAmount;
        if (potAmount > a.bestHandPot) a.bestHandPot = potAmount;
    }
}

void AccountSystem::recordWager(double amount) {
    m_currentAccount.totalWagered += amount;
    m_currentAccount.totalProfit -= amount;
    if (!m_currentAccount.isGuest && m_accounts.contains(m_currentAccount.uid)) {
        m_accounts[m_currentAccount.uid].totalWagered += amount;
        m_accounts[m_currentAccount.uid].totalProfit -= amount;
    }
}

// ── Server-side verification ──

bool AccountSystem::verifyPassword(const QString& username, const QString& password) const {
    auto it = m_usernameToUid.find(username.toLower());
    if (it == m_usernameToUid.end()) return false;
    const auto& account = m_accounts[it.value()];
    return hashPassword(password, account.salt) == account.passwordHash;
}

PlayerAccount* AccountSystem::getAccount(const QString& uid) {
    auto it = m_accounts.find(uid);
    return it != m_accounts.end() ? &it.value() : nullptr;
}

PlayerAccount* AccountSystem::getAccountByUsername(const QString& username) {
    auto it = m_usernameToUid.find(username.toLower());
    if (it == m_usernameToUid.end()) return nullptr;
    return getAccount(it.value());
}

QVector<PlayerAccount> AccountSystem::allAccounts() const {
    QVector<PlayerAccount> result;
    for (const auto& a : m_accounts)
        if (!a.isGuest) result.append(a);
    return result;
}

// ── Persistence ──

void AccountSystem::save(const QString& path) {
    QJsonArray arr;
    for (const auto& a : m_accounts) {
        if (!a.isGuest)
            arr.append(a.toJson());
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(arr).toJson());
}

void AccountSystem::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;

    m_accounts.clear();
    m_usernameToUid.clear();

    for (const auto& item : doc.array()) {
        PlayerAccount a = PlayerAccount::fromJson(item.toObject());
        m_accounts[a.uid] = a;
        m_usernameToUid[a.username.toLower()] = a.uid;
    }
}
