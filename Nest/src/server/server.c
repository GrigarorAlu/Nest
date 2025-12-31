// server.c

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../ext/argon2.h"
#include "../ext/enet/enet.h"
#include "../ext/sqlite3.h"
#include "../protocol.h"

static unsigned int rnd[4];
static int shouldClose = 0;

#define SALT_LENGTH (4 * 4)

void interrupt(int)
{
    printf("Interrupt: stopping.\n");
    shouldClose = 1;
}

static inline unsigned int rol32(unsigned int x, int k)
{
    return (x << k) | (x >> (64 - k));
}

static inline unsigned int xoshiro128pp()
{
    const unsigned int x = rnd[0] + rnd[3];
    const unsigned int result = rol32(rnd[0] + rnd[3], 23) + rnd[0];
    const unsigned int t = rnd[1] << 17;
    rnd[2] ^= rnd[0];
    rnd[3] ^= rnd[1];
    rnd[1] ^= rnd[2];
    rnd[0] ^= rnd[3];
    rnd[2] ^= t;
    rnd[3] = rol32(rnd[3], 45);
    return result;
}

static const char *nextSalt()
{
    static unsigned int salt[SALT_LENGTH / 4];
    unsigned char *str = (unsigned char *)salt;
    
    for (int i = 0; i < SALT_LENGTH / 4; i++)
        salt[i] = xoshiro128pp();
    for (int i = 0; i < SALT_LENGTH; i++)
        str[i] = str[i] % ('~' - '!') + '!';
    return str;
}

void broadcastUser(sqlite3 *db, int userId, char *data, size_t dataLen)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT (peer) FROM connections WHERE user = ?;", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, userId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        enet_peer_send((ENetPeer *)sqlite3_column_int64(stmt, 0), 0,
            enet_packet_create(data, dataLen, ENET_PACKET_FLAG_RELIABLE));
    sqlite3_finalize(stmt);
}

void broadcastChat(sqlite3 *db, int chatId, char *data, size_t dataLen)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
        "SELECT c.peer FROM memberships m JOIN connections c ON c.user = m.user WHERE m.chat = ?;", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, chatId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        enet_peer_send((ENetPeer *)sqlite3_column_int64(stmt, 0), 0,
            enet_packet_create(data, dataLen, ENET_PACKET_FLAG_RELIABLE));
    sqlite3_finalize(stmt);
}

size_t getChatData(sqlite3 *db, int chatId, int userId, char **msg)
{
    int lastMessageId = 0;
    char *lastMessageContent = 0;
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, content FROM messages "
        "WHERE chat = ? AND type = ? ORDER BY id DESC LIMIT 1;", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, chatId);
    sqlite3_bind_int(stmt, 2, MESSAGE_TEXT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        lastMessageId = sqlite3_column_int(stmt, 0);
        lastMessageContent = strdup(sqlite3_column_text(stmt, 1));
    }
    
    sqlite3_finalize(stmt);
    sqlite3_prepare_v2(db, "SELECT c.name, c.owner, c.created, (SELECT COUNT(*) FROM memberships WHERE chat = c.id) "
        "AS member_count, (SELECT joined FROM memberships WHERE chat = c.id AND user = ?) "
        "FROM chats c WHERE c.id = ?;", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_int(stmt, 2, chatId);
    sqlite3_step(stmt);
    
    const char *name = sqlite3_column_text(stmt, 0);
    int owner = sqlite3_column_int(stmt, 1);
    const char *created = sqlite3_column_text(stmt, 2);
    int memberCount = sqlite3_column_int(stmt, 3);
    const char *joined = sqlite3_column_text(stmt, 4);
    
    size_t partLen = serializeLen("hiiisstt", name, lastMessageContent);
    size_t dataLen = partLen + memberCount * sizeof(int) * 2;
    *msg = malloc(dataLen);
    char *members = serializeInto(*msg, "hiiisstt", COMMAND_GET_CHATS,
        chatId, owner, lastMessageId, name, lastMessageContent, created, joined);
    
    sqlite3_finalize(stmt);
    sqlite3_prepare_v2(db, "SELECT user, adder FROM memberships WHERE chat = ?;", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, chatId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        members = serializeInto(members, "ii", sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1));
    
    sqlite3_finalize(stmt);
    return dataLen;
}

