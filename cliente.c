#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <dirent.h>
#include "protocolo.h"

#define CARPETA_PRUEBAS "archivos_prueba/"

typedef struct {
    long inicio, fin;
    int id_hilo, correcto;
    char ruta_archivo[512];
} RangoHilo;

int enviar_completo(int socket_fd, const void *datos, size_t cantidad) {
    const char *p = datos;
    while (cantidad > 0) {
        ssize_t enviados = send(socket_fd, p, cantidad, 0);
        if (enviados <= 0) return -1;
        p += enviados;
        cantidad -= enviados;
    }
    return 0;
}

int conectar_servidor(void) {
    int cliente = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servidor = {0};
    if (cliente < 0) return -1;
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(PUERTO);
    servidor.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(cliente, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        close(cliente);
        return -1;
    }
    return cliente;
}

void *enviar_seccion(void *arg) {
    RangoHilo *rango = arg;
    int cliente = conectar_servidor();
    FILE *archivo;
    MensajeRed mensaje = {0};
    long posicion, bloques = 0;

    rango->correcto = 0;
    if (cliente < 0) {
        printf("[-] Hilo %d: error de conexion\n", rango->id_hilo);
        return NULL;
    }
    archivo = fopen(rango->ruta_archivo, "rb");
    if (archivo == NULL) {
        close(cliente);
        return NULL;
    }

    fseek(archivo, rango->inicio, SEEK_SET);
    posicion = rango->inicio;
    while (posicion < rango->fin) {
        long restante = rango->fin - posicion;
        int cantidad = restante > BUFFER_SIZE ? BUFFER_SIZE : (int)restante;
        mensaje.tipo_mensaje = TIPO_DATOS;
        mensaje.offset = posicion;
        mensaje.size = (int)fread(mensaje.datos, 1, cantidad, archivo);
        if (mensaje.size <= 0 || enviar_completo(cliente, &mensaje, sizeof(mensaje)) < 0) {
            fclose(archivo);
            close(cliente);
            return NULL;
        }
        posicion += mensaje.size;
        bloques++;
    }

    memset(&mensaje, 0, sizeof(mensaje));
    mensaje.tipo_mensaje = TIPO_FIN_HILO;
    if (enviar_completo(cliente, &mensaje, sizeof(mensaje)) == 0)
        rango->correcto = 1;
    printf("[Hilo %d] Seccion %ld-%ld enviada (%ld bloques).\n",
           rango->id_hilo, rango->inicio, rango->fin, bloques);
    fclose(archivo);
    close(cliente);
    return NULL;
}

int main(void) {
    DIR *dir;
    struct dirent *entrada;
    char lista[50][256], ruta[512];
    int cantidad_archivos = 0, seleccion = 0;
    FILE *archivo;
    long tamano_total;
    MensajeRed metadata = {0};
    pthread_t hilos[N_THREADS];
    RangoHilo rangos[N_THREADS];

    dir = opendir(CARPETA_PRUEBAS);
    if (dir == NULL) { perror("No se pudo abrir archivos_prueba"); return 1; }
    printf("Archivos disponibles:\n");
    while ((entrada = readdir(dir)) != NULL && cantidad_archivos < 50) {
        if (entrada->d_name[0] == '.') continue;
        snprintf(lista[cantidad_archivos], sizeof(lista[0]), "%s", entrada->d_name);
        printf("  [%d] %s\n", cantidad_archivos + 1, lista[cantidad_archivos]);
        cantidad_archivos++;
    }
    closedir(dir);
    if (cantidad_archivos == 0) { printf("No hay archivos para enviar.\n"); return 1; }

    do {
        printf("Elija un archivo: ");
        if (scanf("%d", &seleccion) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            seleccion = 0;
        }
    } while (seleccion < 1 || seleccion > cantidad_archivos);

    snprintf(ruta, sizeof(ruta), "%s%s", CARPETA_PRUEBAS, lista[seleccion - 1]);
    archivo = fopen(ruta, "rb");
    if (archivo == NULL) { perror("No se pudo abrir el archivo"); return 1; }
    fseek(archivo, 0, SEEK_END);
    tamano_total = ftell(archivo);
    fclose(archivo);

    int control = conectar_servidor();
    if (control < 0) { printf("El servidor no esta activo.\n"); return 1; }
    metadata.tipo_mensaje = TIPO_METADATA;
    metadata.tamano_archivo = tamano_total;
    snprintf(metadata.nombre_archivo, sizeof(metadata.nombre_archivo), "%s", lista[seleccion - 1]);
    if (enviar_completo(control, &metadata, sizeof(metadata)) < 0) {
        close(control); return 1;
    }
    close(control);

    printf("Enviando %s (%ld bytes) con %d hilos...\n",
           lista[seleccion - 1], tamano_total, N_THREADS);
    for (int i = 0; i < N_THREADS; i++) {
        rangos[i].id_hilo = i + 1;
        rangos[i].inicio = i * (tamano_total / N_THREADS);
        rangos[i].fin = (i == N_THREADS - 1) ? tamano_total
                                             : (i + 1) * (tamano_total / N_THREADS);
        rangos[i].correcto = 0;
        snprintf(rangos[i].ruta_archivo, sizeof(rangos[i].ruta_archivo), "%s", ruta);
        pthread_create(&hilos[i], NULL, enviar_seccion, &rangos[i]);
    }

    int todo_correcto = 1;
    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(hilos[i], NULL);
        if (!rangos[i].correcto) todo_correcto = 0;
    }
    if (!todo_correcto) { printf("La transferencia no se completo.\n"); return 1; }
    printf("Archivo transmitido correctamente.\n");
    return 0;
}
