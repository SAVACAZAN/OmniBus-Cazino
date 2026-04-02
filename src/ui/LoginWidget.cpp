#include "LoginWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QGraphicsDropShadowEffect>

static const QString GOLD   = "#facc15";
static const QString GREEN  = "#10eb04";
static const QString RED    = "#eb0404";
static const QString BG     = "#0a0c12";

LoginWidget::LoginWidget(AccountSystem* accounts, QWidget* parent)
    : QWidget(parent)
    , m_accounts(accounts)
{
    setupUI();

    connect(m_accounts, &AccountSystem::loginSuccess, this, [this](const QString& uid, const QString& username) {
        emit loginSuccess(uid, username);
    });
    connect(m_accounts, &AccountSystem::loginFailed, this, [this](const QString& reason) {
        m_loginError->setText(reason);
        m_loginError->setVisible(true);
    });
    connect(m_accounts, &AccountSystem::registerFailed, this, [this](const QString& reason) {
        m_regError->setText(reason);
        m_regError->setVisible(true);
    });
}

void LoginWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dark background with subtle radial glow
    p.fillRect(rect(), QColor(BG));

    // Gold glow in center
    QRadialGradient glow(width()/2, height()/2, 400);
    glow.setColorAt(0, QColor(250, 204, 21, 15));
    glow.setColorAt(0.5, QColor(250, 204, 21, 5));
    glow.setColorAt(1, Qt::transparent);
    p.fillRect(rect(), glow);

    // Subtle grid pattern
    p.setPen(QPen(QColor(250, 204, 21, 8), 1));
    for (int x = 0; x < width(); x += 60)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += 60)
        p.drawLine(0, y, width(), y);
}

