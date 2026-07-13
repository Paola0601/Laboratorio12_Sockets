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

void *atender_hilo_cliente(void *arg) {
    int socket_cliente = *(int *)arg;
    free(arg);

    BloqueArchivo bloque;
    
    while (recv(socket_cliente, &bloque, sizeof(BloqueArchivo), MSG_WAITALL) > 0) {
        pthread_mutex_lock(&cerrojo_archivo);
        
        fseek(archivo_destino, bloque.offset, SEEK_SET);
        fwrite(bloque.datos, 1, bloque.size, archivo_destino);
        fflush(archivo_destino);
        
        pthread_mutex_unlock(&cerrojo_archivo);
    }

    close(socket_cliente);
    return NULL;
}

int main() {
    int socket_servidor;
    struct sockaddr_in direccion_servidor;
    struct sockaddr_in direccion_cliente;
    socklen_t tamano_cliente = sizeof(direccion_cliente);

    archivo_destino = fopen("recibidos/archivo_final.bin", "wb");
    if (!archivo_destino) {
        perror("[-] Error: Asegurate de crear la carpeta 'recibidos'");
        return EXIT_FAILURE;
    }

    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor < 0) {
        perror("[-] Error al crear el socket");
        return EXIT_FAILURE;
    }

    int opcion = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion));

    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(PUERTO);

    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        perror("[-] Error en bind");
        close(socket_servidor);
        return EXIT_FAILURE;
    }

    if (listen(socket_servidor, 20) < 0) {
        perror("[-] Error en listen");
        close(socket_servidor);
        return EXIT_FAILURE;
    }

    printf("[+] Servidor paralelo activo en el puerto %d...\n", PUERTO);

    while (1) {
        int cliente_fd = accept(socket_servidor, (struct sockaddr *)&direccion_cliente, &tamano_cliente);
        if (cliente_fd < 0) continue;

        int *socket_hilo = malloc(sizeof(int));
        *socket_hilo = cliente_fd;

        pthread_t id_hilo;
        if (pthread_create(&id_hilo, NULL, atender_hilo_cliente, socket_hilo) == 0) {
            pthread_detach(id_hilo);
        } else {
            close(cliente_fd);
            free(socket_hilo);
        }
    }

    fclose(archivo_destino);
    close(socket_servidor);
    pthread_mutex_destroy(&cerrojo_archivo);
    return EXIT_SUCCESS;
}
