#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>       // tzapu/WiFiManager

// ===== MQTT配置 =====
const char* mqttBroker = "broker.hivemq.com";
const int   mqttPort   = 1883;
const char* deviceId   = "nodemcu_a1b2c3";

char topicControl[64];
char topicState[64];
char topicCmd[64];     // 设备命令主题，用于接收 RESET_WIFI 等指令

const int ledPin  = 2;
bool ledState     = false;

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

void publishState() {
  mqttClient.publish(topicState, ledState ? "ON" : "OFF", true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  String t = String(topic);

  if (t == topicControl) {
    if (msg == "ON") {
      ledState = true;
      digitalWrite(ledPin, HIGH);
    } else if (msg == "OFF") {
      ledState = false;
      digitalWrite(ledPin, LOW);
    }
    publishState();
    Serial.println("Command: " + msg);
  } else if (t == String(topicCmd) && msg == "RESET_WIFI") {
    Serial.println("RESET_WIFI received, clearing config and restarting...");
    // 先发一条确认消息再重置
    mqttClient.publish(topicState, "RESETTING", true);
    mqttClient.loop();
    delay(500);
    WiFiManager wm;
    wm.resetSettings();
    ESP.restart();
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  while (!mqttClient.connected()) {
    Serial.print("Connecting MQTT...");
    String clientId = "ESP8266-" + String(deviceId) + "-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" OK");
      mqttClient.subscribe(topicControl, 1);
      mqttClient.subscribe(topicCmd, 1);
      publishState();
    } else {
      Serial.print(" failed rc=");
      Serial.print(mqttClient.state());
      Serial.println(", retry in 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  snprintf(topicControl, sizeof(topicControl), "esp8266/led/%s/control", deviceId);
  snprintf(topicState,   sizeof(topicState),   "esp8266/led/%s/state",   deviceId);
  snprintf(topicCmd,     sizeof(topicCmd),     "esp8266/led/%s/cmd",     deviceId);

  // WiFiManager：有保存的WiFi就直连，没有就开热点 "ESP8266-Setup"
  WiFiManager wm;
  wm.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

  // 配网超时180秒，超时后重启重试
  wm.setConfigPortalTimeout(180);

  Serial.println("Starting WiFiManager...");
  if (!wm.autoConnect("ESP8266-Setup", "12345678")) {
    Serial.println("Config portal timeout, restarting...");
    ESP.restart();
  }

  Serial.println("WiFi connected: " + WiFi.localIP().toString());
  Serial.println("SSID: " + WiFi.SSID());

  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);

  Serial.println("Control topic: " + String(topicControl));
  Serial.println("State topic:   " + String(topicState));
  Serial.println("Cmd topic:     " + String(topicCmd));
}

void loop() {
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}
