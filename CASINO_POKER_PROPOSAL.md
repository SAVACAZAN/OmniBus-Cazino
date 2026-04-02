# OmniBus Casino & Poker — Full Production Proposal

## Status per Categorie: CE AVEM ✅ | CE TREBUIE 🔄 | CE NU AVEM ❌

---

## 1. SECURITATE ȘI INTEGRITATE

| Modul | Status | Cine Folosește | La Ce Servește |
|-------|--------|----------------|----------------|
| **RNG Certificat** | ✅ Avem (OpenSSL CSPRNG + SHA-256 seeds) | Serverul de poker | Amestecarea cărților — dovedibil aleatoriu |
| **Provably Fair** | ✅ Avem (server seed + client seed + commitment) | Ambii jucători | Oricine poate verifica că nu s-a trișat |
| **Anti-Collusion** | ❌ Nu avem | Admin/Server | Detectează 2 jucători care cooperează (IP comun) |
| **Bot Detection** | ❌ Nu avem | Server | Analizează timing-ul acțiunilor — blochează boți |
| **SSL/TLS** | 🔄 Parțial (WebSocket non-secure acum) | Client ↔ Server | Criptează comunicarea |
| **DDoS Protection** | ❌ Nu avem (trebuie Cloudflare) | Infrastructure | Protecție împotriva atacurilor |

**Prioritate**: SSL/TLS (ușor de adăugat — QWebSocketServer suportă WSS nativ)

---

## 2. MANAGEMENTUL BANILOR

| Modul | Status | Cine Folosește | La Ce Servește |
|-------|--------|----------------|----------------|
| **Payment Gateway** | ❌ Nu avem | Jucătorul | Cumpără fise cu cardul (Stripe/PayPal) |
| **Crypto Wallet** | ✅ Avem (OmniBus Wallet, SuperVault) | Jucătorul | Depunere/retragere crypto |
| **Virtual Wallet** | ✅ Avem (BettingEngine + balances) | Serverul | Evidența fisei per jucător |
| **KYC Provider** | ❌ Nu avem | Legal/Compliance | Verificarea identității (Sumsub) |
| **AML Logic** | ❌ Nu avem | Legal/Compliance | Monitorizare retrageri suspecte |

**Prioritate**: Crypto Wallet DONE. Payment Gateway la Tier 4.

---

## 3. INFRASTRUCTURĂ & BACKEND

| Modul | Status | Cine Folosește | La Ce Servește |
|-------|--------|----------------|----------------|
| **Nakama (Meta Server)** | ❌ Planificat Tier 3 | Auth, Matchmaking, Social | Login, găsire mese, prieteni |
| **Colyseus (Game Server)** | ❌ Planificat Tier 3 | Logica de joc real-time | Dealing, betting, state sync |
| **Docker & Kubernetes** | ❌ Planificat Tier 4 | DevOps | Scalabilitate |
| **Redis** | ❌ Planificat Tier 3 | Server | Cache rapid (sesiuni, state) |
| **PostgreSQL** | ❌ Planificat Tier 3 | Server | Baza de date principală |
| **WebSocket Server** | ✅ Avem (PokerServer C++) | Client ↔ Server | Comunicare real-time |
| **P2P Discovery** | ✅ Avem (UDP broadcast + game codes) | Jucători | Găsire jocuri fără server central |
| **Monitoring (Prometheus)** | ❌ Tier 4+ | Admin | Monitorizare server load |
| **Logging (ELK)** | 🔄 Parțial (hand_history.json) | Admin/Dispute | Istoric mâini pentru verificare |

**Ce folosim ACUM**: WebSocket Server propriu + P2P Discovery
**Tier 3**: Adăugăm Nakama + Colyseus + PostgreSQL

---

## 4. EXPERIENȚA JUCĂTORULUI (UI/UX)

| Modul | Status | Cine Folosește | La Ce Servește |
|-------|--------|----------------|----------------|
| **QPainter Table** | ✅ Avem (masă, cărți, jucători) | Jucătorul | Vizualizare joc |
| **OMPEval** | ✅ TOCMAI INTEGRAT! | Serverul | Evaluare mâini profesională (2.5B/sec) |
| **Card Assets (PNG)** | 🔄 Parțial (descărcat deck) | UI | Cărți grafice realiste |
| **Sound Engine** | ❌ Nu avem | Jucătorul | Sunete de cărți, fise, alerte |
| **Chat System** | ✅ Avem (WebSocket chat) | Jucătorii | Comunicare la masă |
| **Replay System** | ✅ Avem (hand_history.json) | Jucătorul | Revede mâini jucate |
| **Timer (30s)** | ✅ Avem | Jucătorul | Countdown per acțiune |
| **Buy-In Screen** | ✅ Avem (slider + presets) | Jucătorul | Alege cu cât intri la masă |
| **Login/Register** | ✅ Avem (SHA-256 + salt) | Jucătorul | Cont persistent |
| **Animations** | 🔄 Basic (QPainter redraw) | Jucătorul | Card deal, chip move |

**Prioritate**: Sound Engine (Qt Multimedia — ușor de adăugat)

---

## 5. POKER ENGINE

