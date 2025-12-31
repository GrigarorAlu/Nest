// client.c

#include <stdatomic.h>
#include <stdio.h>

#include "../ext/enet/enet.h"
#include "../ext/miniaudio.h"
#include "../ext/opus/opus.h"
#include "../ext/sqlite3.h"

#include "ui.h"
#include "../protocol.h"
#include "../utils.h"

static int connected = 0;
static int myId = 0;

static ENetAddress server = { 0 };
static ENetHost *host = 0;
static ENetPeer *peer = 0;

static sqlite3 *db = 0;
static sqlite3 *accountDb = 0;

static ma_device speaker = { 0 };
static ma_device mic = { 0 };
static ma_device_config micConfig = { 0 };
static ma_device_config speakerConfig = { 0 };

static OpusEncoder *encoder = 0;
static OpusDecoder *decoder = 0;

static atomic_bool audioActive = 0;
static atomic_bool audioPlaying = 0;

static void micHandler(ma_device *device, void *out, const void *in, ma_uint32 frameCount)
{
    if (frameCount != AUDIO_SAMPLE_SIZE)
        return;
    
    char buffer[512];
    int len;
    
    serializeInto(buffer, "h", COMMAND_CALL_AUDIO);
    if ((len = opus_encode(encoder, in, 320, buffer + 2, sizeof(buffer) - 2)) >= 0)
        enet_peer_send(peer, 1, enet_packet_create(buffer, len + 2, ENET_PACKET_FLAG_UNSEQUENCED));
}

static void speakerHandler(ma_device *device, void *out, const void *in, ma_uint32 frameCount)
{
    CallMember *callMembers = getCallMembers();
    size_t callMemberCount = getCallMemberCount();
    size_t activeCount = callMemberCount;
    memset(out, 0, frameCount * 2);
    
    audioPlaying = 1;
    if (!audioActive || callMemberCount <= 1 || frameCount != AUDIO_SAMPLE_SIZE)
    {
        audioPlaying = 0;
        return;
    }
    
    for (size_t i = 0; i < callMemberCount; i++)
    {
        CallMember *member = &callMembers[i];
        if ((member->writePos - member->readPos + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE >= AUDIO_SAMPLE_SIZE &&
                member->userId != myId)
            for (int j = 0; j < AUDIO_SAMPLE_SIZE; j++)
            {
                int mixed = ((short *)out)[j] + member->buffer[member->readPos] * member->volume / 100;
                ((short *)out)[j] = mixed > 32767 ? 32767 : (mixed < -32768 ? -32768 : mixed);
                member->readPos = (member->readPos + 1) % AUDIO_BUFFER_SIZE;
            }
        else
            activeCount--;
    }
    audioPlaying = 0;
    
    if (activeCount >= 2)
        for (size_t i = 0; i < AUDIO_SAMPLE_SIZE; i++)
            ((short *)out)[i] /= activeCount;
}

static void onAccountOpen(int id)
{
#ifdef __ANDROID__
    char path[] = "/data/data/com.grigaror.nest/files/0000000000.db";
#else
    char path[] = "0000000000.db";
#endif

    for (int i = sizeof(path) - 5; id; i--)
    {
        path[i] = '0' + id % 10;
        id /= 10;
    }
    
    if (sqlite3_open(path, &db) != SQLITE_OK)
        return;
    
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS users ( "
            "id INTEGER PRIMARY KEY, "
            "name TEXT UNIQUE, "
            "created DATETIME, "
            "seen DATETIME);"
        "CREATE TABLE IF NOT EXISTS chats ( "
            "id INTEGER PRIMARY KEY, "
            "name TEXT UNIQUE, "
            "owner INTEGER, "
            "created DATETIME, "
            "joined DATETIME);"
        "CREATE TABLE IF NOT EXISTS memberships ( "
            "chat INTEGER, "
            "user INTEGER, "
            "adder INTEGER, "
            "PRIMARY KEY (chat, user), "
            "FOREIGN KEY (chat) REFERENCES chats(id), "
            "FOREIGN KEY (user) REFERENCES users(id));"
        "CREATE TABLE IF NOT EXISTS messages ( "
            "id INTEGER PRIMARY KEY, "
            "chat INTEGER, "
            "sender INTEGER, "
            "type INTEGER, "
            "content BLOB, "
            "sent DATETIME, "
            "FOREIGN KEY (chat) REFERENCES chats(id), "
            "FOREIGN KEY (sender) REFERENCES users(id));",
        0, 0, 0);
    
    sqlite3_stmt *stmt;
    sqlite3_stmt *sub;
    sqlite3_prepare_v2(db, "SELECT * FROM chats;", -1, &stmt, 0);
    sqlite3_prepare_v2(db, "SELECT id, content FROM messages "
        "WHERE type = ? AND chat = ? ORDER BY id DESC LIMIT 1;", -1, &sub, 0);
        
    sqlite3_bind_int(sub, 1, MESSAGE_TEXT);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        Chat chat;
        chat.id = sqlite3_column_int(stmt, 0);
        chat.name = strdup(sqlite3_column_text(stmt, 1));
        chat.ownerId = sqlite3_column_int(stmt, 2);
        chat.created = strdup(sqlite3_column_text(stmt, 3));
        chat.joined = strdup(sqlite3_column_text(stmt, 4));
        chat.memberships = 0;
        chat.memberCount = 0;
        
        sqlite3_bind_int(sub, 2, chat.id);
        int res = sqlite3_step(sub);
        chat.lastMessageId = res == SQLITE_ROW ? sqlite3_column_int(sub, 0) : 0;
        chat.lastMessageContent = res == SQLITE_ROW ? strdup(sqlite3_column_text(sub, 1)) : 0;
        sqlite3_reset(sub);
        
        addChat(chat);
    }
    sqlite3_finalize(stmt);
    sqlite3_finalize(sub);
    
    sqlite3_prepare_v2(db, "SELECT * FROM memberships;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        Membership membership;
        membership.chatId = sqlite3_column_int(stmt, 0);
        membership.userId = sqlite3_column_int(stmt, 1);
        membership.adderId = sqlite3_column_int(stmt, 2);
        addMembership(membership);
    }
    sqlite3_finalize(stmt);
}

