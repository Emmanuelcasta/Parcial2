# chat_comm.py
import socket
import threading
from .logging_utils import log_transaction
from .config import PORT, SERVER_IP, DEBUG

def _send(sock, s: str):
    if not s.endswith("\n"):
        s = s + "\n"
    sock.sendall(s.encode())

def _reader(sock, room_id, stop_flag):
    client_ip = socket.gethostbyname(socket.gethostname())
    server_ip = sock.getpeername()[0]
    buffer = ""
    while not stop_flag["exit"]:
        try:
            data = sock.recv(4096).decode()
            if not data:
                print("\n[Conexión cerrada por el servidor]")
                stop_flag["exit"] = True
                break
            buffer += data
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                if not line:
                    continue
                if DEBUG:
                    print(f"[DEBUG] RX: {line!r}")
                log_transaction(line, server_ip, client_ip, " - SERVER")
                parts = line.split("|", 2)
                if parts[0] == "FROM" and len(parts) >= 3:
                    print(parts[2])  # "usuario: texto"
                elif parts[0] == "SYS" and len(parts) >= 3:
                    print(f"[SYS] {parts[2]}")
                elif parts[0] == "GAME_END":
                    print("[Chat finalizado]")
                    stop_flag["exit"] = True
                    break
        except Exception as e:
            print(f"[Error RX] {e}")
            stop_flag["exit"] = True
            break

def run_chat():
    username = input("Username: ").strip()
    if not username:
        print("Username vacío.")
        return

    room = input("Sala (room_id numérico): ").strip()
    try:
        room_id = int(room)
    except ValueError:
        print("Sala inválida (usa entero).")
        return

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((socket.gethostbyname(SERVER_IP), PORT))
    print(f"Conectado a {SERVER_IP}:{PORT}")

    client_ip = socket.gethostbyname(socket.gethostname())
    server_ip = sock.getpeername()[0]

    # Handshake: con \n al final
    login = f"LOGIN|{room_id}|{username}"
    _send(sock, login); log_transaction(login, server_ip, client_ip, " - CLIENT")
    # JOIN es opcional; puedes omitirlo si quieres:
    join  = f"JOIN|{room_id}|{username}"
    _send(sock, join);  log_transaction(join,  server_ip, client_ip, " - CLIENT")

    stop_flag = {"exit": False}
    threading.Thread(target=_reader, args=(sock, room_id, stop_flag), daemon=True).start()

    print("Escribe mensajes ( '/q' para salir )")
    try:
        while not stop_flag["exit"]:
            line = input()
            if line.strip() in ("/q", "/quit", "/salir"):
                leave = f"LEAVE|{room_id}|"
                _send(sock, leave); log_transaction(leave, server_ip, client_ip, " - CLIENT")
                stop_flag["exit"] = True
                break
            msg = f"MSG|{room_id}|{line}"
            _send(sock, msg); log_transaction(msg, server_ip, client_ip, " - CLIENT")
    except (KeyboardInterrupt, EOFError):
        leave = f"LEAVE|{room_id}|"
        try:
            _send(sock, leave); log_transaction(leave, server_ip, client_ip, " - CLIENT")
        except Exception:
            pass
    finally:
        sock.close()
        print("Cliente chat cerrado.")
