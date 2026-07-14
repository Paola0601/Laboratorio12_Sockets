#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#define PUERTO 6000
#define BUFFER_SIZE 8192
#define N_THREADS 4

#define TIPO_METADATA 1
#define TIPO_DATOS 2
#define TIPO_FIN_HILO 3

typedef struct {
    int tipo_mensaje;
    long offset;
    long tamano_archivo;
    int size;
    char nombre_archivo[256];
    char datos[BUFFER_SIZE];
} MensajeRed;

#endif
