#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Tipos de mensajes (compat del juego + chat)
typedef enum {
    // Compat (juego)
    MSG_LOGIN,     // Cliente -> servidor (también para chat: LOGIN|room|username)
    MSG_LOGGED,    // Servidor -> cliente (ACK de login)
    MSG_ATTACK,
    MSG_RESULT,
    MSG_UPDATE,
    MSG_END,
    MSG_ERROR,
    MSG_INVALID,
    MSG_TIMEOUT,
    MSG_FF,
    MSG_ACK,

    // Chat
    MSG_JOIN,      // Cliente -> servidor: JOIN|room|username (opcional si ya vino LOGIN)
    MSG_MSG,       // Cliente -> servidor: MSG|room|texto
    MSG_FROM,      // Servidor -> cliente: FROM|room|username: texto
    MSG_LEAVE,     // Cliente -> servidor: LEAVE|room|
    MSG_SYS        // Servidor -> cliente: SYS|room|texto
} MessageType;

// Mensaje simple con framing "COMANDO|game_id|data"
typedef struct {
    MessageType type;
    int game_id;        // room_id
    char data[256];     // username, texto, etc.
} ProtocolMessage;

const char* message_type_to_string(MessageType type);
MessageType string_to_message_type(const char* str);
bool parse_message(const char* input, ProtocolMessage* msg);     // "TYPE|id|data"
void format_message(ProtocolMessage msg, char* output, size_t size);

#endif
