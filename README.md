# Transferencia paralela de archivos con sockets

El cliente divide un archivo en 4 rangos. Cada hilo abre el mismo archivo,
lee solamente su rango y lo envia por una conexion TCP diferente. Cada bloque
incluye su posicion (`offset`), por lo que el servidor puede escribirlo en el
lugar correcto aunque los hilos lleguen en distinto orden.

## Compilar y ejecutar en Linux

```bash
make
```

En la primera terminal ejecute `./servidor`. En una segunda terminal ejecute
`./cliente`. Coloque antes archivos dentro de `archivos_prueba/`; el resultado
aparecera en `recibidos/`.

Compruebe que el original y el recibido sean identicos:

```bash
sha256sum archivos_prueba/nombre.txt recibidos/nombre.txt
```

Los dos valores deben ser iguales.

## Archivos

- `protocolo.h`: puerto, mensajes y estructura enviada por TCP.
- `cliente.c`: menu, division del archivo y cuatro hilos emisores.
- `servidor.c`: conexiones concurrentes y reconstruccion por `offset`.
- `Makefile`: compilacion con GCC y pthreads.

Se usa `127.0.0.1`, así que cliente y servidor se prueban en la misma maquina.
Para usar dos maquinas, cambie esa direccion por la IP del servidor.
