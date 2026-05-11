#include <Arduino.h>
#include "DHT.h"

// ─── CONFIGURAÇÕES ────────────────────────────────────────────
#define DHTPIN 15           // GPIO do sensor DHT22
#define DHTTYPE DHT22       // Tipo do sensor
#define BUTTON_PIN 18       // GPIO do botão (BPM)

#define MAX_REGISTROS 100   // Máximo de registros no buffer offline
#define INTERVALO_LEITURA 3000  // Leitura a cada 3 segundos
#define JANELA_BPM 10000    // Janela de 10s para calcular BPM

DHT dht(DHTPIN, DHTTYPE);

// ─── ESTRUTURA DE DADO ────────────────────────────────────────
struct Leitura {
  float temperatura;
  float umidade;
  int bpm;
  bool enviado;
  unsigned long timestamp;
};

// ─── VARIÁVEIS GLOBAIS ────────────────────────────────────────
Leitura buffer[MAX_REGISTROS];  // Buffer de armazenamento offline
int totalRegistros = 0;

bool wifiConectado = false;           // Simula conectividade Wi-Fi
unsigned long ultimaLeitura = 0;
unsigned long ultimoToggleWifi = 0;

volatile int contadorBotao = 0;       // Contador de cliques do botão
unsigned long inicioJanela = 0;
int bpmAtual = 0;

// ─── INTERRUPÇÃO DO BOTÃO ─────────────────────────────────────
// Incrementa o contador a cada pressão do botão
void IRAM_ATTR botaoPressionado() {
  contadorBotao++;
}

// ─── SETUP ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  dht.begin();

  // Configura botão com pull-up interno e interrupção
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), botaoPressionado, FALLING);

  inicioJanela = millis();

  Serial.println("===========================================");
  Serial.println("       CardioIA - Monitor Cardíaco        ");
  Serial.println("===========================================");
  Serial.println("Sistema iniciado. WiFi: DESCONECTADO");
  Serial.println();
}

// ─── CALCULA BPM ─────────────────────────────────────────────
// Extrapola os cliques na janela de 10s para BPM (60s)
int calcularBPM() {
  unsigned long agora = millis();
  if (agora - inicioJanela >= JANELA_BPM) {
    bpmAtual = (contadorBotao * 60000) / JANELA_BPM;
    contadorBotao = 0;
    inicioJanela = agora;
  }
  return bpmAtual;
}

// ─── AVALIA ALERTAS ──────────────────────────────────────────
void avaliarAlertas(float temp, float umid, int bpm) {
  if (temp > 38.0)
    Serial.println("  ⚠️  ALERTA: Temperatura elevada (febre)!");
  if (bpm > 120)
    Serial.println("  ⚠️  ALERTA: BPM elevado (taquicardia)!");
  if (bpm > 0 && bpm < 40)
    Serial.println("  ⚠️  ALERTA: BPM baixo (bradicardia)!");
  if (umid > 80)
    Serial.println("  ⚠️  ALERTA: Umidade elevada!");
}

// ─── SALVA LOCALMENTE (BUFFER OFFLINE) ───────────────────────
// Estratégia FIFO: descarta o mais antigo quando buffer cheio
void salvarLocal(float temp, float umid, int bpm) {
  if (totalRegistros >= MAX_REGISTROS) {
    for (int i = 0; i < MAX_REGISTROS - 1; i++) {
      buffer[i] = buffer[i + 1];
    }
    totalRegistros = MAX_REGISTROS - 1;
    Serial.println("  ⚠️  Buffer cheio! Registro mais antigo descartado (FIFO).");
  }

  buffer[totalRegistros] = { temp, umid, bpm, false, millis() };
  totalRegistros++;

  Serial.print("  📦 Salvo localmente. Buffer: ");
  Serial.print(totalRegistros);
  Serial.print("/");
  Serial.println(MAX_REGISTROS);
}

// ─── SINCRONIZA COM NUVEM ─────────────────────────────────────
// Envia todos os registros pendentes via Serial (simulação MQTT)
void sincronizarNuvem() {
  int pendentes = 0;
  for (int i = 0; i < totalRegistros; i++) {
    if (!buffer[i].enviado) pendentes++;
  }

  if (pendentes == 0) return;

  Serial.println();
  Serial.println("  ☁️  Sincronizando dados pendentes...");
  Serial.println("  TOPIC: cardioia/sinais");
  Serial.println("  ------------------------------------------");

  for (int i = 0; i < totalRegistros; i++) {
    if (!buffer[i].enviado) {
      Serial.print("  PUBLISH -> {");
      Serial.print("\"temp\":"); Serial.print(buffer[i].temperatura, 1);
      Serial.print(",\"umid\":"); Serial.print(buffer[i].umidade, 1);
      Serial.print(",\"bpm\":"); Serial.print(buffer[i].bpm);
      Serial.print(",\"ts\":"); Serial.print(buffer[i].timestamp);
      Serial.println("}");
      buffer[i].enviado = true;
    }
  }

  Serial.println("  ------------------------------------------");
  Serial.print("  ✅ ");
  Serial.print(pendentes);
  Serial.println(" registro(s) sincronizado(s) com sucesso!");
  Serial.println();

  // Limpa buffer após sincronização
  totalRegistros = 0;
}

// ─── LOOP PRINCIPAL ──────────────────────────────────────────
void loop() {
  unsigned long agora = millis();

  // Alterna Wi-Fi a cada 15 segundos (simulação de conectividade)
  if (agora - ultimoToggleWifi >= 15000) {
    wifiConectado = !wifiConectado;
    ultimoToggleWifi = agora;
    Serial.print("\n🔌 WiFi: ");
    Serial.println(wifiConectado ? "CONECTADO" : "DESCONECTADO");

    if (wifiConectado) {
      sincronizarNuvem();
    }
  }

  // Leitura periódica a cada 3 segundos
  if (agora - ultimaLeitura >= INTERVALO_LEITURA) {
    ultimaLeitura = agora;

    float temp = dht.readTemperature();
    float umid = dht.readHumidity();
    int bpm = calcularBPM();

    if (isnan(temp) || isnan(umid)) {
      Serial.println("Erro na leitura do DHT22!");
      return;
    }

    Serial.println("-------------------------------------------");
    Serial.print("🌡️  Temp: "); Serial.print(temp, 1); Serial.println(" °C");
    Serial.print("💧 Umid: "); Serial.print(umid, 1); Serial.println(" %");
    Serial.print("❤️  BPM:  "); Serial.println(bpm);
    Serial.print("📡 WiFi: "); Serial.println(wifiConectado ? "ON" : "OFF");

    avaliarAlertas(temp, umid, bpm);

    if (!wifiConectado) {
      // Offline: salva no buffer local
      salvarLocal(temp, umid, bpm);
    } else {
      // Online: publica direto (simulação MQTT via Serial)
      Serial.print("  📤 PUBLISH -> {");
      Serial.print("\"temp\":"); Serial.print(temp, 1);
      Serial.print(",\"umid\":"); Serial.print(umid, 1);
      Serial.print(",\"bpm\":"); Serial.print(bpm);
      Serial.println("}");
    }
  }
}