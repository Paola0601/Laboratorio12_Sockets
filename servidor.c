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

    MensajeRed mensaje;
    
    while (recv(socket_cliente, &mensaje, sizeof(MensajeRed), MSG_WAITALL) > 0) {
        if (mensaje.tipo_mensaje == TIPO_DATOS) {
            pthread_mutex_lock(&cerrojo_archivo);
            if (archivo_destino != NULL) {
                fseek(archivo_destino, mensaje.offset, SEEK_SET);
                fwrite(mensaje.datos, 1, mensaje.size, archivo_destino);
                fflush(archivo_destino);
            }
            pthread_mutex_unlock(&cerrojo_archivo);
        }
    }

    close(socket_cliente);
    return NULL;
}

int main() {
    int socket_servidor;
    struct sockaddr_in direccion_servidor;
    struct sockaddr_in direccion_cliente;
    socklen_t tamano_cliente = sizeof(direccion_cliente);

    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    int opcion = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion));

    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(PUERTO);

    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        perror("[-] Error en bind");
        return 1;
    }
    listen(socket_servidor, 20);
    printf("[+] Servidor Dinámico Inteligente activo en el puerto %d...\n", PUERTO);

    while (1) {
        int cliente_fd = accept(socket_servidor, (struct sockaddr *)&direccion_cliente, &tamano_cliente);
        if (cliente_fd < 0) continue;

        // Leer el primer mensaje para verificar si trae los metadatos del archivo
        MensajeRed primer_msg;
        if (recv(cliente_fd, &primer_msg, sizeof(MensajeRed), MSG_WAITALL) > 0) {
            if (primer_msg.tipo_mensaje == TIPO_METADATA) {
                pthread_mutex_lock(&cerrojo_archivo);
                if (archivo_destino == NULL) {
                    char ruta_final[512];
                    snprintf(ruta_final, sizeof(ruta_final), "recibidos/%s", primer_msg.nombre_archivo);
                    archivo_destino = fopen(ruta_final, "wb");
                    printf("[+] Reconstruyendo archivo detectado: %s\n", ruta_final);
                }
                pthread_mutex_unlock(&cerrojo_archivo);
            }
            
            // Si el mensaje es de datos o es una subconexión de hilo, procesarla
            int *socket_hilo = malloc(sizeof(int));
            *socket_hilo = cliente_fd;
            
            // Si el primer mensaje ya traía datos en una subconexión, lo forzamos a procesar
            pthread_t id_hilo;
            pthread_create(&id_hilo, NULL, atender_hilo_cliente, socket_hilo);
            pthread_detach(id_hilo);

            // Inyectar manualmente el bloque si ya venía con datos
            if (primer_msg.tipo_mensaje == TIPO_DATOS) {
                pthread_mutex_lock(&cerrojo_archivo);
                fseek(archivo_destino, primer_msg.offset, SEEK_SET);
                fwrite(primer_msg.datos, 1, primer_msg.size, archivo_destino);
                fflush(archivo_destino);
                pthread_mutex_unlock(&cerrojo_archivo);
            }
        }
    }

    return 0;
}
