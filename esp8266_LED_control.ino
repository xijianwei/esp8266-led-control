#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>       // tzapu/WiFiManager
#include <ArduinoOTA.h>

// ===== MQTT配置 =====
const char* mqttBroker = "broker.hivemq.com";
const int   mqttPort   = 1883;
const char* deviceId   = "nodemcu_a1b2c3";

// 网页 → 设备
char topicControl[64];   // LED 开关指令，payload: ON / OFF
char topicCmd[64];       // 设备命令，payload: RESET_WIFI

// 设备 → 网页
char topicOTA[64];       // OTA 升级结果，payload: DONE / ERROR:x
char topicStatus[64];    // 设备状态（retain），payload: {"ip":"x.x.x.x","led":"ON/OFF","src":"boot/cmd"}

const int ledPin  = 2;
bool ledState     = false;

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

void publishStatus(const char* src = "boot") {
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"ip\":\"%s\",\"led\":\"%s\",\"src\":\"%s\"}",
    WiFi.localIP().toString().c_str(),
    ledState ? "ON" : "OFF",
    src);
  mqttClient.publish(topicStatus, buf, true);  // retain=true
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  if (strcmp(topic, topicControl) == 0) {
    if (msg == "ON") {
      ledState = true;
      digitalWrite(ledPin, HIGH);
    } else if (msg == "OFF") {
      ledState = false;
      digitalWrite(ledPin, LOW);
    }
    publishStatus("cmd");
    Serial.println("Command: " + msg);
  } else if (strcmp(topic, topicCmd) == 0 && msg == "RESET_WIFI") {
    Serial.println("RESET_WIFI received, clearing config and restarting...");
    mqttClient.publish(topicStatus, "{\"ip\":\"\",\"led\":\"RESETTING\",\"src\":\"cmd\"}", true);
    mqttClient.loop();
    delay(500);
    WiFiManager wm;
    wm.resetSettings();
    ESP.restart();
  }
}

static unsigned long reconnectAt = 0;  // 下次允许重连的时间戳

void reconnectMQTT() {
  if (millis() < reconnectAt) return;  // 未到重试时间，直接返回让 loop 继续跑

  // WiFi 掉线时先触发重连，本次直接返回，下次 loop 再检查结果
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    reconnectAt = millis() + 10000;
    return;
  }

  Serial.print("Connecting MQTT...");
  char clientId[48];
  snprintf(clientId, sizeof(clientId), "ESP8266-%s-%04x", deviceId, (unsigned)random(0xffff));
  if (mqttClient.connect(clientId)) {
    Serial.println(" OK");
    mqttClient.subscribe(topicControl, 1);
    mqttClient.subscribe(topicCmd, 1);
    publishStatus();
    reconnectAt = 0;
  } else {
    Serial.print(" failed rc=");
    Serial.print(mqttClient.state());
    Serial.println(", retry in 5s");
    reconnectAt = millis() + 5000;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  snprintf(topicControl, sizeof(topicControl), "esp8266/led/%s/control", deviceId);
  snprintf(topicCmd,     sizeof(topicCmd),     "esp8266/led/%s/cmd",     deviceId);
  snprintf(topicOTA,     sizeof(topicOTA),     "esp8266/led/%s/ota",     deviceId);
  snprintf(topicStatus,  sizeof(topicStatus),  "esp8266/led/%s/status",  deviceId);

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

  // OTA 配置
  ArduinoOTA.setHostname(deviceId);
  ArduinoOTA.setPassword("ota12345678");
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    Serial.println("\n[OTA] Start: " + type);
    // onStart 时 MQTT 还可用，发通知后立即退出，不调用 loop()
    mqttClient.publish(topicOTA, "START", false);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Done, rebooting...");
    mqttClient.publish(topicOTA, "DONE", false);
    mqttClient.loop();
    delay(200);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int pct = progress * 100 / total;
    // \r 覆盖同一行，串口显示滚动进度
    Serial.printf("[OTA] Progress: %u%% (%u / %u bytes)\r", pct, progress, total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\n[OTA] Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
    char buf[32];
    snprintf(buf, sizeof(buf), "ERROR:%u", error);
    mqttClient.publish(topicOTA, buf, false);
    mqttClient.loop();
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready, hostname: " + String(deviceId));

  Serial.println("Control topic: " + String(topicControl));
  Serial.println("Cmd topic:     " + String(topicCmd));
  Serial.println("Status topic:  " + String(topicStatus));
  Serial.println("OTA topic:     " + String(topicOTA));
}

void loop() {
  ArduinoOTA.handle();
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}
