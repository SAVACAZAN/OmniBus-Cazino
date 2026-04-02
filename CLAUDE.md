# CLAUDE.md — OmniBus Casino

## Project Overview
Professional C++17/Qt6 **crypto casino & betting platform** for the OmniBus ecosystem. 10 games, provably fair, multi-currency. Works **standalone**, as **module** in OmniBus full terminal, and imports **OmniBus Wallet** (SuperVault).

## Architecture: Standalone + Modular + Wallet
```
OmniBus Casino
├── Standalone app (OmniBusCazino.exe)
├── Embeddable as QWidget in OmniBus Trading Terminal
├── Imports OmniBus Wallet (SuperVault Named Pipe / DPAPI)
├── Uses exchange prices for Binary Options (real LCX/Kraken/Coinbase)
└── Provably fair via SHA-256 hash chains
```

## 10 Games
1. 🃏 Texas Hold'em Poker — multiplayer, blinds, AI opponents
2. 🎲 Dice — provably fair, over/under, configurable house edge
3. 🃏 Blackjack — classic 21, hit/stand/double/split
4. 🎰 Slots — 5-reel crypto symbols, wilds, free spins, jackpot
5. ⚽ Sports Betting — football/tennis/basketball, live odds, parlays
6. 🏇 Race Betting — virtual races, animated, win/place/each-way
7. 🎯 Crash — multiplier crash, cash out, social view
8. 🪙 Coin Flip — 50/50, double or nothing, streak bonuses
9. 📊 Binary Options — real price UP/DOWN, 30s-15m timeframes
10. 🏆 Auction — live bidding, countdown, NFT/items

## Wallet Integration
- Reads balance from OmniBus SuperVault (Named Pipe on Windows)
- Falls back to env var OMNIBUS_MNEMONIC
- Supports: EUR, BTC, ETH, LCX, USDT, SOL
- Deposit/withdraw via exchange APIs (same as Trading Terminal)

## Build
```bash
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build
```

## Ecosystem
Part of OmniBus at `C:\Kits work\limaje de programare\`. Siblings:
- **OmniBus QT Tradin GUI terminal** — Trading (can embed Casino)
- **OmniBus QT ChartsMaths** — Charts (Binary Options uses chart data)
- **OmnibusSidebar** — Desktop wallet (SuperVault source)
- **OmniBus-BlockChainCore** — Blockchain (on-chain bets future)
