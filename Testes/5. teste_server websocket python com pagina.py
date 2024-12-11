from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import socket
import threading

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app)

# Variável para armazenar as mensagens recebidas
messages = []

# Página principal
@app.route('/')
def index():
    return render_template('index.html')

# Rota WebSocket para enviar mensagens ao cliente web
@socketio.on('connect')
def handle_connect():
    # Enviar mensagens anteriores ao conectar
    for message in messages:
        emit('new_message', message)

# Função para lidar com o servidor TCP
def start_tcp_server():
    HOST = '0.0.0.0'
    PORT = 5000

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"Servidor TCP rodando em {HOST}:{PORT}")
        
        while True:
            conn, addr = s.accept()
            with conn:
                print(f"Conexão estabelecida com {addr}")
                while True:
                    data = conn.recv(1024)
                    if not data:
                        break
                    message = data.decode()
                    print(f"Recebido: {message}")
                    
                    # Armazena a mensagem e envia ao navegador
                    messages.append(message)
                    socketio.emit('new_message', message)

# Inicia o servidor TCP em uma thread separada
threading.Thread(target=start_tcp_server, daemon=True).start()

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5001)