static void onAccountSave(int id, const char *name, const char *password)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(accountDb, "INSERT INTO accounts VALUES (?, ?, ?)", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, password, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void onStartup()
{
#ifdef __ANDROID__
    FILE *file = fopen("/data/data/com.grigaror.nest/files/default_connection.txt", "rb");
#else
    FILE *file = fopen("default_connection.txt", "rb");
#endif
    
    if (!file)
    {
#ifdef __ANDROID__
        file = fopen("/data/data/com.grigaror.nest/files/default_connection.txt", "w+b");
#else
        file = fopen("default_connection.txt", "w+b");
#endif
        fputs("127.0.0.1:12345", file);
    }
    
    char buffer[sizeof("XXX.XXX.XXX.XXX:YYYYY\n")];
    if (fgets(buffer, sizeof(buffer), file))
    {
        char *delim = strchr(buffer, ':');
        char *end = strchr(buffer, '\n');
        if (!end)
            end = buffer + strlen(buffer);
        *delim = 0;
        *end = 0;
        
        enet_address_set_host(&server, buffer);
        server.port = strtoul(delim + 1, 0, 0);
    }
    
    fclose(file);
}

static int onSignup(const char *username, const char *password)
{
    if (!connected)
        return 0;
    
    ENetPacket *packet = enet_packet_create(0, serializeLen("css", username, password), ENET_PACKET_FLAG_RELIABLE);
    serializeInto(packet->data, "css", 1, username, password);
    
    ENetEvent e;
    enet_peer_send(peer, 0, packet);
    if (enet_host_service(host, &e, 3000) > 0 && e.type == ENET_EVENT_TYPE_RECEIVE)
    {
        myId = *(int *)e.packet->data;
        enet_packet_destroy(e.packet);
        return myId;
    }
    return 0;
}

static int onLogin(const char *username, const char *password)
{
    if (!connected)
        return 0;
    
    ENetPacket *packet = enet_packet_create(0, serializeLen("css", username, password), ENET_PACKET_FLAG_RELIABLE);
    serializeInto(packet->data, "css", 2, username, password);
    
    ENetEvent e;
    enet_peer_send(peer, 0, packet);
    if (enet_host_service(host, &e, 3000) > 0 && e.type == ENET_EVENT_TYPE_RECEIVE)
    {
        packet = enet_packet_create(0, 2, ENET_PACKET_FLAG_RELIABLE);
        *(short *)packet->data = COMMAND_GET_CHATS;
        enet_peer_send(peer, 0, packet);
        myId = *(int *)e.packet->data;
        enet_packet_destroy(e.packet);
        return myId;
    }
    return 0;
}

static void onChatCreate(const char *name)
{
    if (!connected)
        return;
    
    ENetPacket *packet = enet_packet_create(0, serializeLen("hs", name), ENET_PACKET_FLAG_RELIABLE);
    serializeInto(packet->data, "hs", COMMAND_CHAT_CREATE, name);
    enet_peer_send(peer, 0, packet);
}

static void onChatJoin(const char *name)
{
    if (!connected)
        return;
    
    ENetPacket *packet = enet_packet_create(0, serializeLen("hs", name), ENET_PACKET_FLAG_RELIABLE);
    serializeInto(packet->data, "hs", COMMAND_CHAT_JOIN, name);
    enet_peer_send(peer, 0, packet);
}

static void onMessageSend(int chatId, int type, const char *content, size_t len)
{
    if (!connected)
        return;
    
    len = len ? len : strlen(content);
    ENetPacket *packet = enet_packet_create(0, serializeLen("hih") + len, ENET_PACKET_FLAG_RELIABLE);
    memcpy(serializeInto(packet->data, "hih", COMMAND_CHAT_MESSAGE, chatId, type), content, len);
    enet_peer_send(peer, 0, packet);
}

static void onUserRequest(int userId)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT name, created, seen FROM users WHERE id = ?", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, userId);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *name = sqlite3_column_text(stmt, 0);
        const char *created = sqlite3_column_text(stmt, 1);
        const char *seen = sqlite3_column_text(stmt, 2);
        updateUser((User){ userId, strdup(name), strdup(created), strdup(seen) });
    }
    else if (connected)
    {
        ENetPacket *packet = enet_packet_create(0, serializeLen("hi"), ENET_PACKET_FLAG_RELIABLE);
        serializeInto(packet->data, "hi", COMMAND_GET_USER, userId);
        enet_peer_send(peer, 0, packet);
    }
    sqlite3_finalize(stmt);
}

