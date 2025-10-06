#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "protocol.h"
#include "session_handler.h"

#define MAX_LINE 1024

static int read_line(int fd, char *buf, size_t sz) {
    size_t off = 0;
    while (off < sz - 1) {
        char c;
        int n = recv(fd, &c, 1, 0);
        if (n <= 0) return n;
        buf[off++] = c;
        if (c == '\n') break;
    }
    buf[off] = '\0';
    return (int)off;
}

int main(int argc, char *argv[]) {
    const char *log_path = "server.log";
    if (argc > 3) log_path = argv[3];

    fclose(fopen(log_path, "w"));
    FILE *log_file = fopen(log_path, "a");
    if (!log_file) {
        perror("No se pudo abrir log_file");
        exit(EXIT_FAILURE);
    }

    // OJO: utils.h expone setup_server_socket(FILE *log_file)
    int sockfd = setup_server_socket(log_file);
    if (sockfd < 0) {
        fprintf(stderr, "Fallo al iniciar socket servidor\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    rooms_init();

    fprintf(log_file, "%s \"Servidor de chat iniciado\" - SERVER\n", get_timestamp());
    fflush(log_file);
    printf("%s Servidor de chat iniciado\n", get_timestamp());

    while (1) {
        int client_sock = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);
        if (client_sock < 0) {
            perror("Error en accept");
            continue;
        }

        // Esperamos una línea: LOGIN|room|username\n  (JOIN opcional)
        char line[MAX_LINE] = {0};
        if (read_line(client_sock, line, sizeof(line)) <= 0) { close(client_sock); continue; }

        ProtocolMessage m;
        int room_id = -1;
        char username[64] = "anon";

        if (parse_message(line, &m)) {
            if (m.type == MSG_LOGIN || m.type == MSG_JOIN) {
                room_id = m.game_id;
                if (m.data[0]) strncpy(username, m.data, sizeof(username)-1);
            }
        }

        // Intentar leer otra línea (JOIN/LOGIN adicional) sin bloquear
        char line2[MAX_LINE] = {0};
        int n2 = recv(client_sock, line2, sizeof(line2)-1, MSG_DONTWAIT);
        if (n2 > 0) {
            char *nl = memchr(line2, '\n', n2);
            if (nl) {
                *nl = '\0';
                ProtocolMessage m2;
                if (parse_message(line2, &m2)) {
                    if (m2.type == MSG_LOGIN || m2.type == MSG_JOIN) {
                        room_id = m2.game_id;
                        if (m2.data[0]) strncpy(username, m2.data, sizeof(username)-1);
                    }
                }
            }
        }

        if (room_id < 0) {
            const char *err = "ERROR|0|Formato LOGIN/JOIN inválido\n";
            write(client_sock, err, strlen(err));
            close(client_sock);
            continue;
        }

        add_client_to_room(client_sock, room_id, username, log_file);
    }

    fclose(log_file);
    close(sockfd);
    return 0;
}
