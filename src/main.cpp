#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <MQTTClient.h>
#include "secrets_aws.h"

#define AWS_THING_NAME "esp32_holter"
#define AWS_IOT_PUBLISH_TOPIC "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

WiFiClientSecure wifi_client;
MQTTClient client(512);

const int mqtt_port = 8883;
const int LED_PIN = 2;
unsigned long lastPublish = 0;
const unsigned long publishInterval = 60000;  // Publicar cada 60 segundos
unsigned long lastLedToggle = 0;
const unsigned long ledInterval = 1000;       // Intervalo de parpadeo del LED (1 segundo)

//---------------------Conectar a WiFi---------------------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;  // No reconectar si ya está conectado

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    Serial.println("Conectando a WiFi...");
  }

  Serial.print("Conectado a la red WiFi: ");
  Serial.println(WIFI_SSID);
}

//---------------------Conectar a AWS IoT---------------------
void connectAWS() {
  if (client.connected()) return;  // No reconectar si ya está conectado

  Serial.println("Procesando certificados...");

  wifi_client.setCACert(AWS_CERT_CA);
  wifi_client.setCertificate(AWS_CERT_CRT);
  wifi_client.setPrivateKey(AWS_CERT_PRIVATE);

  client.begin(AWS_IOT_ENDPOINT, mqtt_port, wifi_client);
  Serial.println("Conectando a AWS IoT...");

  int retry_count = 0;
  const int max_retries = 10;

  while (!client.connect(AWS_THING_NAME)) {
    Serial.print(".");
    delay(200);
    retry_count++;

    if (retry_count >= max_retries) {
      Serial.println("\nConexión fallida. Reiniciando...");
      ESP.restart();
    }
  }

  if (client.connected()) {
    Serial.println("\nConectado a AWS IoT");
  } else {
    Serial.println("Error: No se pudo conectar a AWS IoT");
  }
}

// --------------------------------Manejador de mensajes--------------------------------
void messageHandler(String &topic, String &payload) {
  Serial.println("Mensaje MQTT recibido!");
  Serial.println("Topic: " + topic);
  Serial.println("Contenido: " + payload);
}

// --------------------------------Publicar mensaje--------------------------------
void publishMessage() {
  client.publish(AWS_IOT_PUBLISH_TOPIC, "{\"message\": \"Message from ESP32\"}");
  Serial.println("Mensaje publicado!");
}

// --------------------------------Led On/Off---------------------------
void ledStatus() {
  if (millis() - lastLedToggle >= ledInterval) {
    lastLedToggle = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // Alternar estado del LED
  }
}

// --------------------------------Setup--------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  connectWiFi();  // Conectar a WiFi
  connectAWS();   // Conectar a AWS IoT

  // Suscribirse a un tópico
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  
  // Crear un manejador de mensajes
  client.onMessage(messageHandler);
}

// --------------------------------Loop--------------------------------
void loop() {
  ledStatus();  // Control del LED sin bloquear el programa

  // Verificar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado, reconectando...");
    connectWiFi();
  }

  // Verificar conexión AWS IoT
  if (!client.connected()) {
    Serial.println("MQTT desconectado, reconectando...");
    connectAWS();
  }

  client.loop();  // Mantener conexión MQTT

  unsigned long currentMillis = millis();

  // Publicar mensaje solo si ha pasado el intervalo definido
  if (currentMillis - lastPublish >= publishInterval) {
    publishMessage();
    lastPublish = currentMillis;
  }
}
