#include "PokerProtocol.h"

namespace PokerProtocol {

QJsonObject makeLogin(const QString& name, double balance)
{
    QJsonObject obj;
    obj["type"] = "login";
    obj["name"] = name;
    obj["balance"] = balance;
    return obj;
}

QJsonObject makeListTables()
{
    QJsonObject obj;
    obj["type"] = "list_tables";
    return obj;
}

QJsonObject makeCreateTable(const QString& name, double sb, double bb, double minBuy, double maxBuy, int seats)
{
    QJsonObject obj;
    obj["type"] = "create_table";
    obj["name"] = name;
    obj["small_blind"] = sb;
    obj["big_blind"] = bb;
    obj["min_buy_in"] = minBuy;
    obj["max_buy_in"] = maxBuy;
    obj["max_seats"] = seats;
    return obj;
}

QJsonObject makeJoinTable(const QString& tableId, double buyIn)
{
    QJsonObject obj;
    obj["type"] = "join_table";
    obj["table_id"] = tableId;
    obj["buy_in"] = buyIn;
    return obj;
}

QJsonObject makeLeaveTable()
{
    QJsonObject obj;
    obj["type"] = "leave_table";
    return obj;
}

QJsonObject makeAction(const QString& action, double amount)
{
    QJsonObject obj;
    obj["type"] = "action";
    obj["action"] = action;
    obj["amount"] = amount;
    return obj;
}

QJsonObject makeChat(const QString& message)
{
    QJsonObject obj;
    obj["type"] = "chat";
    obj["message"] = message;
    return obj;
}

QJsonObject makeAddFunds(double amount)
{
    QJsonObject obj;
    obj["type"] = "add_funds";
    obj["amount"] = amount;
    return obj;
}

QJsonObject makeChooseSeat(int seat)
{
    QJsonObject obj;
    obj["type"] = "choose_seat";
    obj["seat"] = seat;
    return obj;
}

QJsonObject makeClientSeed(const QString& seed)
{
    QJsonObject obj;
    obj["type"] = "client_seed";
    obj["seed"] = seed;
    return obj;
}

QJsonObject makeVerifyHand(int handNumber)
{
    QJsonObject obj;
    obj["type"] = "verify_hand";
    obj["hand_number"] = handNumber;
    return obj;
}

QString messageType(const QJsonObject& msg)
{
    return msg["type"].toString();
}

QByteArray toJson(const QJsonObject& obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QJsonObject fromJson(const QByteArray& data)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();
    return doc.object();
}

} // namespace PokerProtocol
