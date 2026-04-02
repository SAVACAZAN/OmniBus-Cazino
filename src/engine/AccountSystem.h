#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QUuid>
#include <QCryptographicHash>
#include <QDateTime>
#include <QMap>

// ===== Player Account =====
struct PlayerAccount {
    QString uid;                // Unique ID (UUID v4, generated once, permanent)
    QString username;
    QString passwordHash;       // SHA-256(password + salt)
    QString salt;               // Random salt for password
    QString email;              // Optional
    QString avatarEmoji = "😎"; // Default avatar
    QString walletAddress;      // OmniBus blockchain address (optional)
    QString displayName;        // What others see

    // Stats
    int handsPlayed = 0;
    int handsWon = 0;
    double totalWagered = 0.0;
    double totalWon = 0.0;
    double totalProfit = 0.0;
    int tournamentWins = 0;
    double bestHandPot = 0.0;

    // Meta
    QDateTime createdAt;
    QDateTime lastLogin;
    bool isGuest = false;
    int level = 1;              // XP-based level

    double winRate() const {
        return handsPlayed > 0 ? (double)handsWon / handsPlayed * 100.0 : 0.0;
    }

    QJsonObject toJson() const;
    static PlayerAccount fromJson(const QJsonObject& obj);
};

// ===== Account System =====
class AccountSystem : public QObject {
    Q_OBJECT
public:
    explicit AccountSystem(QObject* parent = nullptr);

    // ── Registration & Login ──
    bool registerAccount(const QString& username, const QString& password,
                         const QString& email = "", const QString& avatar = "😎");
    bool login(const QString& username, const QString& password);
    void loginAsGuest(const QString& displayName = "");
    void logout();

    // ── Current user ──
    bool isLoggedIn() const { return m_loggedIn; }
    bool isGuest() const { return m_currentAccount.isGuest; }
    PlayerAccount currentAccount() const { return m_currentAccount; }
    QString uid() const { return m_currentAccount.uid; }
    QString username() const { return m_currentAccount.username; }
    QString displayName() const { return m_currentAccount.displayName; }
    QString avatarEmoji() const { return m_currentAccount.avatarEmoji; }

    // ── Profile ──
    void setAvatar(const QString& emoji);
    void setWalletAddress(const QString& addr);
    void setDisplayName(const QString& name);

    // ── Stats ──
    void recordHandPlayed();
    void recordHandWon(double potAmount);
    void recordWager(double amount);

    // ── Server-side: verify accounts ──
    bool verifyPassword(const QString& username, const QString& password) const;
    PlayerAccount* getAccount(const QString& uid);
    PlayerAccount* getAccountByUsername(const QString& username);
    QVector<PlayerAccount> allAccounts() const;

    // ── Persistence ──
    void save(const QString& path = "accounts.json");
    void load(const QString& path = "accounts.json");

    // ── Device UID ──
    // Generated once per device, stored in device_id.txt
    static QString getDeviceUID();

signals:
    void loginSuccess(const QString& uid, const QString& username);
    void loginFailed(const QString& reason);
    void registerSuccess(const QString& uid);
    void registerFailed(const QString& reason);
    void accountUpdated();
    void loggedOut();

private:
    static QString hashPassword(const QString& password, const QString& salt);
    static QString generateSalt();
    static QString generateUID();

    QMap<QString, PlayerAccount> m_accounts;  // uid -> account
    QMap<QString, QString> m_usernameToUid;   // username -> uid (index)
    PlayerAccount m_currentAccount;
    bool m_loggedIn = false;
};
