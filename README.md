# 🎰 OmniBus Casino

**Decentralized crypto casino & poker platform** built with C++17 / Qt6 / OpenSSL.

10 games • Multiplayer poker • Tournaments • Provably fair • P2P

![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt](https://img.shields.io/badge/Qt-6.8-green)
![License](https://img.shields.io/badge/License-Proprietary-red)
![Games](https://img.shields.io/badge/Games-10-gold)

---

## Screenshots

| Login | Lobby | Poker Room |
|-------|-------|------------|
| Login/Register screen | 10 game cards | QPainter rendered table |

---

## Features

### 10 Games
| # | Game | Type | Description |
|---|------|------|-------------|
| 🎲 | **Dice** | Casino | Over/under target, provably fair, 1% house edge |
| 🎯 | **Crash** | Casino | Multiplier rises — cash out before crash! |
| 🪙 | **Coin Flip** | Casino | 50/50, double or nothing, streak bonuses |
| 🃏 | **Blackjack** | Cards | Classic 21, hit/stand/double, dealer AI |
| 🃏 | **Poker** | Cards | Texas Hold'em vs AI opponents |
| 🎰 | **Poker Room** | Cards | Visual QPainter table, 9 seats, buy-in |
| 🎰 | **Slots** | Casino | 5-reel crypto symbols, wilds, free spins |
| ⚽ | **Sports Betting** | Betting | Football matches, live odds, parlays |
| 📊 | **Binary Options** | Trading | UP/DOWN on price, 30s-15m, 85% payout |
| 🏇 | **Race Betting** | Betting | Virtual races, animated, win/place bets |
| 🏆 | **Auction** | Social | Live bidding, countdown, items |

### Multiplayer Poker
- **HOST/JOIN** with game codes (no IP needed)
- **WebSocket** real-time communication
- **LAN Discovery** — find games on local network automatically
- **P2P** — one player hosts, others join
- **SSL/TLS** — encrypted connections (WSS)

### Tournament System
- **Sit & Go** — 6 or 9 players, starts when full
- **MTT (Multi-Table)** — 18-45 players, blind schedule
- **Blind levels** — 10 levels from 10/20 to 500/1000
- **Prize structure** — SNG: 50/30/20, MTT: 30/20/15/10/...
- **Late registration** + **Rebuys** (MTT)
- **Table balancing** + **Final table** merge

### Provably Fair
- SHA-256 server seed commitment before each hand
- Client seeds from all players
- Combined seed = SHA256(server + client1 + client2 + ...)
- Server seed revealed after hand — anyone can verify
- Full hand history saved with seeds + action log

### Professional Poker Engine
- **OMPEval** — 2.5 billion hand evaluations/second
- **Hand ranking** — Royal Flush through High Card
- **Side pots** — correct all-in calculations
- **Timer** — 30 seconds per action, auto-fold
- **Provably fair deck shuffle** — deterministic from combined seed

### Account System
- Register with username + password (SHA-256 + salt)
- 64-char unique UID per player
- Guest mode (play without account)
- Stats tracking (hands played, won, profit)
- Reputation system (fair play score)

---

## Prerequisites

| Software | Version | Download |
|----------|---------|----------|
| **Qt** | 6.8.3+ (MSVC 2022) | https://www.qt.io/download |
| **Visual Studio Build Tools** | 2022 | https://visualstudio.microsoft.com/downloads/ |
| **OpenSSL** | 3.x (Win64) | https://slproweb.com/products/Win32OpenSSL.html |
| **CMake** | 3.20+ | https://cmake.org/download/ |
| **Git** | Any | https://git-scm.com/ |

---

## Installation

### 1. Clone the repository

```bash
git clone --recursive https://github.com/SAVACAZAN/OmniBus-Cazino.git
cd OmniBus-Cazino
```

> `--recursive` is needed to download the OMPEval submodule.

### 2. Configure paths

Edit `CMakeLists.txt` if your Qt/OpenSSL paths differ:

```cmake
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")   # Your Qt path
set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")  # Your OpenSSL path
```

### 3. Build

**Option A — Command line (recommended):**

Create a `build.bat`:

```batch
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64" -G "NMake Makefiles"
cmake --build build
```

Run:
```
build.bat
```

**Option B — Qt Creator:**

1. Open `CMakeLists.txt` in Qt Creator
2. Select MSVC 2022 64-bit kit
3. Click Build (Ctrl+B)

### 4. Run

```bash
# Add Qt DLLs to PATH
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%

# Run
build\OmniBusCazino.exe
```

---

## Quick Start

### Solo Play (vs AI)
1. Launch `OmniBusCazino.exe`
2. **Register** or **Play as Guest**
3. Click any game in sidebar
4. For poker: go to **Room** → choose buy-in → play

### Multiplayer Poker (2+ players)
1. Launch 2 instances of `OmniBusCazino.exe`
2. Both: login with different accounts
3. Both: go to **MP Room** in sidebar

**Instance 1 (Host):**
4. Click **HOST GAME**
5. Share the game code with friends

**Instance 2 (Join):**
4. Enter the game code
5. Click **JOIN GAME**

6. Both: click an empty seat to sit down
7. Hand starts automatically when 2+ players seated!

### Multiplayer over Internet
1. Host: open port **9999 TCP** on your router
2. Host: share your **public IP** (google "what is my ip")
3. Join: enter `ws://HOST_PUBLIC_IP:9999` or use the game code

### Tournaments
1. Go to **Tournaments** in sidebar
2. Click **REGISTER** on any tournament
3. Tournament starts when enough players register
4. Blinds increase automatically, last one standing wins!

---

## SSL/TLS (Secure Connection)

Generate a self-signed certificate:

```bash
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes -subj "/CN=OmniBus Casino"
```

Copy `cert.pem` and `key.pem` to the `build/` directory. Server will automatically use **WSS** (secure WebSocket).

For production, use [Let's Encrypt](https://letsencrypt.org/) for a free trusted certificate.

---

## Distribution (Send to Friends)

Package the app with all needed DLLs:

```bash
mkdir dist
copy build\OmniBusCazino.exe dist\
cd dist
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt6.exe OmniBusCazino.exe
copy "C:\Program Files\OpenSSL-Win64\bin\libssl-3-x64.dll" .
copy "C:\Program Files\OpenSSL-Win64\bin\libcrypto-3-x64.dll" .
```

Zip the `dist/` folder and send it. No installation needed — just extract and run!

---

## Project Structure

```
OmniBus-Cazino/
├── src/
│   ├── app/              # MainWindow, LoginWidget
│   ├── engine/           # BettingEngine, ProvablyFair, AccountSystem, PokerEvaluator
│   ├── games/            # 10 game implementations + PokerRoom
│   ├── server/           # PokerServer, PokerTable, Tournament
│   ├── network/          # PokerClient (WebSocket)
│   ├── p2p/              # GameCode, P2PDiscovery, MentalPoker, ReputationSystem
│   ├── ui/               # BetPanel, LobbyWidget, TournamentLobbyWidget, P2PLobbyWidget
│   ├── utils/            # CardDeck, SoundEngine, Config
│   └── models/           # Bet, Player, GameResult
├── libs/
│   └── OMPEval/          # Professional hand evaluator (submodule)
├── assets/
│   └── cards/            # 52 card PNGs + back
├── mockup/               # HTML mockups for design reference
├── resources/            # QSS theme, QRC
├── ARCHITECTURE.md       # Technical architecture (Nakama + Colyseus + OMPEval)
├── CASINO_POKER_PROPOSAL.md  # Full production proposal (59 modules)
├── CLAUDE.md             # AI assistant instructions
└── CMakeLists.txt        # Build configuration
```

---

## Architecture

```
                    OmniBus Casino Qt Client
                    ┌───────────────────┐
                    │  Login → Lobby →  │
                    │  Games → Room →   │
                    │  Tournaments      │
                    └────────┬──────────┘
                             │
                    WebSocket (WS/WSS)
                             │
                    ┌────────┴──────────┐
                    │   Poker Server    │
                    │   (port 9999)     │
                    │                   │
                    │  • PokerTable     │
                    │  • Tournament     │
                    │  • OMPEval        │
                    │  • ProvablyFair   │
                    └───────────────────┘
```

**Future (Tier 3+):** Nakama (auth/matchmaking) + Colyseus (game rooms) + PostgreSQL + Blockchain

---

## Provably Fair — How It Works

```
1. Server generates random server_seed (hidden)
2. Server sends SHA256(server_seed) to all players (commitment)
3. Each player sends their own client_seed
4. combined_seed = SHA256(server_seed + client_seed_1 + client_seed_2 + ...)
5. Deck is shuffled deterministically using combined_seed
6. Game plays out normally
7. After hand: server reveals server_seed
8. Anyone can verify: SHA256(revealed_seed) == commitment from step 2
9. Anyone can reproduce the exact deck shuffle from combined_seed
```

**Nobody can cheat. Everything is verifiable.**

---

## Part of OmniBus Ecosystem

| App | Description |
|-----|-------------|
| **OmniBus Casino** | This project — 10 games + multiplayer poker |
| **OmniBus Trading Terminal** | Multi-exchange trading (LCX, Kraken, Coinbase) |
| **OmniBus ChartsMaths** | TradingView-like charting with technical indicators |
| **OmniBus Blockchain** | Custom blockchain with secp256k1 + BIP-32 wallet |
| **OmniBus OS** | Bare-metal sub-microsecond trading OS |

---

## License

Proprietary — OmniBus Ecosystem. All rights reserved.

---

**Built with C++ / Qt6 / OpenSSL / OMPEval**
**Provably Fair • Decentralized • P2P**
