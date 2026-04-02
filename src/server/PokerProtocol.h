#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>

namespace PokerProtocol {

// === CLIENT -> SERVER messages ===
// {"type": "login", "name": "Player1", "balance": 5000}
// {"type": "list_tables"}
// {"type": "create_table", "name": "High Rollers", "small_blind": 10, "big_blind": 20, "min_buy_in": 200, "max_buy_in": 2000, "max_seats": 9}
// {"type": "join_table", "table_id": "t1", "buy_in": 1000}
// {"type": "leave_table"}
// {"type": "action", "action": "fold|check|call|raise|allin", "amount": 120}
// {"type": "chat", "message": "nice hand!"}
// {"type": "add_funds", "amount": 5000}
// {"type": "choose_seat", "seat": 3}
// {"type": "client_seed", "seed": "random_string_from_player"}
// {"type": "verify_hand", "hand_number": 247}

// === SERVER -> CLIENT messages ===
// {"type": "login_ok", "player_id": "p1", "balance": 5000}
// {"type": "table_list", "tables": [{id, name, blinds, players, max_seats, avg_pot}]}
// {"type": "table_joined", "table_id": "t1", "seat": 3, "table_state": {...}}
// {"type": "table_state", "phase": "flop", "community": ["Ah","Kd","7c"], "pot": 340, ...}
// {"type": "deal", "your_cards": ["As","Ad"]}
// {"type": "your_turn", "to_call": 40, "min_raise": 80, "pot": 340, "seconds_left": 30}
// {"type": "action_result", "seat": 2, "action": "raise", "amount": 120}
// {"type": "showdown", "winners": [{seat, hand_name, cards, pot_won}], "all_hands": [...]}
// {"type": "chat_msg", "from": "Player1", "message": "nice hand!"}
// {"type": "error", "message": "Not enough chips"}
// {"type": "player_joined", "seat": 5, "name": "NewPlayer", "stack": 1000}
// {"type": "player_left", "seat": 5, "name": "OldPlayer"}
// {"type": "seat_chosen", "seat": 3, "player": "CryptoKing"}
// {"type": "seat_rejected", "seat": 3, "reason": "Seat taken"}
// {"type": "hand_start", "hand_number": 247, "server_seed_hash": "abc123...", "dealer_seat": 4}
// {"type": "seed_request"}
// {"type": "timer_update", "seat": 2, "seconds_left": 15}
// {"type": "hand_complete", "hand_number": 247, "server_seed": "def456...", "metadata": {...}}
// {"type": "hand_history", "hand_number": 247, "actions": [...], "result": {...}}

// Client -> Server builders
QJsonObject makeLogin(const QString& name, double balance);
QJsonObject makeListTables();
QJsonObject makeCreateTable(const QString& name, double sb, double bb, double minBuy, double maxBuy, int seats);
QJsonObject makeJoinTable(const QString& tableId, double buyIn);
QJsonObject makeLeaveTable();
QJsonObject makeAction(const QString& action, double amount = 0);
QJsonObject makeChat(const QString& message);
QJsonObject makeAddFunds(double amount);
QJsonObject makeChooseSeat(int seat);
QJsonObject makeClientSeed(const QString& seed);
QJsonObject makeVerifyHand(int handNumber);

// Parse incoming message type
QString messageType(const QJsonObject& msg);

// Serialize/deserialize
QByteArray toJson(const QJsonObject& obj);
QJsonObject fromJson(const QByteArray& data);

} // namespace PokerProtocol
