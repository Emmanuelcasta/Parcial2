#ifndef UTILS_H
#define UTILS_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <netinet/in.h> 
#include "protocol.h"

// Estructura para almacenar un cliente en espera.
typedef struct waiting_client {
    int sock;
    int client_id;
    int game_id;
    char username[50];
    struct waiting_client *next;
} waiting_client_t;

typedef struct room {
    int game_id;
    struct room *next;
} rooms;

// Función auxiliar para agregar un cliente a la lista de espera.
void add_waiting_client(waiting_client_t **list, waiting_client_t *client);

// Función auxiliar para buscar y remover un cliente de la lista de espera con el mismo game_id.
waiting_client_t* pop_waiting_client(waiting_client_t **list, int game_id);

// Función auxiliar para agregar una sala como ocupada
void add_room(rooms **list, rooms *room);

//Función auxiliar para buscar y/o remover una sala de la lista de salas ocupadas
rooms* search_room(rooms **list, int game_id, bool remove);

//Inicialización del socket del servidor
int setup_server_socket(FILE *log_file);

// Manejo de la conexión de un cliente (ruta legacy del juego: NO usada en chat)
void handle_client_connection(int client_sock, int client_id, struct sockaddr_in cli_addr,
                              waiting_client_t **waiting_list, rooms **rooms_list, FILE *log_file);

// Enviar ACK al cliente (legacy del juego)
void send_login_ack(int client_sock, ProtocolMessage *ackMsg, FILE *log_file);

// Crear una nueva sesión entre dos clientes (legacy del juego)
void create_session(waiting_client_t *waiting, int client_sock, int client_id, int game_id,
                    const char *username, rooms **rooms_list, FILE *log_file);

// >>> Declaración para logs (implementada en utils.c)
const char* get_timestamp(void);

#endif 
