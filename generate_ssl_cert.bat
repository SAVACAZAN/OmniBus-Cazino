@echo off
echo === Generating self-signed SSL certificate for OmniBus Casino ===
echo.

REM Uses OpenSSL to generate cert.pem and key.pem
"C:\Program Files\OpenSSL-Win64\bin\openssl.exe" req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes -subj "/CN=OmniBus Casino/O=OmniBus/C=RO"

if exist cert.pem (
    echo.
    echo === SUCCESS ===
    echo Certificate: cert.pem
    echo Private Key: key.pem
    echo Valid for: 365 days
    echo.
    echo Copy cert.pem and key.pem to the build/ directory.
    echo Server will automatically use WSS (secure) mode.
    copy cert.pem build\cert.pem >nul 2>&1
    copy key.pem build\key.pem >nul 2>&1
    echo Files copied to build/ directory.
) else (
    echo FAILED - Make sure OpenSSL is installed
)
pause
