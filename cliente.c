#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_TEXTO 256
#define MAX_NOMBRE 50

struct mensaje {
    long mtype;               // 1=JOIN, 3=MSG, 2=RESPUESTA
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
    int cola_sala;            // devuelto por el servidor en JOIN
};

static int cola_global = -1;
static int cola_sala = -1;
static char nombre_usuario[MAX_NOMBRE];
static char sala_actual[MAX_NOMBRE] = "";
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv  = PTHREAD_COND_INITIALIZER;

static void *receptor(void *arg) {
    (void)arg;
    struct mensaje msg;

    while (1) {
        // Esperar hasta que haya una sala activa
        pthread_mutex_lock(&mtx);
        while (cola_sala == -1) pthread_cond_wait(&cv, &mtx);
        int cola_actual = cola_sala;
        pthread_mutex_unlock(&mtx);

        // Escuchar bloqueante en la cola de la sala actual
        if (msgrcv(cola_actual, &msg, sizeof(msg) - sizeof(long), 0, 0) != -1) {
            // Evitar eco del propio mensaje si así se desea
            if (strcmp(msg.remitente, nombre_usuario) != 0 || 1) {
                printf("%s: %s\n", msg.remitente, msg.texto);
                fflush(stdout);
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) { printf("Uso: %s <nombre>\n", argv[0]); exit(1); }
    strncpy(nombre_usuario, argv[1], sizeof(nombre_usuario)-1);

    key_t key_global = ftok("/tmp", 'A');
    cola_global = msgget(key_global, 0666);
    if (cola_global == -1) { perror("Error conectar cola global"); exit(1); }

    printf("Bienvenido %s. Salas: General, Deportes\n", nombre_usuario);

    pthread_t hilo;
    if (pthread_create(&hilo, NULL, receptor, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    struct mensaje msg;
    char input[MAX_TEXTO];

    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "leave") == 0) {
            pthread_mutex_lock(&mtx);
            if (cola_sala != -1) {
                printf("Saliendo de la sala %s\n", sala_actual);
                sala_actual[0] = '\0';
                cola_sala = -1;
            }
            pthread_mutex_unlock(&mtx);
            continue;
        }

        if (strncmp(input, "join ", 5) == 0) {
            char sala[MAX_NOMBRE] = {0};
            sscanf(input, "join %49s", sala);

            // Enviar JOIN a la cola GLOBAL
            memset(&msg, 0, sizeof msg);
            msg.mtype = 1;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala);

            if (msgsnd(cola_global, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                perror("Error JOIN");
                continue;
            }

            // Esperar respuesta (mtype=2) y que el remitente coincida
            while (1) {
                if (msgrcv(cola_global, &msg, sizeof(msg) - sizeof(long), 2, 0) == -1) {
                    perror("msgrcv JOIN resp");
                    break;
                }
                if (strcmp(msg.remitente, nombre_usuario) == 0) {
                    printf("%s\n", msg.texto);
                    pthread_mutex_lock(&mtx);
                    cola_sala = msg.cola_sala;
                    strncpy(sala_actual, sala, sizeof(sala_actual)-1);
                    pthread_cond_broadcast(&cv);
                    pthread_mutex_unlock(&mtx);
                    break;
                }
                // Si no es para mí (otro cliente), seguir esperando
            }
            continue;
        }

        if (strlen(input) > 0) {
            if (sala_actual[0] == '\0') {
                printf("Únete a una sala con 'join <sala>'\n");
                continue;
            }
            // Enviar mensaje a la COLA GLOBAL (el servidor lo reenvía a la sala)
            memset(&msg, 0, sizeof msg);
            msg.mtype = 3;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strncpy(msg.texto, input, sizeof(msg.texto)-1);

            if (msgsnd(cola_global, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                perror("Error enviar MSG");
            }
        }
    }

    return 0;
}
