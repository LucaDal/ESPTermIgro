#include <Arduino.h>
#include "SimpleOTA.h"
#include "WiFiManager.h"
#include "secret_data.h"
#include "AuthenticationManager.h"
#include <Wire.h>
#include "DFRobot_SHT20.h"
#include "PubSubClient.h"

#define DEEPSLEEP 300e6 //5 minuti
#define UPDATE_TIME 300000 //5 minuti

SimpleOTA *simpleOTA = new SimpleOTA();
JsonDocument doc;

const char * mqtt_username;
const char * mqtt_password;
const char *mqtt_topic = "domotica/mirandola/camera/temp_igro";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

DFRobot_SHT20 sht20;

unsigned long update_time = 0;

void connectToMQTTBroker() {

  int n_try = 5;
  while (!mqtt_client.connected() && n_try > 0) {
      Serial.println("Connecting...");
      String client_id = "esp8266-" + String(WiFi.macAddress());
      if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
          Serial.println("Connected to MQTT broker");
          mqtt_client.subscribe(mqtt_topic);
          break;
      } else {
          Serial.println("Failed connecting - retrying in 5 seconds");
          delay(5000);
      }
      n_try--;
  }
  if (n_try <= 0 && DEEPSLEEP > 0)
    ESP.deepSleep(DEEPSLEEP);
}

void update(){

  if(millis() > update_time){
    update_time = millis() + UPDATE_TIME;
    doc["temp"] = String(sht20.readTemperature(),1);
    doc["umid"] = String(sht20.readHumidity(),0); 
    mqtt_client.publish(mqtt_topic,doc.as<String>().c_str());
    delay(200);
    if(DEEPSLEEP > 0)
       ESP.deepSleep(DEEPSLEEP);
  }
}

void setup() {

  Wire.begin(2, 0);  //sda - scl
  sht20.initSHT20();

  AuthManger am((size_t)0, (size_t)512);
  //am.writeUsernameAndPassword("termo_igro_01", "admin");
  mqtt_username = am.getUserame();
  mqtt_password = am.getPassword();

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.autoConnect("TempIgro");

  if (WiFi.status() == WL_CONNECTED){
    simpleOTA->begin(512, SERVER_ADDRESS, TOKEN_ID, true);
  }

  mqtt_client.setServer(SERVER_ADDRESS, mqtt_port);
  connectToMQTTBroker();
}

void loop() {
  simpleOTA->checkUpdates(300);
  if (!mqtt_client.connected()) {
      connectToMQTTBroker();
  }else{
    update();
  }
  mqtt_client.loop();
}
