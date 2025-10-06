#include "session_handler.h"
#include "protocol.h"
#include "utils.h"      // get_timestamp, etc.

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

/* ====== Infra de salas (chat multiusuario) ====== */
static room_t *g_rooms = NULL;
static pthread_mutex_t g_rooms_mtx = PTHREAD_MUTEX_INITIALIZER;

static void format_and_send(int fd, MessageType t, int room, const char* data) {
    ProtocolMessage m = { .type = t, .game_id = room };
    if (data) {
        strncpy(m.data, data, sizeof(m.data)-1);
        m.data[sizeof(m.data)-1] = '\0';
    } else {
        m.data[0] = '\0';
    }
    char out[MAX_LINE];
    format_message(m, out, sizeof(out));
    size_t len = strlen(out);
    if (len < sizeof(out) - 1) { out[len] = '\n'; out[len+1] = '\0'; }
    write(fd, out, strlen(out));
}

static room_t* find_room_nolock(int room_id) {
    for (room_t *r = g_rooms; r; r = r->next)
        if (r->room_id == room_id) return r;
    return NULL;
}

void rooms_init(void) {
    pthread_mutex_lock(&g_rooms_mtx);
    g_rooms = NULL;
    pthread_mutex_unlock(&g_rooms_mtx);
}

static void remove_client_at(room_t *r, int idx) {
    if (idx < 0 || idx >= r->num_clients) return;
    int fd = r->clients[idx].fd;
    if (fd >= 0) close(fd);
    for (int i = idx; i < r->num_clients - 1; ++i)
        r->clients[i] = r->clients[i+1];
    r->num_clients--;
}

void try_close_room(room_t *r) {
    if (r->num_clients > 0) return;

    pthread_mutex_lock(&g_rooms_mtx);
    room_t **pp = &g_rooms;
    while (*pp && *pp != r) pp = &((*pp)->next);
    if (*pp == r) *pp = r->next;
    pthread_mutex_unlock(&g_rooms_mtx);

    pthread_mutex_destroy(&r->mtx);
    free(r);
}

static void broadcast(room_t *r, int from_fd, const char *payload) {
    for (int i = 0; i < r->num_clients; ++i) {
        int fd = r->clients[i].fd;
        if (fd == from_fd) continue;
        format_and_send(fd, MSG_FROM, r->room_id, payload);
    }
}

static void process_line(room_t *r, int idx, const char *line) {
    client_info_t *cli = &r->clients[idx];
    FILE *logf = r->log_file;

    if (logf) { fprintf(logf, "%s \"%s\" - %s\n", get_timestamp(), line, cli->username); fflush(logf); }

    ProtocolMessage msg;
    if (!parse_message(line, &msg)) return;

    if (msg.type == MSG_LEAVE || msg.type == MSG_FF) {
        char sysmsg[256]; snprintf(sysmsg, sizeof(sysmsg), "%s salió del chat.", cli->username);
        for (int j = 0; j < r->num_clients; ++j) {
            if (j == idx) continue;
            format_and_send(r->clients[j].fd, MSG_SYS, r->room_id, sysmsg);
        }
        remove_client_at(r, idx);
        return;
    } else if (msg.type == MSG_MSG) {
        char payload[256];
        snprintf(payload, sizeof(payload), "%s: %s", cli->username, msg.data);
        broadcast(r, cli->fd, payload);
    }
}

