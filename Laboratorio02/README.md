# Laboratorio 02 - Intermediary Chat Server and Client

Este proyecto implementa una arquitectura de chat intermediario multihilo y un protocolo de capa de aplicación basado en sockets TCP en C++. Permite comunicación uno a uno (Unicast), difusión a todos los clientes (Broadcast) y gestión de sesiones mediante nicknames únicos[cite: 1].

## 1. Compilación

Desde la carpeta raíz del proyecto, compila ambos archivos usando `g++` con soporte para hilos (`-pthread`):

```bash
g++ -Wall -Wextra -std=c++11 server.cpp -o server -pthread
g++ -Wall -Wextra -std=c++11 client.cpp -o client -pthread

```

## 2. Ejecutar el Servidor

Inicia el servidor especificando el puerto opcionalmente (si no se proporciona, usará por defecto el puerto `45000`):

```bash
# Ejecución con puerto por defecto (45000)
./server

# O especificando un puerto personalizado
./server 45000

```

## 3. Ejecutar los Clientes

Abre múltiples terminales para conectar diferentes usuarios al servidor:

```bash
# Sintaxis: ./client <IP> <puerto>
./client 127.0.0.1 45000

```

Al iniciar, el programa te solicitará un **Nickname** único para registrarte en el servidor antes de ingresar a la consola de comandos.

## 4. Comandos de la Aplicación

Dentro de la interfaz del cliente puedes enviar las siguientes tramas:

* **Enviar mensaje privado (Unicast):**
```text
M <Nickname_Destino> <Mensaje>

```


* **Enviar mensaje a todos (Broadcast):**
```text
B <Mensaje>

```


* **Desconectarse (Logout):**
```text
Q

```



## 5. Especificación del Protocolo de Aplicación

El protocolo utiliza *Action Bytes* y encabezados de tamaño fijo en texto:

* **Registro ('N'):** `'N'` + 7 bytes (longitud nick) + Nickname.


* **Unicast ('M' / 'm'):** `'M'` + 7 bytes (longitud nick destino) + Nickname destino + 11 bytes (longitud mensaje) + Mensaje.


* **Broadcast ('B' / 'b'):** `'B'` + 11 bytes (longitud mensaje) + Mensaje.


* **Logout ('Q'):** `'Q'`.



```

```
