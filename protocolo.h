#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#define PUERTO 6000          
#define BUFFER_SIZE 8192     

typedef struct __attribute__((packed)) {
    long offset;             
    int size;                
    char datos[BUFFER_SIZE]; 
} BloqueArchivo;

#endif
