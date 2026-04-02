#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QComboBox>
#include "engine/AccountSystem.h"

class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(AccountSystem* accounts, QWidget* parent = nullptr);

signals:
    void loginSuccess(const QString& uid, const QString& username);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void showLogin();
    void showRegister();
    void doLogin();
    void doRegister();
    void doGuest();

    AccountSystem* m_accounts;
    QStackedWidget* m_formStack;  // 0=login, 1=register

    // Login form
    QLineEdit* m_loginUser;
    QLineEdit* m_loginPass;
    QLabel* m_loginError;

    // Register form
    QLineEdit* m_regUser;
    QLineEdit* m_regPass;
    QLineEdit* m_regPassConfirm;
    QLineEdit* m_regEmail;
    QComboBox* m_regAvatar;
    QLabel* m_regError;

    // Shared
    QPushButton* m_switchBtn;  // "Create Account" / "Back to Login"
};
