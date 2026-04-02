# OmniBus Casino — Architecture Pro (Nakama + Colyseus + OMPEval)

## Flow-ul Complet

```
┌─────────────────────────────────────────────────────────────────┐
│                    OmniBus Casino Qt Client                     │
│                                                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│  │  Login   │→ │  Lobby   │→ │ PokerRoom│→ │ Cashier  │      │
│  │  Screen  │  │Matchmaker│  │ (Game UI)│  │  (Cash   │      │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  │   Out)   │      │
│       │              │              │        └──────────┘      │
└───────┼──────────────┼──────────────┼─────────────────────────┘
        │              │              │
    NAKAMA         NAKAMA         COLYSEUS
    (Auth)       (Matchmaking)   (Game Logic)
        │              │              │
┌───────┴──────────────┴──────────────┴─────────────────────────┐
│                                                                 │
│  ┌─────────────────────────┐  ┌──────────────────────────┐    │
│  │   NAKAMA SERVER         │  │  COLYSEUS GAME SERVER    │    │
│  │   (Meta Server)         │  │  (Real-time Game)        │    │
│  │                         │  │                          │    │
│  │  • Auth (login/register)│  │  • Card dealing          │    │
│  │  • Player profiles      │  │  • Betting rounds        │    │
│  │  • Wallet/balance       │  │  • OMPEval hand eval     │    │
│  │  • Friends list         │  │  • Pot calculation       │    │
│  │  • Leaderboards         │  │  • Side pots             │    │
│  │  • Matchmaking          │  │  • Timer (30s)           │    │
│  │  • Chat (global)        │  │  • Provably fair seeds   │    │
│  │  • Tournaments          │  │  • State sync            │    │
│  │  • Achievements         │  │  • Reconnection          │    │
│  │                         │  │                          │    │
│  │  PostgreSQL + Redis     │  │  In-memory state         │    │
│  └────────────┬────────────┘  └──────────┬───────────────┘    │
│               │                           │                    │
│               │   Server-to-Server API    │                    │
│               │←─────────────────────────→│                    │
│               │  (validate transactions)  │                    │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  OMPEval (C++ Library — linked into Colyseus/Qt)        │  │
│  │  • Hand evaluation (5/7 cards → ranking)                 │  │
│  │  • Equity calculator (Monte Carlo simulation)            │  │
│  │  • 2.5 billion evaluations/second                        │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  OmniBus Blockchain (optional, Tier 4+)                  │  │
│  │  • On-chain seed commitment                              │  │
│  │  • Smart contract for pot escrow                         │  │
│  │  • NFT achievements/rewards                              │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Deployment

```
VPS 1 (Nakama):     nakama server + PostgreSQL + Redis
VPS 2 (Colyseus):   colyseus game rooms (Node.js)  
VPS 3 (Blockchain): OmniBus node + RPC
Client:             OmniBusCazino.exe (Qt desktop)
```

## Tier Progression

| Tier | What We Add | Status |
|------|------------|--------|
| 1 | Custom evaluator + WebSocket + local JSON | ✅ DONE |
| 2 | OMPEval + better server + SQLite | 🔄 NEXT |
| 3 | Nakama + Colyseus + PostgreSQL | Planned |
| 4 | Kubernetes + Anti-cheat + Licensing | Future |