static void* room_thread(void *arg) {
    room_t *r = (room_t*)arg;

    pthread_mutex_lock(&r->mtx);
    if (r->log_file) {
        fprintf(r->log_file, "%s \"Sala %d creada\" - SERVER\n", get_timestamp(), r->room_id);
        fflush(r->log_file);
    }
    pthread_mutex_unlock(&r->mtx);

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        pthread_mutex_lock(&r->mtx);
        for (int i = 0; i < r->num_clients; ++i) {
            int fd = r->clients[i].fd;
            FD_SET(fd, &rfds);
            if (fd > maxfd) maxfd = fd;
        }
        FILE *logf = r->log_file;
        pthread_mutex_unlock(&r->mtx);

        struct timeval tmo = { .tv_sec = 1, .tv_usec = 0 };
        int sel = select(maxfd + 1, &rfds, NULL, NULL, &tmo);
        if (sel < 0) {
            if (errno == EINTR) continue;
            pthread_mutex_lock(&r->mtx);
            if (logf) { fprintf(logf, "%s \"select error en sala %d\" - SERVER\n", get_timestamp(), r->room_id); fflush(logf); }
            for (int i = r->num_clients - 1; i >= 0; --i) remove_client_at(r, i);
            try_close_room(r);
            pthread_mutex_unlock(&r->mtx);
            return NULL;
        }
        if (sel == 0) {
            pthread_mutex_lock(&r->mtx);
            if (r->num_clients == 0) {
                if (logf) { fprintf(logf, "%s \"Sala %d vacía -> cerrando\" - SERVER\n", get_timestamp(), r->room_id); fflush(logf); }
                try_close_room(r);
                pthread_mutex_unlock(&r->mtx);
                return NULL;
            }
            pthread_mutex_unlock(&r->mtx);
            continue;
        }

        pthread_mutex_lock(&r->mtx);
        for (int i = 0; i < r->num_clients; /* ++i dentro */) {
            int fd = r->clients[i].fd;
            if (!FD_ISSET(fd, &rfds)) { i++; continue; }

            char tmp[1024];
            int n = read(fd, tmp, sizeof(tmp));
            if (n <= 0) {
                if (logf) { fprintf(logf, "%s \"LEAVE por desconexión: %s (sala %d)\" - SERVER\n", get_timestamp(), r->clients[i].username, r->room_id); fflush(logf); }
                char sysmsg[256]; snprintf(sysmsg, sizeof(sysmsg), "%s salió del chat.", r->clients[i].username);
                for (int j = 0; j < r->num_clients; ++j) {
                    if (j == i) continue;
                    format_and_send(r->clients[j].fd, MSG_SYS, r->room_id, sysmsg);
                }
                remove_client_at(r, i);
                continue;
            }

            client_info_t *cli = &r->clients[i];
            if (n > INBUF_SIZE - cli->inlen - 1) n = INBUF_SIZE - cli->inlen - 1;
            memcpy(cli->inbuf + cli->inlen, tmp, n);
            cli->inlen += n;
            cli->inbuf[cli->inlen] = '\0';

            char *start = cli->inbuf;
            for (;;) {
                char *nl = strchr(start, '\n');
                if (!nl) break;
                *nl = '\0';
                if (*start) process_line(r, i, start);
                start = nl + 1;
                if (i >= r->num_clients) break; // pudo salir del chat
            }

            if (i < r->num_clients) {
                int rem = (int)(cli->inbuf + cli->inlen - start);
                memmove(cli->inbuf, start, rem);
                cli->inlen = rem;
                cli->inbuf[cli->inlen] = '\0';
                i++;
            }
        }
        pthread_mutex_unlock(&r->mtx);
    }
    return NULL;
}

void add_client_to_room(int client_fd, int room_id, const char* username, FILE *log_file) {
    if (!username) username = "anon";

    pthread_mutex_lock(&g_rooms_mtx);
    room_t *r = find_room_nolock(room_id);
    if (!r) {
        r = (room_t*)calloc(1, sizeof(room_t));
        r->room_id = room_id;
        r->num_clients = 0;
        r->log_file = log_file;
        pthread_mutex_init(&r->mtx, NULL);
        r->next = g_rooms;
        g_rooms = r;
        pthread_create(&r->thread, NULL, room_thread, r);
        pthread_detach(r->thread);
    }
    pthread_mutex_unlock(&g_rooms_mtx);

    pthread_mutex_lock(&r->mtx);
    if (r->num_clients >= MAX_CLIENTS_PER_ROOM) {
        const char *full = "SYS|0|Sala llena\n";
        write(client_fd, full, strlen(full));
        close(client_fd);
        pthread_mutex_unlock(&r->mtx);
        return;
    }

    client_info_t *cli = &r->clients[r->num_clients];
    cli->fd = client_fd;
    strncpy(cli->username, username, MAX_USERNAME_LEN-1);
    cli->username[MAX_USERNAME_LEN-1] = '\0';
    cli->alive = true;
    cli->inlen = 0; cli->inbuf[0] = '\0';
    r->num_clients++;

    ProtocolMessage ack = { .type = MSG_LOGGED, .game_id = room_id };
    snprintf(ack.data, sizeof(ack.data), "Ok|1|");
    char out[MAX_LINE]; format_message(ack, out, sizeof(out));
    size_t len = strlen(out);
    if (len < sizeof(out) - 1) { out[len] = '\n'; out[len+1] = '\0'; }
    write(client_fd, out, strlen(out));

    char sysmsg[256];
    snprintf(sysmsg, sizeof(sysmsg), "%s se unió al chat.", username);
    for (int i = 0; i < r->num_clients - 1; ++i) {
        format_and_send(r->clients[i].fd, MSG_SYS, room_id, sysmsg);
    }
    pthread_mutex_unlock(&r->mtx);
}

/* ====== DUMMY legacy ======
 * utils.c llama pthread_create(..., session_handler, session)
 * pero en modo chat NO usamos esa ruta. Definimos stub para enlazar.
 */
void *session_handler(void *arg) {
    session_pair_t *s = (session_pair_t*)arg;
    if (!s) return NULL;
    if (s->client_sock1 > 0) close(s->client_sock1);
    if (s->client_sock2 > 0) close(s->client_sock2);
    free(s);
    return NULL;
}
