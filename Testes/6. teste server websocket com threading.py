import socket
import threading
import time

# Variáveis globais
mensagem_recebida = None  # Para armazenar a mensagem recebida
mensagem_enviar = ""  # Para armazenar a mensagem a ser enviada
lock = threading.Lock()  # Lock para proteger as variáveis globais

HOST = '0.0.0.0'
PORT = 5000

def servidor_tcp():
    """Função para rodar o servidor TCP."""
    global mensagem_recebida, mensagem_enviar

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

                    # Atualizando a mensagem recebida com lock
                    with lock:
                        mensagem_recebida = data.decode()
                        print(f"Recebido: {mensagem_recebida}")

                    # Verificando e enviando a mensagem
                    with lock:
                        if mensagem_enviar:
                            conn.sendall(mensagem_enviar.encode())
                            print(f"Enviado: {mensagem_enviar}")
                            mensagem_enviar = ""  # Limpando a mensagem de envio

def processar_mensagens():
    """Função para processar mensagens recebidas e preparar mensagens para envio."""
    global mensagem_recebida, mensagem_enviar

    while True:
        with lock:
            if mensagem_recebida:
                print(f"Processando mensagem recebida: {mensagem_recebida}")
                # Simulação de processamento
                mensagem_enviar = f"Resposta à mensagem: {mensagem_recebida}"
                mensagem_recebida = None  # Limpando a mensagem recebida

        time.sleep(0.1)  # Evita uso excessivo da CPU

def outra_funcao():
    """Outra função que utiliza as variáveis globais."""
    global mensagem_enviar  # Declarando variável global
    while True:
        with lock:
            if not mensagem_enviar:
                mensagem_enviar = "Mensagem padrão para envio"  # Exemplo de lógica
        time.sleep(5)  # Simulação de tempo para enviar mensagens automáticas

if __name__ == "__main__":
    # Criando threads
    thread_servidor = threading.Thread(target=servidor_tcp, daemon=True)
    thread_processador = threading.Thread(target=processar_mensagens, daemon=True)
    thread_outra = threading.Thread(target=outra_funcao, daemon=True)

    # Iniciando threads
    thread_servidor.start()
    thread_processador.start()
    thread_outra.start()

    # Mantendo o programa principal rodando
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Encerrando o programa.")