static void onMessageRequest(int chatId, int messageId)
{
    // TODO: firstMessageType and add a type for the first message.
    // Also add types for event messages, e.g. join, etc.
    sqlite3_stmt *stmt;
    if (messageId)
    {
        sqlite3_prepare_v2(db,
            "SELECT * FROM messages WHERE chat = ? AND id < ? ORDER BY id DESC LIMIT 16;", -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, chatId);
        sqlite3_bind_int(stmt, 2, messageId);
        int firstMessageId = messageId;
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int id = sqlite3_column_int(stmt, 0);
            int senderId = sqlite3_column_int(stmt, 2);
            int type = sqlite3_column_int(stmt, 3);
            const char *content = sqlite3_column_blob(stmt, 4);
            int contentLen = sqlite3_column_bytes(stmt, 4);
            const char *sent = sqlite3_column_text(stmt, 5);
            if (id < firstMessageId)
                firstMessageId = id;
            count++;
            addMessage((Message) { id, chatId, senderId, type,
                memdup(content, contentLen), contentLen, strdup(sent) });
        }
        sqlite3_finalize(stmt);
        
        if (connected && count < 16 && firstMessageId > 1)
        {
            ENetPacket *packet = enet_packet_create(0, serializeLen("hii"), ENET_PACKET_FLAG_RELIABLE);
            serializeInto(packet->data, "hii", COMMAND_GET_MESSAGES, chatId, -firstMessageId);
            enet_peer_send(peer, 0, packet);
        }
    }
    else
    {
        clearMessages();
        sqlite3_prepare_v2(db, "SELECT * FROM messages WHERE chat = ? ORDER BY id DESC LIMIT 16;", -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, chatId);
        int lastMessageId = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int id = sqlite3_column_int(stmt, 0);
            int senderId = sqlite3_column_int(stmt, 2);
            int type = sqlite3_column_int(stmt, 3);
            const char *content = sqlite3_column_blob(stmt, 4);
            int contentLen = sqlite3_column_bytes(stmt, 4);
            const char *sent = sqlite3_column_text(stmt, 5);
            if (id > lastMessageId)
                lastMessageId = id;
            addMessage((Message) { id, chatId, senderId, type,
                memdup(content, contentLen), contentLen, strdup(sent) });
        }
        sqlite3_finalize(stmt);
        
        if (connected)
        {
            ENetPacket *packet = enet_packet_create(0, serializeLen("hii"), ENET_PACKET_FLAG_RELIABLE);
            serializeInto(packet->data, "hii", COMMAND_GET_MESSAGES, chatId, lastMessageId);
            enet_peer_send(peer, 0, packet);
        }
    }
}

