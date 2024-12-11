import requests

# Defina suas credenciais do ThingSpeak
THINGSPEAK_API_READ_KEY = "C8KYD4W3XNM5D09K"
THINGSPEAK_CHANNEL_ID = "2780336"
THINGSPEAK_URL_READ = f"http://api.thingspeak.com/channels/{THINGSPEAK_CHANNEL_ID}/feeds/last.json"

def get_thingspeak_data():
    try:
        # Faz a requisição GET para o ThingSpeak
        response = requests.get(THINGSPEAK_URL_READ, params={'api_key': THINGSPEAK_API_READ_KEY})
        
        # Verifica se a requisição foi bem-sucedida
        if response.status_code == 200:
            # Imprime o corpo da resposta em formato JSON
            print("Corpo da resposta:", response.json())
        else:
            print(f"Falha ao obter dados, código de status: {response.status_code}")
    except requests.exceptions.RequestException as e:
        print(f"Erro ao realizar a requisição: {e}")

if __name__ == "__main__":
    get_thingspeak_data()