void LoginWidget::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    // ── Logo ──
    auto* logoLabel = new QLabel("OMNIBUS CASINO");
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet(
        "font-size: 36px; font-weight: 900; color: " + GOLD + "; "
        "letter-spacing: 4px; background: transparent; padding: 10px;");
    mainLayout->addWidget(logoLabel);

    auto* subtitleLabel = new QLabel("Decentralized • Provably Fair • P2P");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet("font-size: 12px; color: rgba(250,204,21,0.5); background: transparent; letter-spacing: 2px;");
    mainLayout->addWidget(subtitleLabel);

    mainLayout->addSpacing(30);

    // ── Form Container (card) ──
    auto* formCard = new QWidget;
    formCard->setFixedSize(420, 500);
    formCard->setStyleSheet(
        "background: rgba(20,25,35,0.9); border: 2px solid rgba(250,204,21,0.2); border-radius: 16px;");

    // Drop shadow
    auto* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(40);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 8);
    formCard->setGraphicsEffect(shadow);

    auto* cardLayout = new QVBoxLayout(formCard);
    cardLayout->setContentsMargins(30, 25, 30, 25);
    cardLayout->setSpacing(12);

    // Stacked widget for Login / Register
    m_formStack = new QStackedWidget;

    QString inputStyle =
        "QLineEdit { background: rgba(0,0,0,0.4); color: " + GOLD + "; "
        "border: 1px solid rgba(250,204,21,0.25); border-radius: 8px; "
        "padding: 14px 16px; font-size: 14px; font-weight: 600; min-height: 20px; }"
        "QLineEdit:focus { border-color: " + GOLD + "; }";

    QString labelStyle =
        "color: rgba(250,204,21,0.5); font-size: 10px; font-weight: 700; "
        "text-transform: uppercase; letter-spacing: 1px; background: transparent;";

    // ═══════════ LOGIN FORM ═══════════
    auto* loginPage = new QWidget;
    auto* loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setSpacing(10);

    auto* loginTitle = new QLabel("SIGN IN");
    loginTitle->setStyleSheet("font-size: 18px; font-weight: 900; color: " + GOLD + "; background: transparent;");
    loginTitle->setAlignment(Qt::AlignCenter);
    loginLayout->addWidget(loginTitle);

    loginLayout->addSpacing(8);

    auto* userLabel = new QLabel("USERNAME");
    userLabel->setStyleSheet(labelStyle);
    loginLayout->addWidget(userLabel);

    m_loginUser = new QLineEdit;
    m_loginUser->setPlaceholderText("Enter username");
    m_loginUser->setStyleSheet(inputStyle);
    loginLayout->addWidget(m_loginUser);

    auto* passLabel = new QLabel("PASSWORD");
    passLabel->setStyleSheet(labelStyle);
    loginLayout->addWidget(passLabel);

    m_loginPass = new QLineEdit;
    m_loginPass->setPlaceholderText("Enter password");
    m_loginPass->setEchoMode(QLineEdit::Password);
    m_loginPass->setStyleSheet(inputStyle);
    loginLayout->addWidget(m_loginPass);

    m_loginError = new QLabel;
    m_loginError->setStyleSheet("color: " + RED + "; font-size: 11px; background: transparent;");
    m_loginError->setVisible(false);
    loginLayout->addWidget(m_loginError);

    loginLayout->addSpacing(6);

    auto* loginBtn = new QPushButton("SIGN IN");
    loginBtn->setFixedHeight(42);
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 " + GOLD + ", stop:1 #f59e0b); "
        "color: #0a0c12; border: none; border-radius: 6px; font-size: 14px; font-weight: 900; letter-spacing: 1px; }"
        "QPushButton:hover { background: " + GOLD + "; }");
    connect(loginBtn, &QPushButton::clicked, this, &LoginWidget::doLogin);
    connect(m_loginPass, &QLineEdit::returnPressed, this, &LoginWidget::doLogin);
    loginLayout->addWidget(loginBtn);

    loginLayout->addStretch();
    m_formStack->addWidget(loginPage); // index 0

    // ═══════════ REGISTER FORM ═══════════
    auto* regPage = new QWidget;
    auto* regLayout = new QVBoxLayout(regPage);
    regLayout->setSpacing(8);

    auto* regTitle = new QLabel("CREATE ACCOUNT");
    regTitle->setStyleSheet("font-size: 18px; font-weight: 900; color: " + GOLD + "; background: transparent;");
    regTitle->setAlignment(Qt::AlignCenter);
    regLayout->addWidget(regTitle);

    regLayout->addSpacing(4);

    auto* rUserLabel = new QLabel("USERNAME");
    rUserLabel->setStyleSheet(labelStyle);
    regLayout->addWidget(rUserLabel);

    m_regUser = new QLineEdit;
    m_regUser->setPlaceholderText("Choose username (3+ chars)");
    m_regUser->setStyleSheet(inputStyle);
    regLayout->addWidget(m_regUser);

    auto* rPassLabel = new QLabel("PASSWORD");
    rPassLabel->setStyleSheet(labelStyle);
    regLayout->addWidget(rPassLabel);

    m_regPass = new QLineEdit;
    m_regPass->setPlaceholderText("Choose password (4+ chars)");
    m_regPass->setEchoMode(QLineEdit::Password);
    m_regPass->setStyleSheet(inputStyle);
    regLayout->addWidget(m_regPass);

    auto* rPass2Label = new QLabel("CONFIRM PASSWORD");
    rPass2Label->setStyleSheet(labelStyle);
    regLayout->addWidget(rPass2Label);

    m_regPassConfirm = new QLineEdit;
    m_regPassConfirm->setPlaceholderText("Confirm password");
    m_regPassConfirm->setEchoMode(QLineEdit::Password);
    m_regPassConfirm->setStyleSheet(inputStyle);
    regLayout->addWidget(m_regPassConfirm);

    // Avatar selector
    auto* avatarRow = new QHBoxLayout;
    auto* avatarLabel = new QLabel("AVATAR");
    avatarLabel->setStyleSheet(labelStyle);
    avatarRow->addWidget(avatarLabel);
    m_regAvatar = new QComboBox;
    m_regAvatar->addItems({"😎", "🤠", "🎩", "👨‍💼", "👩", "🧔", "🤖", "🎭", "👑", "🦊", "🐺", "🦁"});
    m_regAvatar->setStyleSheet(
        "QComboBox { background: rgba(0,0,0,0.4); color: " + GOLD + "; "
        "border: 1px solid rgba(250,204,21,0.25); border-radius: 6px; "
        "padding: 6px 10px; font-size: 16px; }"
        "QComboBox QAbstractItemView { background: rgba(20,25,35,0.95); color: white; font-size: 16px; }");
    avatarRow->addWidget(m_regAvatar, 1);
    regLayout->addLayout(avatarRow);

    m_regError = new QLabel;
    m_regError->setStyleSheet("color: " + RED + "; font-size: 11px; background: transparent;");
    m_regError->setVisible(false);
    regLayout->addWidget(m_regError);

    auto* regBtn = new QPushButton("CREATE ACCOUNT");
    regBtn->setFixedHeight(42);
    regBtn->setCursor(Qt::PointingHandCursor);
    regBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 " + GREEN + ", stop:1 #0cc800); "
        "color: #0a0c12; border: none; border-radius: 6px; font-size: 14px; font-weight: 900; letter-spacing: 1px; }"
        "QPushButton:hover { background: " + GREEN + "; }");
    connect(regBtn, &QPushButton::clicked, this, &LoginWidget::doRegister);
    regLayout->addWidget(regBtn);

    regLayout->addStretch();
    m_formStack->addWidget(regPage); // index 1

    cardLayout->addWidget(m_formStack);

    // ── Switch link ──
    m_switchBtn = new QPushButton("Don't have an account? Create one");
    m_switchBtn->setCursor(Qt::PointingHandCursor);
    m_switchBtn->setStyleSheet(
        "QPushButton { background: transparent; color: rgba(250,204,21,0.6); "
        "border: none; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { color: " + GOLD + "; }");
    connect(m_switchBtn, &QPushButton::clicked, this, [this]() {
        if (m_formStack->currentIndex() == 0) showRegister();
        else showLogin();
    });
    cardLayout->addWidget(m_switchBtn, 0, Qt::AlignCenter);

    // ── Guest button ──
    auto* guestBtn = new QPushButton("Play as Guest");
    guestBtn->setCursor(Qt::PointingHandCursor);
    guestBtn->setStyleSheet(
        "QPushButton { background: transparent; color: rgba(255,255,255,0.3); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; "
        "padding: 6px 16px; font-size: 10px; font-weight: 600; }"
        "QPushButton:hover { color: rgba(255,255,255,0.6); border-color: rgba(255,255,255,0.2); }");
    connect(guestBtn, &QPushButton::clicked, this, &LoginWidget::doGuest);
    cardLayout->addWidget(guestBtn, 0, Qt::AlignCenter);

    mainLayout->addWidget(formCard, 0, Qt::AlignCenter);

    mainLayout->addSpacing(20);

    // ── Footer ──
    auto* footer = new QLabel("OmniBus Casino v1.0 • Provably Fair • SHA-256 • Decentralized P2P");
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet("font-size: 9px; color: rgba(255,255,255,0.15); background: transparent;");
    mainLayout->addWidget(footer);
}

