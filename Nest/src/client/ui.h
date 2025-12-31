// ui.h

#ifndef UI_H
#define UI_H

#include <stdlib.h>

#define AUDIO_SAMPLE_SIZE 320
#define AUDIO_BUFFER_SIZE (AUDIO_SAMPLE_SIZE * 8)

typedef struct
{
    int id;
    char *name;
    char *created;
    char *seen;
} User;

typedef struct
{
    int chatId;
    int userId;
    int adderId;
} Membership;

typedef struct
{
    int id;
    int chatId;
    int senderId;
    int type;
    char *content;
    int contentLen;
    char *sent;
} Message;

typedef struct
{
    int id;
    char *name;
    int ownerId;
    char *created;
    char *joined;
    Membership *memberships;
    size_t memberCount;
    int lastMessageId;
    char *lastMessageContent;
} Chat;

typedef struct
{
    int userId;
    int volume;
    int pressed;
    int writePos;
    int readPos;
    short *buffer;
} CallMember;

typedef struct
{
    int user;
    char *name;
    char *password;
} Account;

void UI_open();

int UI_draw();

void UI_close();

void setHandlers(void (*onStartup)(),
    int (*onSignup)(const char *, const char *), int (*onLogin)(const char *, const char *),
    void (*onAccountOpen)(int), void (*onAccountSave)(int, const char *, const char *),
    void (*onChatCreate)(const char *), void (*onChatJoin)(const char *),
    void (*onMessageSend)(int, int, const char *, size_t), void (*onUserRequest)(int),
    void (*onMessageRequest)(int, int), void (*onCallJoin)(int), void (*onCallLeave)());

void addChat(Chat chat);

void addUser(User user);

void updateUser(User user);

void addMembership(Membership membership);

void addMessage(Message message);

void clearMessages();

void addCallMember(int userId);

void removeCallMember(int userId);

void clearCall();

CallMember *getCallMember(int userId);

CallMember *getCallMembers();

size_t getCallMemberCount();

void addAccount(Account account);

void setStatus(const char *msg, int arg);

int getFrameTime();

int authenticate();

#endif
