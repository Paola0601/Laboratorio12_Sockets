#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "protocolo.h"

pthread_mutex_t cerrojo_archivo = PTHREAD_MUTEX_INITIALIZER;
FILE *archivo_destino = NULL;
int hilos_terminados = 0;

void registrar_fin_hilo(void) {
    pthread_mutex_lock(&cerrojo_archivo);
    hilos_terminados++;
    printf("Seccion recibida (%d de %d).\n", hilos_terminados, N_THREADS);
    if (hilos_terminados == N_THREADS && archivo_destino != NULL) {
        fclose(archivo_destino);
        archivo_destino = NULL;
        printf("Archivo reconstruido completamente.\n");
    }
    pthread_mutex_unlock(&cerrojo_archivo);
}

int recibir_completo(int socket_fd, void *datos, size_t cantidad) {
    char *p = datos;
    while (cantidad > 0) {
        ssize_t recibidos = recv(socket_fd, p, cantidad, 0);
        if (recibidos <= 0) return -1;
        p += recibidos;
        cantidad -= recibidos;
    }
    return 0;
}

void escribir_bloque(MensajeRed *mensaje) {
    if (mensaje->size < 0 || mensaje->size > BUFFER_SIZE) return;
    pthread_mutex_lock(&cerrojo_archivo);
    if (archivo_destino != NULL) {
        fseek(archivo_destino, mensaje->offset, SEEK_SET);
        fwrite(mensaje->datos, 1, mensaje->size, archivo_destino);
    }
    pthread_mutex_unlock(&cerrojo_archivo);
}

void *atender_cliente(void *arg) {
    int socket_cliente = *(int *)arg;
    MensajeRed mensaje;
    free(arg);
    while (recibir_completo(socket_cliente, &mensaje, sizeof(mensaje)) == 0) {
        if (mensaje.tipo_mensaje == TIPO_DATOS) {
            escribir_bloque(&mensaje);
        } else if (mensaje.tipo_mensaje == TIPO_FIN_HILO) {
            registrar_fin_hilo();
            break;
        }
    }
    close(socket_cliente);
    return NULL;
}

int main(void) {
    int socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servidor = {0}, cliente;
    socklen_t tamano_cliente = sizeof(cliente);
    if (socket_servidor < 0) return 1;

    int opcion = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion));
    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY;
    servidor.sin_port = htons(PUERTO);
    if (bind(socket_servidor, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        perror("Error en bind"); close(socket_servidor); return 1;
    }
    if (listen(socket_servidor, 20) < 0) return 1;
    printf("Servidor activo en el puerto %d.\n", PUERTO);

    while (1) {
        int cliente_fd = accept(socket_servidor, (struct sockaddr *)&cliente, &tamano_cliente);
        if (cliente_fd < 0) continue;
        MensajeRed primero;
        if (recibir_completo(cliente_fd, &primero, sizeof(primero)) < 0) {
            close(cliente_fd); continue;
        }

        if (primero.tipo_mensaje == TIPO_METADATA) {
            const char *nombre = strrchr(primero.nombre_archivo, '/');
            nombre = nombre == NULL ? primero.nombre_archivo : nombre + 1;
            pthread_mutex_lock(&cerrojo_archivo);
            if (archivo_destino != NULL) fclose(archivo_destino);
            char ruta[512];
            snprintf(ruta, sizeof(ruta), "recibidos/%s", nombre);
            archivo_destino = fopen(ruta, "wb");
            hilos_terminados = 0;
            if (archivo_destino != NULL && primero.tamano_archivo > 0) {
                fseek(archivo_destino, primero.tamano_archivo - 1, SEEK_SET);
                fputc('\0', archivo_destino);
                fflush(archivo_destino);
            }
            printf("Reconstruyendo: %s (%ld bytes)\n", ruta, primero.tamano_archivo);
            pthread_mutex_unlock(&cerrojo_archivo);
            close(cliente_fd);
            continue;
        }

        if (primero.tipo_mensaje == TIPO_DATOS) {
            escribir_bloque(&primero);
        } else if (primero.tipo_mensaje == TIPO_FIN_HILO) {
            registrar_fin_hilo();
            close(cliente_fd);
            continue;
        }
        int *socket_hilo = malloc(sizeof(int));
        pthread_t hilo;
        if (socket_hilo == NULL) { close(cliente_fd); continue; }
        *socket_hilo = cliente_fd;
        pthread_create(&hilo, NULL, atender_cliente, socket_hilo);
        pthread_detach(hilo);
    }
}
