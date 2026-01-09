// protocol.h

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdlib.h>

#define datetimeLen (sizeof("YYYY-MM-DD HH:MM:SS") - 1)

typedef enum
{
    COMMAND_NULL,
    
    COMMAND_GET_CHATS,
    COMMAND_GET_MESSAGES,
    COMMAND_GET_USER,
    
    COMMAND_CHAT_CREATE,
    COMMAND_CHAT_JOIN,
    COMMAND_CHAT_ADD,
    COMMAND_CHAT_MESSAGE,
    
    COMMAND_CALL_MEMBERS,
    COMMAND_CALL_JOIN,
    COMMAND_CALL_LEAVE,
    COMMAND_CALL_AUDIO,
} Command;

typedef enum
{
    MESSAGE_TEXT,
    // TODO
} MessageType;

size_t serializeLen(const char *format, ...);

char *serializeInto(char *data, const char *format, ...);

char *serializeNew(const char *format, ...);

char *deserialize(char *data, size_t maxLen, const char *format, ...);

#endif
