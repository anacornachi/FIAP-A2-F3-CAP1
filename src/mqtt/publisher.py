import paho.mqtt.client as mqtt
import json
import time
import random
import ssl

# ─── CONFIGURAÇÕES HIVEMQ ─────────────────────────────────────
BROKER   = "dc8d802607554c6cb32cacfed071c8b9.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "cardioia"
PASSWORD = "FIAPhu3hu3hu3BR!"
TOPIC    = "cardioia/sinais"

# ─── SIMULA LEITURAS DO ESP32 ─────────────────────────────────
def gerar_leitura():
    return {
        "temp": round(random.uniform(35.5, 39.5), 1),
        "umid": round(random.uniform(40.0, 85.0), 1),
        "bpm":  random.randint(45, 130),
        "ts":   int(time.time())
    }

# ─── CALLBACKS ────────────────────────────────────────────────
def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("✅ Conectado ao HiveMQ Cloud!")
    else:
        print(f"❌ Erro de conexão: {rc}")

def on_publish(client, userdata, mid, rc=None, properties=None):
    print(f"   📤 Mensagem {mid} publicada com sucesso")

# ─── SETUP CLIENTE MQTT ───────────────────────────────────────
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="cardioia-esp32")
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(tls_version=ssl.PROTOCOL_TLS_CLIENT)
client.on_connect = on_connect
client.on_publish = on_publish

# ─── CONECTA E PUBLICA ────────────────────────────────────────
print(f"🔌 Conectando em {BROKER}...")
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

time.sleep(2)  # aguarda conexão

print(f"\n📡 Publicando no tópico: {TOPIC}")
print("─" * 50)

try:
    while True:
        leitura = gerar_leitura()
        payload = json.dumps(leitura)

        client.publish(TOPIC, payload, qos=1)

        print(f"🌡️  Temp: {leitura['temp']}°C  "
              f"💧 Umid: {leitura['umid']}%  "
              f"❤️  BPM: {leitura['bpm']}")

        # Alertas
        if leitura['temp'] > 38.0:
            print("  ⚠️  ALERTA: Temperatura elevada (febre)!")
        if leitura['bpm'] > 120:
            print("  ⚠️  ALERTA: BPM elevado (taquicardia)!")
        if leitura['bpm'] < 40:
            print("  ⚠️  ALERTA: BPM baixo (bradicardia)!")
        if leitura['umid'] > 80:
            print("  ⚠️  ALERTA: Umidade elevada!")

        time.sleep(3)  # leitura a cada 3 segundos

except KeyboardInterrupt:
    print("\n🛑 Publicação encerrada.")
    client.loop_stop()
    client.disconnect()