#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#define MAX_SALAS 10
#define MAX_USUARIOS_POR_SALA 20
#define MAX_TEXTO 256
#define MAX_NOMBRE 50

struct mensaje {
    long mtype;               // 1=JOIN, 3=MSG, 2=RESPUESTA
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
    int cola_sala;            // ID de la cola de la sala (para JOIN)
};

struct sala {
    char nombre[MAX_NOMBRE];
    int cola_id;
    int num_usuarios;
    char usuarios[MAX_USUARIOS_POR_SALA][MAX_NOMBRE];
};

static struct sala salas[MAX_SALAS];
static int num_salas = 0;

// Crear sala
static int crear_sala(const char *nombre) {
    if (num_salas >= MAX_SALAS) return -1;

    key_t key = ftok("/tmp", (int)('B' + num_salas)); // clave estable
    int cola_id = msgget(key, IPC_CREAT | 0666);
    if (cola_id == -1) {
        perror("Error al crear la cola de la sala");
        return -1;
    }

    strcpy(salas[num_salas].nombre, nombre);
    salas[num_salas].cola_id = cola_id;
    salas[num_salas].num_usuarios = 0;
    num_salas++;
    return num_salas - 1;
}

static int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++)
        if (strcmp(salas[i].nombre, nombre) == 0) return i;
    return -1;
}

static int agregar_usuario_a_sala(int indice, const char *usuario) {
    if (indice < 0 || indice >= num_salas) return -1;
    struct sala *s = &salas[indice];

    for (int i = 0; i < s->num_usuarios; i++)
        if (strcmp(s->usuarios[i], usuario) == 0) return 0; // ya estaba

    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) return -1;

    strcpy(s->usuarios[s->num_usuarios], usuario);
    s->num_usuarios++;
    return 0;
}

static void enviar_a_todos_en_sala(int indice, struct mensaje *msg) {
    if (indice < 0 || indice >= num_salas) return;
    struct sala *s = &salas[indice];

    // Reenvía a la cola de la sala; los clientes conectados a esa sala la están
    // leyendo. Se filtra por remitente en el cliente (o aquí si quieres).
    if (msgsnd(s->cola_id, msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
        perror("Error al enviar mensaje a la sala");
    }
}

int main(void) {
    key_t key_global = ftok("/tmp", 'A');
    int cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) { perror("Error cola global"); exit(1); }

    printf("Servidor de chat iniciado. Esperando clientes...\n");

    struct mensaje msg;
    while (1) {
        if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
            perror("Error al recibir mensaje");
            continue;
        }

        if (msg.mtype == 1) { // JOIN
            printf("Solicitud JOIN: %s a %s\n", msg.remitente, msg.sala);
            int idx = buscar_sala(msg.sala);
            if (idx == -1) idx = crear_sala(msg.sala);

            if (idx != -1 && agregar_usuario_a_sala(idx, msg.remitente) == 0) {
                struct mensaje resp = {0};
                resp.mtype = 2; // RESPUESTA
                strcpy(resp.remitente, msg.remitente);
                strcpy(resp.sala, msg.sala);
                resp.cola_sala = salas[idx].cola_id;
                snprintf(resp.texto, sizeof(resp.texto), "Te has unido a la sala: %s", msg.sala);

                if (msgsnd(cola_global, &resp, sizeof(resp) - sizeof(long), 0) == -1) {
                    perror("Error confirmación JOIN");
                } else {
                    printf("Usuario %s agregado a la sala %s\n", msg.remitente, msg.sala);
                }
            }
        } else if (msg.mtype == 3) { // MSG
            int idx = buscar_sala(msg.sala);
            if (idx != -1) {
                printf("[%s] %s: %s\n", msg.sala, msg.remitente, msg.texto);
                enviar_a_todos_en_sala(idx, &msg);
            }
        }
    }
    return 0;
}
