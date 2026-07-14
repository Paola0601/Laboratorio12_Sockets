#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#define PUERTO 6000          
#define BUFFER_SIZE 8192     

#define TIPO_METADATA 1
#define TIPO_DATOS    2

typedef struct __attribute__((packed)) {
    int tipo_mensaje;      
    long offset;            
    int size;                 
    char nombre_archivo[256]; 
    char datos[BUFFER_SIZE];  
} MensajeRed;

#endif
