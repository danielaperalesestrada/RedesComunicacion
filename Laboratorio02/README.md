# Laboratorio 02 - Multi-clients Broadcast

Este proyecto implementa un servidor C++ que admite múltiples clientes conectados simultáneamente y distribuye los mensajes en modo *broadcast* utilizando sockets e hilos (`std::thread`).

## 1. Compilar

Desde la carpeta del proyecto, compilar utilizando `g++` e incluyendo la bandera `-pthread` para el soporte de hilos:

```bash
g++ -Wall -Wextra -std=c++11 server.cpp -o server -pthread
g++ -Wall -Wextra -std=c++11 client.cpp -o client -pthread

```

## 2. Ejecutar el servidor

**Terminal 1 (Servidor):**

```bash
./server 54001

```

## 3. Ejecutar múltiples clientes

Puedes abrir varias terminales para simular diferentes clientes conectados al servidor:

**Terminal 2 (Cliente 1):**

```bash
./client 127.0.0.1 54001

```

**Terminal 3 (Cliente 2):**

```bash
./client 127.0.0.1 54001

```

Todo mensaje enviado desde un cliente será retransmitido (*broadcast*) a los demás clientes conectados.

## 4. Probar con `nc` (Netcat)

### Usar `nc` como cliente adicional

Puedes conectar una instancia de Netcat para verificar el reenvío de mensajes:

```bash
nc 127.0.0.1 54001

```

### Para salir

Escribe `END` en la consola del cliente o presiona `Ctrl + C` para cerrar la conexión.
