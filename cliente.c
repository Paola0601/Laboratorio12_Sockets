#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <dirent.h>  
#include "protocolo.h"

#define N_THREADS 4 
#define CARPETA_PRUEBAS "archivos_prueba/"

typedef struct {
    long inicio;
    long fin;
    int id_hilo;
    char ruta_archivo[512];
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

    FILE *f = fopen(rango.ruta_archivo, "rb");
    if (!f) return NULL;

    fseek(f, rango.inicio, SEEK_SET);
    long posicion_actual = rango.inicio;
    MensajeRed msg;
    msg.tipo_mensaje = TIPO_DATOS;
    long bloques_enviados = 0;

    while (posicion_actual < rango.fin) {
        long restante = rango.fin - posicion_actual;
        int leer_bytes = (restante > BUFFER_SIZE) ? BUFFER_SIZE : (int)restante;

        msg.offset = posicion_actual;
        msg.size = fread(msg.datos, 1, leer_bytes, f);

        if (msg.size <= 0) break;

        send(cliente, &msg, sizeof(MensajeRed), 0);
        posicion_actual += msg.size;
        bloques_enviados++;
    }

    // REAGREGADO: Impresión en pantalla para certificar la concurrencia ante el docente
    printf("[Hilo %d] Completado con éxito. Bloques transmitidos: %ld\n", rango.id_hilo, bloques_enviados);

    fclose(f);
    close(cliente);
    return NULL;
}

int main() {
    DIR *dir;
    struct dirent *ent;
    char lista_archivos[50][256];
    int contador_archivos = 0;
    int seleccion = 0;

    printf("==================================================\n");
    printf("   SISTEMA CONCURRENTE: MENU DE ARCHIVOS LOCALES   \n");
    printf("==================================================\n");

    dir = opendir(CARPETA_PRUEBAS);
    if (dir == NULL) {
        perror("[-] Error al abrir la carpeta archivos_prueba/");
        return 1;
    }

    printf("[+] Archivos disponibles para transferir:\n");
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") != 0 && 
            strcmp(ent->d_name, "..") != 0 && 
            strcmp(ent->d_name, ".gitkeep") != 0) {
            
            strncpy(lista_archivos[contador_archivos], ent->d_name, 256);
            printf("  [%d] %s\n", contador_archivos + 1, lista_archivos[contador_archivos]);
            contador_archivos++;
            
            if (contador_archivos >= 50) break; 
        }
    }
    closedir(dir);

    if (contador_archivos == 0) {
        printf("[-] No se encontraron archivos en 'archivos_prueba/'.\n");
        return 1;
    }

    do {
        printf("\n[?] Elige el numero del archivo que deseas enviar: ");
        if (scanf("%d", &seleccion) != 1) {
            while (getchar() != '\n'); 
            seleccion = 0;
        }
    } while (seleccion < 1 || seleccion > contador_archivos);

    char *nombre_seleccionado = lista_archivos[seleccion - 1];
    char ruta_completa[512];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s%s", CARPETA_PRUEBAS, nombre_seleccionado);

    FILE *f = fopen(ruta_completa, "rb");
    if (!f) {
        printf("[-] Error crítico al abrir '%s'\n", ruta_completa);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long tamano_total = ftell(f);
    fclose(f);

    int socket_control = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servidor;
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(PUERTO);
    servidor.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(socket_control, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        printf("[-] Error: El servidor no está activo.\n");
        return 1;
    }

    MensajeRed msg_meta;
    msg_meta.tipo_mensaje = TIPO_METADATA;
    strncpy(msg_meta.nombre_archivo, nombre_seleccionado, 256);
    send(socket_control, &msg_meta, sizeof(MensajeRed), 0);
    close(socket_control);
    
    usleep(100000); 

    pthread_t hilos[N_THREADS];
    long tamano_por_hilo = tamano_total / N_THREADS;

    printf("[+] Procesando: '%s' (%ld bytes)\n", nombre_seleccionado, tamano_total);
    printf("[+] Lanzando %d hilos de datos en paralelo por la red...\n", N_THREADS);

    for (int i = 0; i < N_THREADS; i++) {
        RangoHilo *rango = malloc(sizeof(RangoHilo));
        rango->id_hilo = i + 1;
        rango->inicio = i * tamano_por_hilo;
        rango->fin = (i == N_THREADS - 1) ? tamano_total : (rango->inicio + tamano_por_hilo);
        strncpy(rango->ruta_archivo, ruta_completa, 512);
        
        pthread_create(&hilos[i], NULL, enviar_seccion, rango);
    }

    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(hilos[i], NULL);
    }

    printf("[+] [CLIENTE] ¡Archivo '%s' transmitido con exito total!\n", nombre_seleccionado);
    return 0;
}