int main()
{
    signal(SIGINT, interrupt);
    
    if (enet_initialize() < 0)
    {
        printf("Cannot initialize ENet.\n");
        return -1;
    }
    printf("ENet initialized.\n");
    
    ENetAddress address = { ENET_HOST_ANY, 18176 };
    ENetHost *host = enet_host_create(&address, 16, 2, 0, 0);
    if (!host)
    {
        printf("Cannot create host.\n");
        enet_deinitialize();
        printf("ENet deinitialized.\n");
        return -2;
    }
    printf("Host created.\n");
    
    sqlite3 *db;
    if (sqlite3_open("server.db", &db))
    {
        printf("Cannot open database: %s\n", sqlite3_errmsg(db));
        return -3;
    }
    printf("Database opened.\n");
    
    rnd[0] = (size_t)host;
    rnd[2] = (size_t)db;
    rnd[1] = (size_t)&address;
    rnd[3] = (size_t)(void *)main;
    
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS users ( "
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT UNIQUE, "
            "password TEXT, "
            "created DATETIME DEFAULT CURRENT_TIMESTAMP, "
            "seen DATETIME DEFAULT CURRENT_TIMESTAMP); "
        "CREATE TABLE IF NOT EXISTS chats ( "
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT UNIQUE, "
            "owner INTEGER, "
            "created DATETIME DEFAULT CURRENT_TIMESTAMP); "
        "CREATE TABLE IF NOT EXISTS memberships ( "
            "chat INTEGER, "
            "user INTEGER, "
            "adder INTEGER, "
            "joined DATETIME DEFAULT CURRENT_TIMESTAMP, "
            "PRIMARY KEY (chat, user), "
            "FOREIGN KEY (chat) REFERENCES chats(id), "
            "FOREIGN KEY (user) REFERENCES users(id)); "
        "CREATE TABLE IF NOT EXISTS messages ( "
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "chat INTEGER, "
            "sender INTEGER, "
            "type INTEGER, "
            "content BLOB, "
            "sent DATETIME DEFAULT CURRENT_TIMESTAMP, "
            "FOREIGN KEY (chat, sender) REFERENCES memberships(chat, user)); "
        "CREATE TEMP TABLE connections ( "
            "id INTEGER PRIMARY KEY, "
            "user INTEGER, "
            "peer INTEGER, "
            "FOREIGN KEY (user) REFERENCES users(id)); "
        "CREATE TEMP TABLE callMembers ( "
            "chat INTEGER, "
            "user INTEGER, "
            "connection INTEGER UNIQUE, "
            "joined DATETIME DEFAULT CURRENT_TIMESTAMP, "
            "PRIMARY KEY (chat, user), "
            "FOREIGN KEY (chat, user) REFERENCES memberships(chat, user), "
            "FOREIGN KEY (connection, user) REFERENCES connections(id, user)); ",
        0, 0, 0);
    sqlite3_stmt *stmt;
    ENetEvent e;
    int userId = 0;
    int connectionId = 0;
    while (!shouldClose)
    {
        while (enet_host_service(host, &e, 10) > 0)
        {
            userId = (int)((size_t)e.peer->data & 0xFFFFFFFF);
            connectionId = (int)((size_t)e.peer->data >> 32);
            switch (e.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                printf("%x:%hu connected.\n", e.peer->address.host, e.peer->address.port);
                e.peer->data = 0;
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                if (userId)
                {
                    short command;
                    char *data = deserialize(e.packet->data, e.packet->dataLength, "h", &command);
                    if (!data)
                    {
                        printf("#%d: Invalid packet.\n", e.peer->data);
                        enet_packet_destroy(e.packet);
                        break;
                    }
                    size_t dataLen = e.packet->dataLength - serializeLen("h");
                    if (command != COMMAND_CALL_AUDIO)
                        printf("#%d: Received command #%hd:\n", userId, command);
                    
                    switch (command)
                    {
                    case COMMAND_NULL:
                        printf("\tType: NULL.\n");
                        break;
                    case COMMAND_GET_CHATS:
                        printf("\tType: GET_CHATS.\n");
                        if (dataLen)
                            printf("\tInvalid format.\n");
                        else
                        {
                            sqlite3_prepare_v2(db, "SELECT c.id FROM chats c JOIN memberships m "
                                "ON m.chat = c.id WHERE m.user = ?;", -1, &stmt, 0);
                            sqlite3_bind_int(stmt, 1, userId);
                            int chatCount = 0;
                            while (sqlite3_step(stmt) == SQLITE_ROW)
                            {
                                int chatId = sqlite3_column_int(stmt, 0);
                                char *msg;
                                size_t len = getChatData(db, chatId, userId, &msg);
                                enet_peer_send(e.peer, 0, enet_packet_create(msg, len, ENET_PACKET_FLAG_RELIABLE));
                                free(msg);
                                chatCount++;
                            }
                            sqlite3_finalize(stmt);
                            printf("\tSent %d chats.\n", chatCount);
                        }
                        break;
                    case COMMAND_GET_MESSAGES:
                        printf("\tType: GET_MESSAGES.\n");
                        {
                            int chatId;
                            int message;
                            if (deserialize(data, dataLen, "ii", &chatId, &message) != data + dataLen)
                            {
                                printf("\tInvalid format.\n");
                                break;
                            }
                            printf("\tChat id: %d, message id/dir: %d.\n", chatId, message);
                            
                            if (message > 0)
                            {
                                sqlite3_prepare_v2(db, "SELECT id, sender, type, content, sent FROM messages "
                                    "WHERE chat = ? AND id > ? ORDER BY id DESC LIMIT 16;", -1, &stmt, 0);
                                sqlite3_bind_int(stmt, 2, message);
                            }
                            else if (message < 0)
                            {
                                sqlite3_prepare_v2(db, "SELECT id, sender, type, content, sent FROM messages "
                                    "WHERE chat = ? AND id < ? ORDER BY id DESC LIMIT 16;", -1, &stmt, 0);
                                sqlite3_bind_int(stmt, 2, -message);
                            }
                            else
                                sqlite3_prepare_v2(db, "SELECT id, sender, type, content, sent FROM messages "
                                    "WHERE chat = ? ORDER BY id DESC LIMIT 16;", -1, &stmt, 0);
                            sqlite3_bind_int(stmt, 1, chatId);
                            
                            int count = 0;
                            while (sqlite3_step(stmt) == SQLITE_ROW)
                            {
                                int messageId = sqlite3_column_int(stmt, 0);
                                int senderId = sqlite3_column_int(stmt, 1);
                                int type = sqlite3_column_int(stmt, 2);
                                const char *content = sqlite3_column_blob(stmt, 3);
                                int contentLen = sqlite3_column_bytes(stmt, 3);
                                const char *sent = sqlite3_column_text(stmt, 4);
                                
                                ENetPacket *packet = enet_packet_create(
                                    0, serializeLen("hiiiht") + contentLen, ENET_PACKET_FLAG_RELIABLE);
                                memcpy(serializeInto(packet->data, "hiiiht", COMMAND_GET_MESSAGES,
                                    messageId, chatId, senderId, type, content, sent), content, contentLen);
                                enet_peer_send(e.peer, 0, packet);
                                count++;
                            }
                            sqlite3_finalize(stmt);
                            printf("\tSent %d messages.\n", count);
                        }
                        break;
                    case COMMAND_GET_USER:
                        printf("\tType: GET_USER.\n");
                        {
                            int id;
                            if (deserialize(data, dataLen, "i", &id) != data + dataLen)
                            {
                                printf("\tInvalid format.\n");
                                break;
                            }
                            printf("\tUser id: %d.\n", id);
                            
                            sqlite3_prepare_v2(db,
                                "SELECT name, created, seen FROM users WHERE id = ?;", -1, &stmt, 0);
                            sqlite3_bind_int(stmt, 1, id);
                            size_t partLen = serializeLen("hi");
                            ENetPacket *packet = enet_packet_create(0, partLen, ENET_PACKET_FLAG_RELIABLE);
                            serializeInto(packet->data, "hi", COMMAND_GET_USER, id);
                            if (sqlite3_step(stmt) == SQLITE_ROW)
                            {
                                const char *name = sqlite3_column_text(stmt, 0);
                                const char *created = sqlite3_column_text(stmt, 1);
                                const char *seen = sqlite3_column_text(stmt, 2);
                                enet_packet_resize(packet, serializeLen("histt", name));
                                serializeInto(packet->data + partLen, "stt", name, created, seen);
                                printf("\tName: \"%s\".\n", name);
                            }
                            else
                                printf("\tInvalid user.\n");
                            enet_peer_send(e.peer, 0, packet);
                            sqlite3_finalize(stmt);
                        }
                        break;
                    case COMMAND_CHAT_CREATE:
                        printf("\tType: CHAT_CREATE.\n");
                        {
                            char *name = 0;
                            if (deserialize(data, dataLen, "s", &name) != data + dataLen)
                            {
                                printf("\tInvalid format.\n");
                                if (name)
                                    free(name);
                                break;
                            }
                            printf("\tName: \"%s\".\n", name);
                            
                            sqlite3_prepare_v2(db, "INSERT INTO chats (name, owner) VALUES (?, ?);", -1, &stmt, 0);
                            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                            sqlite3_bind_int(stmt, 2, userId);
                            
                            if (sqlite3_step(stmt) == SQLITE_CONSTRAINT)
                            {
                                printf("\tChat exists.\n");
                                ENetPacket *packet = enet_packet_create(
                                    0, serializeLen("h"), ENET_PACKET_FLAG_RELIABLE);
                                serializeInto(packet->data, "h", COMMAND_CHAT_CREATE);
                                enet_peer_send(e.peer, 0, packet);
                            }
                            else
                            {
                                sqlite3_finalize(stmt);
                                sqlite3_prepare_v2(db, "INSERT INTO memberships (chat, user, adder) VALUES "
                                    "(last_insert_rowid(), ?, ?) RETURNING chat, CURRENT_TIMESTAMP;", -1, &stmt, 0);
                                sqlite3_bind_int(stmt, 1, userId);
                                sqlite3_bind_int(stmt, 2, userId);
                                sqlite3_step(stmt);
                                
                                int chatId = sqlite3_column_int(stmt, 0);
                                const char *created = sqlite3_column_text(stmt, 1);
                                printf("\tCreated successfully, id: %d, created: %s.\n", chatId, created);
                                
                                char *msg = serializeNew("hist", COMMAND_CHAT_CREATE, chatId, name, created);
                                broadcastUser(db, userId, msg, serializeLen("hist", name));
                                free(msg);
                                free(name);
                            }
                            sqlite3_finalize(stmt);
                        }
                        break;
                    case COMMAND_CHAT_JOIN:
                        printf("\tType: CHAT_JOIN.\n");
                        {
                            char *name = 0;
                            if (deserialize(data, dataLen, "s", &name) != data + dataLen)
                            {
                                printf("\tInvalid format.\n");
                                if (name)
                                    free(name);
                                break;
                            }
                            printf("\tName: \"%s\".\n", name);
                            
                            sqlite3_prepare_v2(db, "WITH c AS (SELECT * FROM chats WHERE name = ?) "
                                "INSERT INTO memberships (chat, user, adder) "
                                "SELECT c.id, ?, ? FROM c RETURNING chat;", -1, &stmt, 0);
                            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                            sqlite3_bind_int(stmt, 2, userId);
                            sqlite3_bind_int(stmt, 3, userId);
                            sqlite3_step(stmt);
                            if (sqlite3_changes(db))
                            {
                                int chatId = sqlite3_column_int(stmt, 0);
                                printf("\tJoined successfully, id: %d.\n", chatId);
                                
                                char *msg;
                                size_t len = getChatData(db, chatId, userId, &msg);
                                serializeInto(msg, "h", COMMAND_CALL_JOIN);
                                broadcastUser(db, userId, msg, len);
                                free(msg);
                                
                                char *msg2 = serializeNew("hiii", COMMAND_CHAT_ADD, chatId, userId, userId);
                                broadcastChat(db, chatId, msg, serializeLen("hiii"));
                                free(msg2);
                            }
                            else
                            {
                                printf("\tChat doesn't exist or user has already joined.\n");
                                ENetPacket *packet = enet_packet_create(
                                    0, serializeLen("h"), ENET_PACKET_FLAG_RELIABLE);
                                serializeInto(packet->data, "h", COMMAND_CHAT_JOIN);
                                enet_peer_send(e.peer, 0, packet);
                            }
                            sqlite3_finalize(stmt);
                            free(name);
                        }
                        break;
                    case COMMAND_CHAT_ADD:
                        printf("\tType: CHAT_ADD.\n");
                        {
                            int chatId;
                            char *username = 0;
                            if (deserialize(data, dataLen, "is", &username) != data + dataLen)
                            {
                                printf("\tInvalid format.\n");
                                if (username)
                                    free(username);
                                break;
                            }
                            printf("\tChat id: %d, username: \"%s\".\n", chatId, username);
                            
                            sqlite3_prepare_v2(db, "INSERT INTO memberships (chat, user, adder) "
                                "SELECT c.id, u.id, ? FROM users AS u, chats AS c "
                                "WHERE u.name = ? AND c.id = ? RETURNING u.id;", -1, &stmt, 0);
                            sqlite3_bind_int(stmt, 1, userId);
                            sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
                            sqlite3_bind_int(stmt, 3, chatId);
                            sqlite3_step(stmt);
                            if (sqlite3_changes(db))
                            {
                                int id = sqlite3_column_int(stmt, 0);
                                printf("\tAdded successfully, member id: %d.\n", id);
                                
                                char *msg;
                                size_t len = getChatData(db, chatId, userId, &msg);
                                serializeInto(msg, "h", COMMAND_CALL_JOIN);
                                broadcastUser(db, userId, msg, len);
                                free(msg);
                                
                                char *msg2 = serializeNew("hiii", COMMAND_CHAT_ADD, chatId, id, userId);
                                broadcastChat(db, chatId, msg, serializeLen("hiii"));
                                free(msg2);
                            }
                            else
                            {
                                printf("\tChat or user don't exist or the user has already joined.\n");
                                ENetPacket *packet = enet_packet_create(
                                    0, serializeLen("h"), ENET_PACKET_FLAG_RELIABLE);
                                serializeInto(packet->data, "h", COMMAND_CHAT_ADD);
                                enet_peer_send(e.peer, 0, packet);
                            }
                            sqlite3_finalize(stmt);
                            free(username);
                        }
                        break;
                    case COMMAND_CHAT_MESSAGE:
                        printf("\tType: CHAT_MESSAGE.\n");
                        {
                            int chatId;
                            short type;
                            const char *content = deserialize(data, dataLen, "ih", &chatId, &type);
                            if (!content || content == data + dataLen)
                            {
                                printf("\tInvalid format.\n");
                                break;
                            }
                            int contentLen = dataLen - serializeLen("ih");
                            printf("\tChat id: %d, content type: %hd, length: %llu.\n", chatId, type, contentLen);
                            
                            sqlite3_prepare_v2(db, "INSERT INTO messages (chat, sender, type, content) "
                                "VALUES (?, ?, ?, ?) RETURNING CURRENT_TIMESTAMP;", -1, &stmt, 0);
                            sqlite3_bind_int(stmt, 1, chatId);
                            sqlite3_bind_int(stmt, 2, userId);
                            sqlite3_bind_int(stmt, 3, type);
                            sqlite3_bind_blob(stmt, 4, content, contentLen, SQLITE_STATIC);
                            sqlite3_step(stmt);
                            const char *sent = sqlite3_column_text(stmt, 0);
                            int messageId = sqlite3_last_insert_rowid(db);
                            
                            int msgLen = serializeLen("hiiiht") + contentLen;
                            char *msg = malloc(msgLen);
                            memcpy(serializeInto(msg, "hiiiht", COMMAND_GET_MESSAGES,
                                messageId, chatId, userId, type, sent), content, contentLen);
                            broadcastChat(db, chatId, msg, msgLen);
                            free(msg);
                            sqlite3_finalize(stmt);
                        }
                        break;
                    case COMMAND_CALL_JOIN:
                        printf("\tType: CALL_JOIN.\n");
                        {
                            int chatId;
                            if (deserialize(data, dataLen, "i", &chatId) != data + dataLen)
                            {
                                printf("\tInvalid format.\n");
                                break;
                            }
                            printf("\tChat id: %d.\n", chatId);
                            
                            sqlite3_prepare_v2(db,
                                "INSERT INTO callMembers (chat, user, connection) VALUES (?, ?, ?);", -1, &stmt, 0);
                            sqlite3_bind_int(stmt, 1, chatId);
                            sqlite3_bind_int(stmt, 2, userId);
                            sqlite3_bind_int(stmt, 3, connectionId);
                            if (sqlite3_step(stmt) == SQLITE_CONSTRAINT)
                            {
                                sqlite3_finalize(stmt);
                                sqlite3_prepare_v2(db,
                                    "SELECT chat FROM callMembers WHERE connection = ?;", -1, &stmt, 0);
                                sqlite3_bind_int(stmt, 1, connectionId);
                                if (sqlite3_step(stmt) == SQLITE_ROW)
                                {
                                    int oldChat = sqlite3_column_int(stmt, 0);
                                    sqlite3_finalize(stmt);
                                    
                                    char *msg = serializeNew("hi", COMMAND_CALL_LEAVE, userId);
                                    
                                    sqlite3_prepare_v2(db, "SELECT c.peer FROM callMembers cm JOIN connections c "
                                        "ON c.id = cm.connection WHERE cm.chat = ?;", -1, &stmt, 0);
                                    sqlite3_bind_int(stmt, 1, oldChat);
                                    printf("\tLeft previous call, chat id: %d.\n", oldChat);
                                    
                                    while (sqlite3_step(stmt) == SQLITE_ROW)
                                        enet_peer_send((ENetPeer *)sqlite3_column_int64(stmt, 0), 0,
                                            enet_packet_create(msg, 6, ENET_PACKET_FLAG_RELIABLE));
                                    sqlite3_finalize(stmt);
                                    free(msg);
                                
                                    sqlite3_prepare_v2(db,
                                        "UPDATE callMembers SET chat = ? WHERE connection = ?;", -1, &stmt, 0);
                                    sqlite3_bind_int(stmt, 1, chatId);
                                    sqlite3_bind_int(stmt, 2, connectionId);
                                    if (sqlite3_step(stmt) != SQLITE_DONE)
                                        chatId = 0;
                                }
                                else
                                    chatId = 0;
                            }
                            sqlite3_finalize(stmt);
                            
                            if (chatId)
                            {
                                char *msg = serializeNew("hi", COMMAND_CALL_JOIN, userId);
                                
                                sqlite3_prepare_v2(db,
                                    "SELECT COUNT(*) FROM callMembers WHERE chat = ?;", -1, &stmt, 0);
                                sqlite3_bind_int(stmt, 1, chatId);
                                sqlite3_step(stmt);
                                int memberCount = sqlite3_column_int(stmt, 0);
                                sqlite3_finalize(stmt);
                                printf("\tJoined successfully, member count: %d.\n", memberCount);
                                
                                sqlite3_prepare_v2(db, "SELECT c.user, c.peer FROM callMembers cm "
                                    "JOIN connections c ON c.id = cm.connection WHERE cm.chat = ?;", -1, &stmt, 0);
                                sqlite3_bind_int(stmt, 1, chatId);
                                
                                ENetPacket *packet = enet_packet_create(
                                    0, 2 + memberCount * 4, ENET_PACKET_FLAG_RELIABLE);
                                char *members = serializeInto(packet->data, "h", COMMAND_CALL_MEMBERS);
                                for (int i = 0; sqlite3_step(stmt) == SQLITE_ROW; i++)
                                {
                                    members = serializeInto(members, "i", sqlite3_column_int(stmt, 0));
                                    if (sqlite3_column_int(stmt, 0) != userId)
                                        enet_peer_send((ENetPeer *)sqlite3_column_int64(stmt, 1), 0,
                                            enet_packet_create(msg, 6, ENET_PACKET_FLAG_RELIABLE));
                                }
                                sqlite3_finalize(stmt);
                                enet_peer_send(e.peer, 0, packet);
                                free(msg);
                            }
                            else
                            {
                                printf("\tUser is not in the chat.\n");
                                ENetPacket *packet = enet_packet_create(0, 2, ENET_PACKET_FLAG_RELIABLE);
                                serializeInto(packet->data, "h", COMMAND_CALL_MEMBERS);
                                enet_peer_send(e.peer, 0, packet);
                            }
                        }
                        break;
                    case COMMAND_CALL_LEAVE:
                        printf("\tType: CALL_LEAVE.\n");
                        if (dataLen)
                            printf("\tInvalid format.\n");
                        else
                        {
                            char *msg = serializeNew("hi", COMMAND_CALL_LEAVE, userId);
                            
                            sqlite3_prepare_v2(db,
                                "DELETE FROM callMembers WHERE connection = ? RETURNING chat;", -1, &stmt, 0);
                            sqlite3_bind_int(stmt, 1, connectionId);
                            if (sqlite3_step(stmt) == SQLITE_ROW)
                            {
                                int chatId = sqlite3_column_int(stmt, 0);
                                printf("\tLeft call successfully, chat id: %d\n", chatId);
                                
                                sqlite3_finalize(stmt);
                                sqlite3_prepare_v2(db, "SELECT c.peer FROM callMembers cm "
                                    "JOIN connections c ON c.id = cm.connection WHERE cm.chat = ?;", -1, &stmt, 0);
                                sqlite3_bind_int(stmt, 1, chatId);
                                
                                while (sqlite3_step(stmt) == SQLITE_ROW)
                                    enet_peer_send((ENetPeer *)sqlite3_column_int64(stmt, 0), 0,
                                        enet_packet_create(msg, 6, ENET_PACKET_FLAG_RELIABLE));
                                
                                enet_peer_send(e.peer, 0, enet_packet_create(msg, 6, ENET_PACKET_FLAG_RELIABLE));
                            }
                            else
                            {
                                printf("\tUser is not in a call.\n");
                                enet_peer_send(e.peer, 0, enet_packet_create(msg, 2, ENET_PACKET_FLAG_RELIABLE));
                            }
                            sqlite3_finalize(stmt);
                            free(msg);
                        }
                        break;
                    case COMMAND_CALL_AUDIO:
                        if (dataLen > 510)
                            printf("\tData is too big.\n");
                        else
                        {
                            sqlite3_prepare_v2(db,
                                "SELECT c.peer FROM callMembers cm JOIN connections c ON c.id = cm.connection "
                                "WHERE cm.chat = (SELECT chat FROM callMembers WHERE connection = ?) "
                                "AND cm.connection != ?;", -1, &stmt, 0);
                            sqlite3_bind_int(stmt, 1, connectionId);
                            sqlite3_bind_int(stmt, 2, connectionId);
                            
                            char *msg = malloc(serializeLen("hi") + dataLen);
                            memcpy(serializeInto(msg, "hi", COMMAND_CALL_AUDIO, userId), data, dataLen);
                            while (sqlite3_step(stmt) == SQLITE_ROW)
                                enet_peer_send((ENetPeer *)sqlite3_column_int64(stmt, 0), 1,
                                    enet_packet_create(msg, dataLen + 6, ENET_PACKET_FLAG_UNSEQUENCED));
                            sqlite3_finalize(stmt);
                            free(msg);
                        }
                        break;
                    default:
                        printf("\tType: Invalid.\n");
                        break;
                    }
                }
                else
                {
                    printf("%x:%hu authenticates:\n", e.peer->address.host, e.peer->address.port);
                    
                    char authMode;
                    char *username = 0;
                    char *password = 0;
                    if ((void *)deserialize(e.packet->data, e.packet->dataLength, "css",
                            &authMode, &username, &password) != (void *)(e.packet->data + e.packet->dataLength))
                    {
                        printf("\tInvalid format, %d, %s, %s.\n", authMode, username, password);
                        if (username)
                            free(username);
                        if (password)
                            free(password);
                        break;
                    }
                    
                    int id = 0;
                    printf("\tName: \"%s\".\n", username);
                    if (authMode == 1)
                    {
                        printf("\tSigning up...\n");
                        char encoded[128];
                        argon2id_hash_encoded(3, 1 << 16, 1, password, strlen(password),
                            nextSalt(), SALT_LENGTH, 32, encoded, sizeof(encoded));
                        
                        sqlite3_prepare_v2(db, "INSERT INTO users (name, password) VALUES (?, ?);", -1, &stmt, 0);
                        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 2, encoded, strlen(encoded), SQLITE_STATIC);
                        if (sqlite3_step(stmt) == SQLITE_CONSTRAINT)
                            printf("\tUser exists.\n");
                        else
                        {
                            id = sqlite3_last_insert_rowid(db);
                            e.peer->data = (void *)(size_t)id;
                            printf("\tSigned up successfully, user id: %d.\n", id);
                        }
                    }
                    else if (authMode == 2)
                    {
                        printf("\tLogging in...\n");
                        
                        sqlite3_prepare_v2(db, "SELECT id, password FROM users WHERE name = ?;", -1, &stmt, 0);
                        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
                        
                        if (sqlite3_step(stmt) == SQLITE_ROW)
                        {
                            id = sqlite3_column_int(stmt, 0);
                            const char *encoded = sqlite3_column_text(stmt, 1);
                            if (argon2id_verify(encoded, password, strlen(password)) == ARGON2_OK)
                            {
                                e.peer->data = (void *)(size_t)id;
                                printf("\tLogged in successfully, user id: %d.\n", id);
                            }
                            else
                                printf("\tIncorrect password, user id: %d.\n", id);
                        }
                        else
                            printf("\tIncorrect username.\n");
                    }
                    else
                        printf("\tInvalid authentication mode.\n");
                    
                    if (id)
                    {
                        sqlite3_finalize(stmt);
                        sqlite3_prepare_v2(db, "INSERT INTO connections (user, peer) VALUES (?, ?);", -1, &stmt, 0);
                        sqlite3_bind_int(stmt, 1, id);
                        sqlite3_bind_int64(stmt, 2, (size_t)e.peer);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        printf("\tConnection id: %d.\n", sqlite3_last_insert_rowid(db));
                        e.peer->data = (void *)((size_t)e.peer->data | (size_t)sqlite3_last_insert_rowid(db) << 32);
                        
                        sqlite3_prepare_v2(db,
                            "UPDATE users SET seen = '-------------------' WHERE id = ?", -1, &stmt, 0);
                        sqlite3_bind_int(stmt, 1, id);
                        sqlite3_step(stmt);
                    }
                    enet_peer_send(e.peer, 0, enet_packet_create(&id, 4, ENET_PACKET_FLAG_RELIABLE));
                    sqlite3_finalize(stmt);
                    free(username);
                    free(password);
                }
                
                enet_packet_destroy(e.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                if (!e.peer->data)
                {
                    printf("%x:%hu disconnected.\n", e.peer->address.host, e.peer->address.port);
                    break;
                }
                printf("#%d disconnects...\n", userId);
                
                sqlite3_prepare_v2(db,
                    "DELETE FROM callMembers WHERE connection = ? RETURNING chat;", -1, &stmt, 0);
                sqlite3_bind_int(stmt, 1, connectionId);
                if (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    char *msg = serializeNew("hi", COMMAND_CALL_LEAVE, userId);
                
                    int chatId = sqlite3_column_int(stmt, 0);
                    printf("\tLeft call successfully, chat id: %d\n", chatId);
                    
                    sqlite3_finalize(stmt);
                    sqlite3_prepare_v2(db, "SELECT c.peer FROM callMembers cm "
                        "JOIN connections c ON c.id = cm.connection WHERE cm.chat = ?;", -1, &stmt, 0);
                    sqlite3_bind_int(stmt, 1, chatId);
                    
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                        enet_peer_send((ENetPeer *)sqlite3_column_int64(stmt, 0), 0,
                            enet_packet_create(msg, 6, ENET_PACKET_FLAG_RELIABLE));
                    
                    enet_peer_send(e.peer, 0, enet_packet_create(msg, 6, ENET_PACKET_FLAG_RELIABLE));
                    free(msg);
                }
                sqlite3_finalize(stmt);
                sqlite3_prepare_v2(db, "DELETE FROM connections WHERE id = ?;", -1, &stmt, 0);
                sqlite3_bind_int(stmt, 1, connectionId);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                sqlite3_prepare_v2(db, "UPDATE users u SET seen = CURRENT_TIMESTAMP WHERE u.id = ? AND "
                    "NOT EXISTS (SELECT 1 FROM connections c WHERE c.user = u.id);", -1, &stmt, 0);
                sqlite3_bind_int(stmt, 1, userId);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                printf("\tDisconnected successfully.\n");
                break;
            }
        }
    }
    
    sqlite3_close(db);
    printf("Database closed.\n");
    enet_host_destroy(host);
    printf("Host destroyed.\n");
    enet_deinitialize();
    printf("ENet deinitialized.\n");
    return 0;
}
