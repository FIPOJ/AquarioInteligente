import asyncio
import websockets
import threading
from flask import Flask, render_template, request, jsonify

# Variáveis globais
mensagem_recebida = None  # Para armazenar a mensagem recebida
mensagem_enviar = ""  # Para armazenar a mensagem a ser enviada

# Inicializando o Flask
app = Flask(__name__)

@app.route('/')
def index():
    """Rota principal que exibe a página com a mensagem recebida e o formulário para enviar mensagens."""
    global mensagem_recebida
    mensagem = mensagem_recebida if mensagem_recebida else "Nenhuma mensagem recebida."
    return render_template('index.html', mensagem_recebida=mensagem)

@app.route('/enviar', methods=['POST'])
def enviar():
    """Rota para enviar a mensagem através do formulário web."""
    global mensagem_enviar
    mensagem = request.form['mensagem']
    mensagem_enviar = mensagem  # Atualizando a mensagem para envio
    print(f"Mensagem para enviar: {mensagem_enviar}")
    return jsonify({'status': 'Mensagem enviada para a ESP32.'}), 200

async def websocket_handler(websocket, path):
    """Lida com conexões WebSocket."""
    global mensagem_recebida, mensagem_enviar
    print("Conexão WebSocket estabelecida.")
    try:
        while True:
            # Envia mensagem se existir
            if mensagem_enviar:
                await websocket.send(mensagem_enviar)
                print(f"Enviado para ESP32: {mensagem_enviar}")
                mensagem_enviar = ""  # Limpa após envio

            # Aguarda mensagens da ESP32
            mensagem_recebida = await websocket.recv()
            print(f"Recebido da ESP32: {mensagem_recebida}")

    except websockets.exceptions.ConnectionClosed:
        print("Conexão WebSocket encerrada.")

async def iniciar_servidor_websocket():
    """Inicia o servidor WebSocket."""
    print("Servidor WebSocket iniciado na porta 5000.")
    async with websockets.serve(websocket_handler, "0.0.0.0", 5000):
        await asyncio.Future()  # Mantém o servidor rodando

def executar_servidor_websocket():
    """Executa o servidor WebSocket em uma thread separada."""
    asyncio.run(iniciar_servidor_websocket())

if __name__ == "__main__":
    # Inicia o servidor WebSocket em uma thread
    thread_websocket = threading.Thread(target=executar_servidor_websocket, daemon=True)
    thread_websocket.start()

    # Inicia o servidor Flask
    app.run(host='0.0.0.0', port=5001)
