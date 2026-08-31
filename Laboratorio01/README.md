# Laboratorio 01

## 1. Compilar

Desde la carpeta del proyecto:

```bash
gcc -Wall -Wextra server.c -o server
gcc -Wall -Wextra client.c -o client
```

## 2. Ejecutar el servidor

**Terminal 1:**

```bash
./server 54001
```

**Terminal 2:**

```bash
./client 127.0.0.1 54001
```

## 3. Probar con `nc`

### `nc` como cliente

```bash
nc 127.0.0.1 54001
```

### `nc` como servidor

**Terminal 1:**

```bash
nc -l 54001
```

**Terminal 2:**

```bash
./client 127.0.0.1 54001
```
