#ifndef SESSION_HANDLER_H
#define SESSION_HANDLER_H

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include "protocol.h"
#include "game_manager.h"   // usa typedef real de GameManager

/* ========= Compat con código legado (utils.c) ========= */
/* No redefinimos 'rooms'; sólo adelantamos etiqueta para punteros. */
struct room;

/* Estructura legacy usada por utils.c:create_session() */
typedef struct {
    int client_sock1;
    int client_sock2;
    int client_id1;
    int client_id2;
    int game_id;           // ID de partida / sala
    char username1[50];
    char username2[50];
    GameManager *gm;
    struct room **room;    // evita choque con typedef 'rooms' de utils.h
    FILE *log_file;
} session_pair_t;

/* Prototipo legacy para enlazar (stub en .c) */
void *session_handler(void *arg);

/* ========= Chat multi-sala (nuevo) ========= */
#define MAX_CLIENTS_PER_ROOM 64
#define MAX_USERNAME_LEN     50
#define MAX_LINE             1024
#define INBUF_SIZE           4096

typedef struct {
    int fd;
    char username[MAX_USERNAME_LEN];
    bool alive;
    char inbuf[INBUF_SIZE];
    int  inlen;
} client_info_t;

typedef struct room_s {
    int room_id;
    client_info_t clients[MAX_CLIENTS_PER_ROOM];
    int num_clients;

    pthread_t thread;
    pthread_mutex_t mtx;

    FILE *log_file;

    struct room_s *next;
} room_t;

void rooms_init(void);
void add_client_to_room(int client_fd, int room_id, const char* username, FILE *log_file);
void try_close_room(room_t *r);

#endif // SESSION_HANDLER_H
