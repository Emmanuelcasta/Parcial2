#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/select.h>
#include <time.h>              // ← añadido
#include "session_handler.h"
#include "protocol.h"
#include "game_manager.h"
#include "utils.h"

#define PORT 8080
#define MAX 1024

// ===== Timestamp para logs (usado por session_handler/server_main) =====
const char* get_timestamp(void) {
    static char buf[32];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// Función auxiliar para agregar un cliente a la lista de espera.
void add_waiting_client(waiting_client_t **list, waiting_client_t *client) {
    client->next = *list;
    *list = client;
}

// Función auxiliar para buscar y remover un cliente de la lista de espera con el mismo game_id.
waiting_client_t* pop_waiting_client(waiting_client_t **list, int game_id) {
    waiting_client_t *prev = NULL, *curr = *list;
    while (curr != NULL) {
        if (curr->game_id == game_id) {
            if (prev == NULL) {
                *list = curr->next;
            } else {
                prev->next = curr->next;
            }
            curr->next = NULL;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

// Función auxiliar para agregar una sala como ocupada
void add_room(rooms **list, rooms *room){
    room->next = *list;
    *list = room;
}

//Función auxiliar para buscar y/o remover una sala de la lista de salas ocupadas
rooms* search_room(rooms **list, int game_id, bool remove) {
    rooms *prev = NULL, *curr = *list;
    while (curr != NULL) {
        if (curr->game_id == game_id) {
            if(remove){
                if (prev == NULL) {
                    *list = curr->next;
                } else {
                    prev->next = curr->next;
                }
                curr->next = NULL;
            }
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

// Inicialización del socket del servidor
int setup_server_socket(FILE *log_file) {
    char message[MAX];
    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        snprintf(message, sizeof(message), "Error al crear socket");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error al crear socket"); 
        exit(EXIT_FAILURE);
    }
    snprintf(message, sizeof(message), "Socket del servidor creado.\n");
    fprintf(log_file, "%s\n", message);
    fflush(log_file);
    printf("%s", message);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        snprintf(message, sizeof(message), "Error en bind");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error en bind");
        exit(EXIT_FAILURE);
    }

    snprintf(message, sizeof(message), "Socket enlazado a puerto %d.\n", PORT);
    fprintf(log_file, "%s\n", message);
    fflush(log_file);
    printf("%s", message);

    if (listen(sockfd, 5) < 0) {
        snprintf(message, sizeof(message), "Error en listen");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error en listen");
        exit(EXIT_FAILURE);
    }
    snprintf(message, sizeof(message), "Server escuchando...\n");
    fprintf(log_file, "%s\n", message);
    fflush(log_file);
    printf("%s", message);
    return sockfd;
}

// ===== Rutas legacy del juego (NO usadas por el chat) =====

// Manejo de la conexión de un cliente (legacy)
void handle_client_connection(int client_sock, int client_id, struct sockaddr_in cli_addr,
                              waiting_client_t **waiting_list, rooms **rooms_list, FILE *log_file) {
    char message[MAX];
    char buff[MAX];
    ProtocolMessage msg;

    // Recibir mensaje de login del cliente.
    memset(buff, 0, MAX);
    int n = read(client_sock, buff, MAX);
    if (n <= 0) {
        snprintf(message, sizeof(message), "Error al leer login del cliente");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error al leer login del cliente"); 
        close(client_sock);
        return;
    }

    if (!parse_message(buff, &msg) || msg.type != MSG_LOGIN) {
        snprintf(message, sizeof(message), "Cliente %d: Login mal formado o tipo incorrecto.\n", client_id);
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        close(client_sock);
        return;
    }

    int game_id = msg.game_id;
    char username[50];
    strncpy(username, msg.data, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';

    int turn;

    rooms *room = search_room(rooms_list, game_id, false);
    bool empty_room = true;
    if(room!=NULL){
        empty_room = false;
    }

    waiting_client_t *waiting = pop_waiting_client(waiting_list, game_id);

    if (empty_room) {
        if (waiting == NULL) {
            turn = 1;

            snprintf(message, sizeof(message), "Cliente %d autenticado como %s en partida %d. (Turno: %d)\n", client_id, username, game_id, turn);
            fprintf(log_file, "%s\n", message);
            fflush(log_file);
            printf("%s", message);
            
            waiting_client_t *new_waiting = malloc(sizeof(waiting_client_t));
            if (!new_waiting) {
                snprintf(message, sizeof(message), "Error al asignar memoria para cliente en espera");
                fprintf(log_file, "%s\n", message);
                fflush(log_file);
                perror("Error al asignar memoria para cliente en espera");
                close(client_sock);
                return;
            }

            new_waiting->sock = client_sock;
            new_waiting->client_id = client_id;
            new_waiting->game_id = game_id;
            strncpy(new_waiting->username, username, sizeof(new_waiting->username) - 1);
            new_waiting->username[sizeof(new_waiting->username) - 1] = '\0';
            new_waiting->next = NULL;
            add_waiting_client(waiting_list, new_waiting);

            snprintf(message, sizeof(message), "Cliente %d en espera para partida %d.\n", client_id, game_id);
            fprintf(log_file, "%s\n", message);
            fflush(log_file);
            printf("%s", message);
        } else {
            turn = 0;
            
            snprintf(message, sizeof(message), "Cliente %d autenticado como %s en partida %d. (Turno: %d)\n", client_id, username, game_id, turn);
            fprintf(log_file, "%s\n", message);
            fflush(log_file);
            printf("%s", message);

            create_session(waiting, client_sock, client_id, game_id, username, rooms_list, log_file);
        }
    } else {
        turn = -1;
        ProtocolMessage ackMsg;
        ackMsg.type = MSG_LOGGED;
        ackMsg.game_id = game_id;
        char temp_data[150];
        snprintf(temp_data, sizeof(temp_data), "NO|%d", turn);
        strncpy(ackMsg.data, temp_data, sizeof(ackMsg.data)-1);
        ackMsg.data[sizeof(ackMsg.data)-1] = '\0';
        send_login_ack(client_sock, &ackMsg, log_file);

        snprintf(message, sizeof(message), 
                "Cliente %d (%s) no fue asignado a ninguna partida. El game_id %d ya está en uso.\n", client_id, username, game_id);
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        printf("%s", message);
    }
}

// Enviar ACK al cliente (legacy)
void send_login_ack(int client_sock, ProtocolMessage *ackMsg, FILE *log_file) {
    char message[MAX];
    char formatted_ack[MAX];
    format_message(*ackMsg, formatted_ack, MAX);
    snprintf(message, sizeof(message), "%s", formatted_ack);
    fprintf(log_file, "%s\n", message);
    fflush(log_file);
    write(client_sock, formatted_ack, strlen(formatted_ack));
}

// Crear una nueva sesión entre dos clientes (legacy)
void create_session(waiting_client_t *waiting, int client_sock, int client_id, int game_id,
                    const char *username, rooms **rooms_list, FILE *log_file) {
    
    char message[MAX];
    int client_sock1 = waiting->sock;
    int client_id1 = waiting->client_id;
    char username1[50];
    strncpy(username1, waiting->username, sizeof(username1));
    username1[sizeof(username1) - 1] = '\0';
    free(waiting);

    int client_sock2 = client_sock;
    int client_id2 = client_id;
    char username2[50];
    strncpy(username2, username, sizeof(username2));
    username2[sizeof(username2) - 1] = '\0';
    
    rooms *new_room = malloc(sizeof(rooms));
    if (!new_room) {
        snprintf(message, sizeof(message), "Error al asignar memoria para la sala");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error al asignar memoria para la sala");
        return;
    }

    new_room->game_id = game_id;
    new_room->next = NULL;
    add_room(rooms_list, new_room);

    session_pair_t *session = malloc(sizeof(session_pair_t));
    if (session == NULL) {
        snprintf(message, sizeof(message), "Error al asignar memoria para la sesión");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error al asignar memoria para la sesión");
        close(client_sock1);
        close(client_sock2);
        search_room(rooms_list, game_id, true);
        return;
    }

    session->gm = malloc(sizeof(GameManager));
    if (!session->gm) {
        snprintf(message, sizeof(message), "Error al asignar memoria para el GameManager");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error al asignar memoria para el GameManager");
        close(client_sock1);
        close(client_sock2);
        free(session);
        return;
    }
    session->gm->states = malloc(2 * sizeof(GameState));
    if (!session->gm->states) {
        snprintf(message, sizeof(message), "Error al asignar memoria para estados del juego");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error al asignar memoria para estados del juego");
        close(client_sock1);
        close(client_sock2);
        free(session->gm);
        free(session);
        return;
    }

    session->client_sock1 = client_sock1;
    session->client_sock2 = client_sock2;
    session->client_id1 = client_id1;
    session->client_id2 = client_id2;
    session->game_id = game_id;
    strncpy(session->username1, username1, sizeof(session->username1) - 1);
    session->username1[sizeof(session->username1) - 1] = '\0';
    strncpy(session->username2, username2, sizeof(session->username2) - 1);
    session->username2[sizeof(session->username2) - 1] = '\0';

    session->room = rooms_list;
    session->log_file = log_file;

    pthread_t tid;
    if (pthread_create(&tid, NULL, session_handler, (void *)session) != 0) {
        snprintf(message, sizeof(message), "Error al crear el hilo de sesión");
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
        perror("Error al crear el hilo de sesión");
        free(session);
        close(client_sock1);
        close(client_sock2);
        search_room(rooms_list, game_id, true);
        return;
    }

    pthread_detach(tid);
    snprintf(message, sizeof(message), 
            "Sesión creada entre Cliente %d (%s) y Cliente %d (%s) en partida %d.\n", client_id1, username1, client_id2, username2, game_id);
    fprintf(log_file, "%s\n", message);
    fflush(log_file);
    printf("%s", message);
}