| Modul | Status | Cine Folosește | La Ce Servește |
|-------|--------|----------------|----------------|
| **Hand Evaluator** | ✅ OMPEval (profesional!) | Server + Client | Cine câștigă (Royal Flush → High Card) |
| **Equity Calculator** | 🔄 Monte Carlo basic | Client | % șanse de câștig live |
| **Betting Logic** | ✅ Avem (fold/check/call/raise/allin) | Server | Reguli de pariere |
| **Blinds System** | ✅ Avem (SB/BB auto-post) | Server | Structura blind-urilor |
| **Side Pots** | ✅ Avem (calculateSidePots) | Server | All-in cu sume diferite |
| **Seed System** | ✅ Avem (commitment + reveal) | Server + Verificare | Provably fair |
| **Action Timer** | ✅ Avem (30s + auto-fold) | Server | Timeout per jucător |
| **Hand History** | ✅ Avem (JSON cu seed + acțiuni) | Admin/Jucător | Verificare + replay |
| **Tournament System** | ❌ Nu avem | Server | Sit & Go, MTT, blind schedule |
| **Sit & Go** | ❌ Nu avem | Matchmaking | Turnee rapide 6/9 jucători |
| **Multi-Table** | ❌ Nu avem | Jucător avansat | Joacă la mai multe mese |

---

## 6. ADMINISTRARE & MARKETING

| Modul | Status | Cine Folosește | La Ce Servește |
|-------|--------|----------------|----------------|
| **Admin Panel** | ❌ Nu avem | Admin | Ban jucători, creare turnee |
| **Affiliate System** | ❌ Nu avem | Marketing | Plătește referreri |
| **Email Marketing** | ❌ Nu avem | Marketing | Promoții, confirmări |
| **Analytics** | ❌ Nu avem | Business | Comportament jucători |
| **Customer Support** | ❌ Nu avem | Support | Tichete, dispute |
| **Reputation System** | ✅ Avem (fair play score) | P2P | Anti-cheat descentralizat |

---

## 7. LEGAL & CONFORMITATE

| Modul | Status | Cine Folosește | La Ce Servește |
|-------|--------|----------------|----------------|
| **Gambling License** | ❌ Nu avem | Legal | Operare legală (Curacao/Malta/ONJN) |
| **Terms of Service** | ❌ Nu avem | Legal | Contract cu jucătorul |
| **Privacy Policy (GDPR)** | ❌ Nu avem | Legal | Protecția datelor |
| **Responsible Gambling** | ❌ Nu avem | Legal/Jucător | Limite depunere, auto-excludere |
| **Company (SRL/SA)** | ❌ Nu avem | Legal | Entitate juridică |

**Notă**: Fără licență nu poți opera pe bani reali legal. Curacao e cel mai ieftin (~$25K).

---

## 8. DISTRIBUȚIE

| Modul | Status | Cine Folosește | La Ce Servește |
|-------|--------|----------------|----------------|
| **Installer** | 🔄 Avem ZIP + PLAY.bat | Jucătorul | Descarcă și instalează |
| **Auto-Updater** | ❌ Nu avem | Jucătorul | Update automat |
| **CDN** | ❌ Nu avem | Distribuție | Download rapid assets |
| **windeployqt** | ✅ Avem (dist/ folder) | Build | Pachetare DLL-uri |

---

## REZUMAT SCORCARD

| Categorie | Avem | Parțial | Lipsă | Score |
|-----------|------|---------|-------|-------|
| Securitate | 2 | 1 | 3 | 42% |
| Bani | 2 | 0 | 3 | 40% |
| Infrastructură | 3 | 1 | 6 | 35% |
| UI/UX | 8 | 2 | 1 | 82% |
| Poker Engine | 8 | 1 | 3 | 71% |
| Admin | 1 | 0 | 5 | 17% |
| Legal | 0 | 0 | 5 | 0% |
| Distribuție | 2 | 0 | 2 | 50% |
| **TOTAL** | **26** | **5** | **28** | **~47%** |

**Suntem la ~47% din producție completă. UI/UX și Poker Engine sunt cele mai avansate.**

---

## ROADMAP — Pași Concreți

### ACUM (Tier 2) — 1 săptămână
- [x] OMPEval integrat
- [x] Buy-in screen
- [x] Login/Register
- [x] Provably fair seeds
- [ ] Fix multiplayer connection flow
- [ ] Sound effects (Qt Multimedia)
- [ ] Card PNG assets proper

### LUNA 1 (Tier 3) — Nakama + Colyseus
- [ ] Deploy Nakama pe VPS (auth + matchmaking)
- [ ] Deploy Colyseus pe VPS (game logic)
- [ ] PostgreSQL pentru accounts
- [ ] SSL/TLS (WSS)
- [ ] Tournament system (Sit & Go)

### LUNA 2-3 (Tier 4) — Production
- [ ] Admin panel (web)
- [ ] Payment gateway (Stripe)
- [ ] KYC integration
- [ ] Anti-cheat basic
- [ ] Docker deployment
- [ ] Gambling license application

### LUNA 4-6 (Tier 5) — Launch
- [ ] Beta testing cu jucători reali
- [ ] Marketing + affiliate system
- [ ] Mobile version (Qt for Android/iOS)
- [ ] Blockchain integration (on-chain bets)