void LoginWidget::showLogin() {
    m_formStack->setCurrentIndex(0);
    m_switchBtn->setText("Don't have an account? Create one");
    m_loginError->setVisible(false);
}

void LoginWidget::showRegister() {
    m_formStack->setCurrentIndex(1);
    m_switchBtn->setText("Already have an account? Sign in");
    m_regError->setVisible(false);
}

void LoginWidget::doLogin() {
    m_loginError->setVisible(false);
    QString user = m_loginUser->text().trimmed();
    QString pass = m_loginPass->text();

    if (user.isEmpty() || pass.isEmpty()) {
        m_loginError->setText("Please fill in all fields");
        m_loginError->setVisible(true);
        return;
    }

    m_accounts->login(user, pass);
}

void LoginWidget::doRegister() {
    m_regError->setVisible(false);
    QString user = m_regUser->text().trimmed();
    QString pass = m_regPass->text();
    QString pass2 = m_regPassConfirm->text();
    QString avatar = m_regAvatar->currentText();

    if (user.isEmpty() || pass.isEmpty()) {
        m_regError->setText("Please fill in all fields");
        m_regError->setVisible(true);
        return;
    }
    if (pass != pass2) {
        m_regError->setText("Passwords don't match");
        m_regError->setVisible(true);
        return;
    }

    m_accounts->registerAccount(user, pass, "", avatar);
}

void LoginWidget::doGuest() {
    m_accounts->loginAsGuest();
}
