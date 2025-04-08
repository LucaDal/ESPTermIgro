#include <Arduino.h>
#include "SimpleOTA.h"
#include "WiFiManager.h"
#include "secret_data.h"
#include "AuthenticationManager.h"
#include <Wire.h>
#include "PubSubClient.h"
#include "SHTSensor.h"
#include "cert.h"

#define DEEPSLEEP 600e6 //10 minuti
#define UPDATE_TIME 600e6 //10 minuti

SimpleOTA *simpleOTA = new SimpleOTA();
JsonDocument doc;

const char * mqtt_username;
const char * mqtt_password;

BearSSL::WiFiClientSecure esp_client;
PubSubClient mqtt_client(esp_client);

SHTSensor sht;

unsigned long update_time = 0;

void connectToMQTTBroker() {
  int n_try = 5;
  while (!mqtt_client.connected() && n_try > 0) {
      Serial.println("Connecting...");
      String client_id = "esp8266-" + String(WiFi.macAddress());
      if (mqtt_client.connect(client_id.c_str(),mqtt_username, mqtt_password)) {
          //Serial.println("Connected to MQTT broker");
          mqtt_client.subscribe(mqtt_topic.c_str());
          break;
      } else {
          //Serial.println("Failed connecting - retrying in 5 seconds");
          //Serial.println(mqtt_client.state());
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
    sht.readSample();
    doc["temp"] = String(sht.getTemperature(),1);
    doc["umid"] = String(sht.getHumidity(),0);     
    // doc["temp"] = String(20.9,1);
    // doc["umid"] = String(89.55,0); 
    mqtt_client.publish(mqtt_topic.c_str(),doc.as<String>().c_str());
    delay(500);
    if(DEEPSLEEP > 0)
      ESP.deepSleep(DEEPSLEEP);
  }
}

void setup() {
  //Serial.begin(115200);
  Wire.begin(2, 0);  //sda - scl
  sht.init();
  AuthManger am((size_t)0, (size_t)512);

  mqtt_username = am.getUserame();
  mqtt_password = am.getPassword();
  mqtt_topic.concat(mqtt_username);
  
  // Serial.printf("[%s]\n",mqtt_password);
  // Serial.printf("[%s]\n",mqtt_password);

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.autoConnect("TempIgro");
  
  simpleOTA->begin(512, SERVER_ADDRESS, TOKEN_ID, true);
  
  BearSSL::X509List *serverTrustedCA = new BearSSL::X509List(ssl_ca_cert);
  esp_client.setTrustAnchors(serverTrustedCA);
  esp_client.setBufferSizes(512, 512);
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  
  connectToMQTTBroker();
}

void loop() {
  //simpleOTA->checkUpdates(86400);//24 ore
  if (!mqtt_client.connected()) {
      connectToMQTTBroker();
  }else{
    update();
  }
  mqtt_client.loop();
}
