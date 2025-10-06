# Chat de Texto Multisala — Guía Completa (Servidor en C + Cliente en Python)

Este documento reúne **toda** la información en un **solo archivo `.md`**:
- Guía general (paso a paso) para levantar **servidor** y **cliente**.
- Mini‑readme del **servidor** (rama `serve`).
- Mini‑readme del **cliente** (rama `client`).

> Recomendación: clona el repo **dos veces** (en carpetas distintas): una para el **servidor** y otra para el **cliente**. Así puedes correr ambos al mismo tiempo sin cambiar de rama en la misma carpeta.

---

## Índice
- [Requisitos](#requisitos)
- [Paso a paso recomendado](#paso-a-paso-recomendado)
  - [Clonar el repo dos veces](#1-clonar-el-repo-dos-veces)
  - [Compilar y correr el servidor](#2-compilar-y-correr-el-servidor-carpeta-server-chat-rama-serve)
  - [Ejecutar el cliente](#3-ejecutar-el-cliente-carpeta-client-chat-rama-client)
- [¿Cómo funciona?](#cómo-funciona)
- [Problemas comunes y soluciones](#problemas-comunes-y-soluciones)
- [Estructura del proyecto (resumen)](#estructura-del-proyecto-resumen)
- [Instrucciones rápidas por rama](#instrucciones-rápidas-por-rama)
  - [Servidor — rama `serve`](#servidor--rama-serve)
  - [Cliente — rama `client`](#cliente--rama-client)

---

## Requisitos

**Servidor (rama `serve`)**
- Linux/Unix (probado en Ubuntu/Debian).
- `gcc`, `make`, `libpthread`.
  ```bash
  sudo apt update
  sudo apt install -y build-essential
  ```
- Puerto **8080/TCP** abierto (por defecto).

**Cliente (rama `client`)**
- Linux/Unix o macOS (Windows: WSL).
- **Python 3.7+** (solo librería estándar).

---

## Paso a paso recomendado

### 1) Clonar el repo **dos veces**

> Sustituye el nombre del repo por el tuyo si es diferente.

```bash
# Carpeta para el servidor
git clone https://github.com/Emmanuelcasta/Parcial2.git server-chat
cd server-chat
git switch serve

# (en otra carpeta) Carpeta para el cliente
cd ..
git clone https://github.com/Emmanuelcasta/Parcial2.git client-chat
cd client-chat
git switch client
```

### 2) Compilar y correr el **servidor** (carpeta `server-chat`, rama `serve`)

```bash
cd server-chat
make -f build/Makefile
./server 0.0.0.0 8080 ./server.log
```
- Queda escuchando en **0.0.0.0:8080**.
- Log en `./server.log` (útil ver con `tail -f server.log`).

### 3) Ejecutar el **cliente** (carpeta `client-chat`, rama `client`)

Revisa `src/config.py`:
```python
SERVER_IP = "127.0.0.1"   # IP del servidor; 127.0.0.1 si es la misma máquina
PORT = 8080
DEBUG = True
```

Corre **dos clientes** (dos terminales distintas) para probar el chat:
```bash
cd client-chat
python3 -m src.chat_main
# Username: emmanuel
# Sala (room_id numérico): 1
```
En otra terminal:
```bash
cd client-chat
python3 -m src.chat_main
# Username: santiago
# Sala (room_id numérico): 1
```

Escribe y verás:
```
emmanuel: hola
santiago: ¡qué hubo!
```

> Para salir del cliente: escribe **`/q`** y presiona **Enter**.

---

## ¿Cómo funciona?

- **Multisala**: cada número de sala (`1`, `2`, `99`, …) es un chat independiente.  
  Solo los clientes que ingresan a la **misma** sala se leen entre sí.
- **Login** (el cliente lo hace por ti):
  ```
  LOGIN|<room_id>|<username>\n
  ```
- **Mensaje** (el cliente lo hace por ti):
  ```
  MSG|<room_id>|<texto>\n
  ```
- **Difusión** (servidor → clientes de la sala):
  ```
  FROM|<room_id>|<username>: <texto>\n
  ```
- **Salida**:
  ```
  LEAVE|<room_id>|\n
  ```
  Aviso a la sala:
  ```
  SYS|<room_id>|<username> salió del chat.\n
  ```

---

## Problemas comunes y soluciones

- **El cliente no conecta**
  - ¿El servidor está corriendo? Mira `tail -f server.log`.
  - ¿`SERVER_IP` en `src/config.py` apunta a la IP correcta?
  - ¿Firewall? Abre el puerto 8080/TCP o desactívalo momentáneamente para pruebas.

- **`python3 -m src.chat_main`: “No module named src.chat_main”**
  - Ejecuta el comando **desde la carpeta que contiene `src/`**.
  - Asegúrate de que exista `src/__init__.py`.

- **Servidor: `bind: Address already in use`**
  - El puerto 8080 está ocupado.
  ```bash
  ss -ltnp | grep 8080
  ```
  - Cambia el puerto en el código (en `src/utils.c`, `#define PORT 8080`) o detén el proceso que lo usa.

- **No llegan los mensajes**
  - Verifica que **ambos clientes** están en **la misma sala** (mismo número).

---

## Estructura del proyecto (resumen)

**Rama `serve`** (C):
```
build/Makefile
include/*.h
src/*.c
server           # binario al compilar
server.log       # log al ejecutar
```

**Rama `client`** (Python):
```
src/
  chat_main.py
  chat_comm.py
  config.py
  logging_utils.py
  __init__.py
transaction_log.txt
```

---

## Instrucciones rápidas por rama

### Servidor — rama `serve`

**Requisitos**
```bash
sudo apt update
sudo apt install -y build-essential
```

**Compilar**
```bash
make -f build/Makefile
```

**Ejecutar**
```bash
./server 0.0.0.0 8080 ./server.log
# ver logs:
tail -f server.log
```

**Protocolo (resumen)**
```
LOGIN|<room_id>|<username>\n
MSG|<room_id>|<texto>\n
FROM|<room_id>|<username>: <texto>\n
LEAVE|<room_id>|\n
SYS|<room_id>|<username> salió del chat.\n
```

---

### Cliente — rama `client`

**Requisitos**
- Python 3.7+

**Configurar** (`src/config.py`)
```python
SERVER_IP = "127.0.0.1"  # o la IP real del servidor
PORT = 8080
DEBUG = True
```

**Ejecutar**
```bash
python3 -m src.chat_main
# Ingresar Username y número de sala (ej. 1)
# Para salir: /q
```

**Log del cliente**
- `transaction_log.txt` guarda los envíos/recepciones.

**Refencias**
- https://github.com/SebasUr/Battleship-Networking.git (Un proyecto anterior de telematica en el cual Santiago trabaje el semestre pasado y justo trabajamos el tema de la comunicacion por medio de hilos)