static void onCallJoin(int chatId)
{
    if (!connected)
    {
        setStatus("No connection", -1);
        return;
    }
    
    int status = 0;
    int opusErr;
    
    speakerConfig = ma_device_config_init(ma_device_type_playback);
    speakerConfig.playback.format = ma_format_s16;
    speakerConfig.playback.channels = 1;
    speakerConfig.sampleRate = 16000;
    speakerConfig.periodSizeInFrames = 320;
    speakerConfig.dataCallback = speakerHandler;
    if (ma_device_init(0, &speakerConfig, &speaker) == MA_SUCCESS)
    {
        decoder = opus_decoder_create(16000, 1, &opusErr);
        if (opusErr == OPUS_OK)
            ma_device_start(&speaker);
        else
        {
            status |= 1;
            opus_decoder_destroy(decoder);
            ma_device_uninit(&speaker);
            decoder = 0;
        }
    }
    else
        status |= 3;
    
    micConfig = ma_device_config_init(ma_device_type_capture);
    micConfig.capture.format = ma_format_s16;
    micConfig.capture.channels = 1;
    micConfig.sampleRate = 16000;
    micConfig.periodSizeInFrames = 320;
    micConfig.dataCallback = micHandler;
    if (ma_device_init(0, &micConfig, &mic) == MA_SUCCESS)
    {
        encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &opusErr);
        if (opusErr == OPUS_OK)
        {
            opus_encoder_ctl(encoder, OPUS_SET_BITRATE(16000));
            opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(1));
            opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(10));
            opus_encoder_ctl(encoder, OPUS_SET_DTX(1));
            opus_encoder_ctl(encoder, OPUS_SET_VBR(1));
            opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(10));
            ma_device_start(&mic);
        }
        else
        {
            status |= 4;
            opus_encoder_destroy(encoder);
            ma_device_uninit(&mic);
            encoder = 0;
        }
    }
    else
        status |= 12;
    
    setStatus(status & 1 ? (status & 4 ? "No sound input/output" : "No sound output") :
        (status & 4 ? "No sound input" : 0), status);
    
    ENetPacket *packet = enet_packet_create(0, serializeLen("hi"), ENET_PACKET_FLAG_RELIABLE);
    serializeInto(packet->data, "hi", COMMAND_CALL_JOIN, chatId);
    enet_peer_send(peer, 0, packet);
}

static void onCallLeave()
{
    ma_device_uninit(&speaker);
    ma_device_uninit(&mic);
    opus_decoder_destroy(decoder);
    opus_encoder_destroy(encoder);
    clearCall();
    encoder = 0;
    decoder = 0;
    audioActive = 0;
    audioPlaying = 0;
    
    if (connected)
    {
        ENetPacket *packet = enet_packet_create(0, serializeLen("h"), ENET_PACKET_FLAG_RELIABLE);
        serializeInto(packet->data, "h", COMMAND_CALL_LEAVE);
        enet_peer_send(peer, 0, packet);
    }
}

static void processChatData(char *data, size_t dataLen)
{
    int chatId;
    int ownerId;
    int lastMessageId;
    char *name;
    char *lastMessageContent;
    char *created;
    char *joined;
    char *members = deserialize(data, dataLen, "iiisstt",
        &chatId, &ownerId, &lastMessageId, &name, &lastMessageContent, &created, &joined);
    size_t memberCount = dataLen - serializeLen("iiisstt", name, lastMessageContent);
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT INTO chats VALUES (?, ?, ?, ?, ?);", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, chatId);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, ownerId);
    sqlite3_bind_text(stmt, 4, created, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, joined, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_CONSTRAINT)
    {
        addChat((Chat){ chatId, name, ownerId, created, joined, 0, 0, lastMessageId, lastMessageContent });
        
        sqlite3_finalize(stmt);
        sqlite3_prepare_v2(db, "INSERT INTO memberships VALUES (last_insert_rowid(), ?, ?);", -1, &stmt, 0);
        sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);
        for (size_t i = 0; i < memberCount; i += 2)
        {
            int memberId;
            int adderId;
            members = deserialize(members, memberCount * sizeof(int) * 2, "ii", &memberId, &adderId);
            addMembership((Membership) { chatId, memberId, adderId });
            
            sqlite3_bind_int(stmt, 1, memberId);
            sqlite3_bind_int(stmt, 2, adderId);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    }
    sqlite3_finalize(stmt);
}

#include "res/res.h"

