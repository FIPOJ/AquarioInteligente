import socket

HOST = '0.0.0.0'
PORT = 5000

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()
    print(f"Servidor rodando em {HOST}:{PORT}")

    while True:
        conn, addr = s.accept()
        with conn:
            print(f"Conexão estabelecida com {addr}")
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                print(f"Recebido: {data.decode()}")
                # Enviando resposta para a ESP32
                response = f"Mensagem recebida: {data.decode()}"
                conn.sendall(response.encode())
