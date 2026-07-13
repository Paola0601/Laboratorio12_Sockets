#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "protocolo.h"

#define N_THREADS 4 
#define ORIGEN "archivos_prueba/prueba.bin"

typedef struct {
    long inicio;
    long fin;
    int id_hilo;
} RangoHilo;

void *enviar_seccion(void *arg) {
    RangoHilo rango = *(RangoHilo *)arg;
    free(arg);

    int cliente = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servidor;
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(PUERTO);
    servidor.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(cliente, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        printf("[-] Hilo %d: Error de conexion al servidor\n", rango.id_hilo);
        return NULL;
    }

    FILE *f = fopen(ORIGEN, "rb");
    if (!f) return NULL;

    fseek(f, rango.inicio, SEEK_SET);
    long posicion_actual = rango.inicio;
    BloqueArchivo bloque;
    long bloques_enviados = 0;

    while (posicion_actual < rango.fin) {
        long restante = rango.fin - posicion_actual;
        int leer_bytes = (restante > BUFFER_SIZE) ? BUFFER_SIZE : (int)restante;

        bloque.offset = posicion_actual;
        bloque.size = fread(bloque.datos, 1, leer_bytes, f);

        if (bloque.size <= 0) break;

        send(cliente, &bloque, sizeof(BloqueArchivo), 0);
        posicion_actual += bloque.size;
        bloques_enviados++;
    }

    printf("[Hilo %d] Completado con éxito. Bloques transmitidos: %ld\n", rango.id_hilo, bloques_enviados);
    fclose(f);
    close(cliente);
    return NULL;
}

int main() {
    FILE *f = fopen(ORIGEN, "rb");
    if (!f) {
        printf("[-] Error: No se encontro el archivo de prueba en 'archivos_prueba/prueba.bin'\n");
        printf("[!] Por favor, ejecuta primero el comando para generar el archivo de prueba.\n");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long tamano_total = ftell(f);
    fclose(f);

    pthread_t hilos[N_THREADS];
    long tamano_por_hilo = tamano_total / N_THREADS;

    printf("[+] Archivo detectado: %ld bytes\n", tamano_total);
    printf("[+] Iniciando transmision concurrente usando %d hilos de red en paralelo...\n", N_THREADS);

    for (int i = 0; i < N_THREADS; i++) {
        RangoHilo *rango = malloc(sizeof(RangoHilo));
        rango->id_hilo = i + 1;
        rango->inicio = i * tamano_por_hilo;
        rango->fin = (i == N_THREADS - 1) ? tamano_total : (rango->inicio + tamano_por_hilo);
        
        pthread_create(&hilos[i], NULL, enviar_seccion, rango);
    }

    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(hilos[i], NULL);
    }

    printf("[+] [CLIENTE] ¡Transferencia paralela concluida con exito total!\n");
    return 0;
}