int main()
{
    setHandlers(onStartup, onSignup, onLogin, onAccountOpen, onAccountSave,
        onChatCreate, onChatJoin, onMessageSend, onUserRequest, onMessageRequest, onCallJoin, onCallLeave);
    UI_open();
    
    if (enet_initialize())
        return -1;
    if (!(host = enet_host_create(0, 1, 2, 0, 0)))
        return -2;
    
#ifdef __ANDROID__
    if (sqlite3_open("/data/data/com.grigaror.nest/files/client.db", &accountDb) != SQLITE_OK)
#else
    if (sqlite3_open(".db", &accountDb) != SQLITE_OK)
#endif
        return -3;
    
    sqlite3_exec(accountDb,
        "CREATE TABLE IF NOT EXISTS accounts ( "
            "user INTEGER PRIMARY KEY, "
            "name TEXT UNIQUE, "
            "password TEXT);",
        0, 0, 0);
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(accountDb, "SELECT * FROM accounts;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int user = sqlite3_column_int(stmt, 0);
        const char *name = sqlite3_column_text(stmt, 1);
        const char *password = sqlite3_column_text(stmt, 2);
        
        addAccount((Account) { user, strdup(name), strdup(password) });
    }
    sqlite3_finalize(stmt);
    
    ENetEvent e;
    int reconnectTmr = 0;

#ifdef __ANDROID__
    for (;;)
    {
        UI_draw();
#else
    while (UI_draw())
    {
#endif
        while (enet_host_service(host, &e, 10) > 0)
            switch (e.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                connected = 1;
                reconnectTmr = 0;
                if (!authenticate() && myId)
                {
                    connected = 0;
                    enet_peer_disconnect(peer, 0);
                }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                short command;
                char *data = deserialize(e.packet->data, e.packet->dataLength, "h", &command);
                size_t dataLen = e.packet->dataLength - serializeLen("h");
                
                switch (command)
                {
                case COMMAND_GET_CHATS:
                    processChatData(data, dataLen);
                    break;
                case COMMAND_GET_MESSAGES:
                    {
                        int messageId;
                        int chat;
                        int sender;
                        int type;
                        char *sent;
                        char *content = deserialize(data, dataLen, "iiiht", &messageId, &chat, &sender, &type, &sent);
                        size_t contentLen = dataLen - serializeLen("iiiht");
                        
                        sqlite3_prepare_v2(db, "INSERT INTO messages VALUES (?, ?, ?, ?, ?, ?);", -1, &stmt, 0);
                        sqlite3_bind_int(stmt, 1, messageId);
                        sqlite3_bind_int(stmt, 2, chat);
                        sqlite3_bind_int(stmt, 3, sender);
                        sqlite3_bind_int(stmt, 4, type);
                        sqlite3_bind_blob(stmt, 5, content, contentLen, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 6, sent, -1, SQLITE_STATIC);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        
                        addMessage((Message){ messageId, chat, sender, type,
                            memdup(content, contentLen), contentLen, sent });
                    }
                    break;
                case COMMAND_GET_USER:
                    if (dataLen != 4)
                    {
                        int userId;
                        char *name;
                        char *created;
                        char *seen;
                        deserialize(data, dataLen, "istt", &userId, &name, &created, &seen);
                        
                        sqlite3_prepare_v2(db, "INSERT INTO users VALUES (?, ?, ?, ?);", -1, &stmt, 0);
                        sqlite3_bind_int(stmt, 1, userId);
                        sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 3, created, -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 4, seen, -1, SQLITE_STATIC);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        
                        updateUser((User){ userId, name, created, seen });
                    }
                    break;
                case COMMAND_CHAT_CREATE:
                    if (!dataLen)
                        setStatus("Chat already exists", -1);
                    else
                    {
                        int chatId;
                        char *name;
                        char *created;
                        deserialize(data, dataLen, "ist", &chatId, &name, &created);
                        
                        sqlite3_prepare_v2(db, "INSERT INTO chats VALUES (?, ?, ?, ?, ?);", -1, &stmt, 0);
                        sqlite3_bind_int(stmt, 1, chatId);
                        sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
                        sqlite3_bind_int(stmt, 3, myId);
                        sqlite3_bind_text(stmt, 4, created, -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 5, created, -1, SQLITE_STATIC);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        sqlite3_prepare_v2(db, "INSERT INTO memberships "
                            "VALUES (last_insert_rowid(), ?, ?);", -1, &stmt, 0);
                        sqlite3_bind_int(stmt, 1, myId);
                        sqlite3_bind_int(stmt, 2, myId);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        
                        addChat((Chat){ chatId, strdup(name), myId, created, created, memdup(&myId, 4), 1 });
                        
                        setStatus(0, chatId);
                    }
                    break;
                case COMMAND_CHAT_JOIN:
                    if (!dataLen)
                    {
                        setStatus("Chat doesn't exist or you have already joined", -1);
                        break;
                    }
                    processChatData(data, dataLen);
                    int chatId;
                    deserialize(data, dataLen, "i", &chatId);
                    setStatus(0, chatId);
                    break;
                case COMMAND_CHAT_ADD:
                    if (!dataLen)
                        setStatus("Chat or user don't exist or user has already joined", -1);
                    else
                    {
                        int chatId;
                        int userId;
                        int adderId;
                        deserialize(data, dataLen, "iii", &chatId, &userId, &adderId);
                        
                        sqlite3_prepare_v2(db,
                            "INSERT INTO memberships VALUES (?, ?, ?);", -1, &stmt, 0);
                        sqlite3_bind_int(stmt, 1, chatId);
                        sqlite3_bind_int(stmt, 2, userId);
                        sqlite3_bind_int(stmt, 3, adderId);
                        
                        addMembership((Membership){ chatId, userId, adderId });
                        
                        setStatus(0, userId);
                    }
                    break;
                case COMMAND_CALL_MEMBERS:
                    if (dataLen)
                    {
                        int userId;
                        char *members = data;
                        for (int i = 0; i < dataLen / 4; i++)
                        {
                            members = deserialize(members, 4, "i", &userId);
                            addCallMember(userId);
                        }
                        audioActive = 1;
                    }
                    else
                        setStatus("Cannot join call.\n", -1);
                    break;
                case COMMAND_CALL_JOIN:
                    {
                        int userId;
                        deserialize(data, dataLen, "i", &userId);
                        audioActive = 0;
                        while (audioPlaying);
                        addCallMember(userId);
                        audioActive = 1;
                    }
                    break;
                case COMMAND_CALL_LEAVE:
                    if (!dataLen)
                        setStatus("Cannot leave call.\n", -1);
                    else
                    {
                        int userId;
                        deserialize(data, dataLen, "i", &userId);
                        if (userId != myId)
                        {
                            audioActive = 0;
                            while (audioPlaying);
                            removeCallMember(userId);
                            audioActive = 1;
                        }
                    }
                    break;
                case COMMAND_CALL_AUDIO:
                    if (decoder)
                    {
                        int userId;
                        char *audio = deserialize(data, dataLen, "i", &userId);
                        int audioLen = dataLen - serializeLen("i");
                        
                        CallMember *member = getCallMember(userId);
                        short buffer[AUDIO_SAMPLE_SIZE];
                        int len = opus_decode(decoder, audio, audioLen, buffer, AUDIO_SAMPLE_SIZE, 0);
                        if (len == OPUS_INVALID_PACKET)
                            len = opus_decode(decoder, 0, 0, buffer, AUDIO_SAMPLE_SIZE, 0);
                        if (len > 0)
                            for (int j = 0; j < len; j++)
                            {
                                member->buffer[member->writePos] = buffer[j];
                                member->writePos = (member->writePos + 1) % AUDIO_BUFFER_SIZE;
                            }
                        break;
                    }
                    break;
                }
                enet_packet_destroy(e.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                connected = 0;
                peer = 0;
                reconnectTmr = 0;
                onCallLeave();
                break;
            }
        
        if (!connected)
        {
            if (peer)
            {
                if (reconnectTmr > 5000)
                {
                    enet_peer_reset(peer);
                    peer = 0;
                    reconnectTmr = 0;
                }
            }
            else
            {
                peer = enet_host_connect(host, &server, 2, 0);
                reconnectTmr = 0;
            }
            reconnectTmr += getFrameTime();
        }   
    }
    
#ifndef __ANDROID__
    UI_close();
    
    ma_device_uninit(&mic);
    ma_device_uninit(&speaker);
    
    opus_encoder_destroy(encoder);
    opus_decoder_destroy(decoder);
    
    if (connected)
    {
        enet_peer_disconnect(peer, 0);
        enet_host_service(host, &e, 3000);
        switch (e.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(e.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            peer = 0;
        }
    }
    if (peer)
        enet_peer_reset(peer);
    enet_host_destroy(host);
    enet_deinitialize();
    
    sqlite3_close(db);
    sqlite3_close(accountDb);
#endif
    
    return 0;
}
