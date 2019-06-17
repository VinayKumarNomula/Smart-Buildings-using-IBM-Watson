/**
 IBM IoT Foundation managed Device

 Author: Ant Elder
 License: Apache License v2
*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson/releases/tag/v5.0.7
#include "DHT.h"  //U need to add DHT sensor library 
#define DHTPIN D2   // what pin we're connected to 
#define DHTTYPE DHT11   // define type of sensor DHT 11
DHT dht (DHTPIN, DHTTYPE);
//-------- Customise these values -----------
const char* ssid = "SmartBridge WIFI";
const char* password = "SmartBridge";
#define light D0
#define pir D4
#define ac D3
int h;
float t;

int PIR_Status = LOW;
#define ORG "nyapiv"
#define DEVICE_TYPE "building"
#define DEVICE_ID "1994"
#define TOKEN "KQJYf)HgqiZElvQ3aW"

//-------- Customise the above values --------

char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;
//String value;
const char publishTopic[] = "iot-2/evt/building/fmt/json";
const char responseTopic[] = "iotdm-1/response";
const char manageTopic[] = "iotdevice-1/mgmt/manage";
const char updateTopic[] = "iotdm-1/device/update";
const char rebootTopic[] = "iotdm-1/mgmt/initiate/device/reboot";
const char subTopic[] = "iot-2/cmd/surya/fmt/json";
void wifiConnect();
void mqttConnect();
void initManagedDevice();
void publishData();
void handleUpdate(byte* payload) ;
void callback(char* topic, byte* payload, unsigned int payloadLength);
WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

int publishInterval = 5000; // 30 seconds
long lastPublishMillis;
float Idig, Imax, Ipeak, Irms, Isum;
float Iarray[60];


void setup() {
  Serial.begin(115200);

  dht.begin(); 
  Serial.println();
  pinMode(D0,OUTPUT);
  pinMode(D3,OUTPUT);
  pinMode(D4,INPUT);
  wifiConnect();
  mqttConnect();
  initManagedDevice();
}

void loop() {

  
  if (millis() - lastPublishMillis > publishInterval) {
    publishData();
    lastPublishMillis = millis();
  }

  if (!client.loop()) {
    mqttConnect();
    initManagedDevice();
  }
}

void wifiConnect() {
  Serial.print("Connecting to "); Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("nWiFi connected, IP address: "); Serial.println(WiFi.localIP());
}

void mqttConnect() {
  if (!!!client.connected()) {
    Serial.print("Reconnecting MQTT client to "); Serial.println(server);
    while (!!!client.connect(clientId, authMethod, token)) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
  }
}

void initManagedDevice() {
  
  if (client.subscribe("iotdm-1/response")) {
    Serial.println("subscribe to responses OK");
  } else {
    Serial.println("subscribe to responses FAILED");
  }

  if (client.subscribe(rebootTopic)) {
    Serial.println("subscribe to reboot OK");
  } 
  else {
    Serial.println("subscribe to reboot FAILED");
  }

  if (client.subscribe("iotdm-1/device/update")) {
    Serial.println("subscribe to update OK");
  } else {
    Serial.println("subscribe to update FAILED");
  }
  if (client.subscribe(subTopic)) {
    Serial.println("subscribe to subtopic OK");
  } else {
    Serial.println("subscribe to update FAILED");
  }


  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");
  JsonObject& metadata = d.createNestedObject("metadata");
  metadata["publishInterval"] = publishInterval;
  JsonObject& supports = d.createNestedObject("supports");
  supports["deviceActions"] = true;

  char buff[300];
  root.printTo(buff, sizeof(buff));
  Serial.println("publishing device metadata:"); Serial.println(buff);
  if (client.publish(manageTopic, buff)) {
    Serial.println("device Publish ok");
  } else {
    Serial.print("device Publish failed:");
  }
}

void publishData() {
  
    h = dht.readHumidity();
    t = dht.readTemperature();
    PIR_Status = digitalRead(pir);
      
 int i;
for(i=0;i<60;i++){
Idig = analogRead(A0);
if(Idig>511)
{
  Ipeak = ((Idig-511)*5)/(0.185*1023); // Peak value of positive
  Iarray[i] = Ipeak;
}
else Iarray[i] = 0;

delay(10);
}

Ipeak = findMax();
Irms = Ipeak*0.707;
Serial.print("current Value =  ");
Serial.println(Irms);
delay(100);
    
    
  String payload = "{\"d\":{\"Temp\":";
  payload += t;
  payload += ",""\"Humidity\":";
  payload += h;
  payload += ",""\"Current\":";
  payload += Irms;
  payload +=",""\"PIRStatus\":";
  payload +=PIR_Status;
  payload += "}}";

  Serial.print("Sending payload: "); Serial.println(payload);

  if (client.publish(publishTopic, (char*) payload.c_str())) {
    Serial.println("Publish OK");
  } else {
    Serial.println("Publish FAILED");
  }
}
float findMax(){
  Imax = Iarray[0];
  int j;
  for(j=0;j<=60;j++){
  if(Iarray[j] > Imax){ 
  Imax = Iarray[j];
}

}
Serial.print("Imax Value =  ");
Serial.println(Imax);
return Imax;
}


void callback(char* topic, byte* payload, unsigned int payloadLength) {
  Serial.print("callback invoked for topic: "); Serial.println(topic);

  if (strcmp (responseTopic, topic) == 0) {
    return; // just print of response for now
  }

  if (strcmp (rebootTopic, topic) == 0) {
    Serial.println("Rebooting...");
    ESP.restart();
  }

  if (strcmp (updateTopic, topic) == 0) {
    handleUpdate(payload);
  }
  if (strcmp (subTopic, topic) == 0) {
    Serial.print("Subscribed");
    Serial.println((char*)payload);
    handleUpdate(payload);
  }
}

void handleUpdate(byte* payload) {
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if (!root.success()) {
    Serial.println("handleUpdate: payload parse FAILED");
    return;
  }
 String Mode =  root["MODE"];
 String Light =  root["LIGHT"];
 String  AC =  root["AC"];
 Serial.println("MODE:"+ Mode);
 Serial.println("LIGHT:"+Light);
 Serial.println("AC:"+ AC);
 
 
 if(Mode == "ECO" && Light == "LIGHTON" && AC == "ACON")
{
  Serial.println("Light is ON and AC is ON in ECO");
  digitalWrite(D0,HIGH);
  digitalWrite(D3,HIGH); 
  delay(100);
}
if(Mode == "ECO" && Light == "LIGHTOFF" && AC == "ACOFF")
{
  Serial.println("Light is OFF and AC is OFF in ECO");
  digitalWrite(D0,LOW);
  digitalWrite(D3,LOW); 
   delay(100);
}
if(Mode == "AWAY" && Light == "LIGHTOFF" && AC == "ACOFF")
{
  Serial.println("Light is OFF and AC is OFF in AWAY");
  digitalWrite(D0,LOW);
  digitalWrite(D3,LOW); 
  delay(100);
}
if(Mode == "MANUAL" && Light == "LIGHTON")
{
   digitalWrite(D0,HIGH);
   Serial.println("Light is ON in MANUAL");
     delay(100);
}
if(Mode == "MANUAL" && Light == "LIGHTOFF")
{
   digitalWrite(D0,LOW);
   Serial.println("Light is OFF in MANUAL");
     delay(100);
}
if(Mode == "MANUAL" && AC == "ACON")
{
   digitalWrite(D3,HIGH);
   Serial.println("AC is ON in MANUAL");
     delay(100);
}
if(Mode == "MANUAL" && AC == "ACOFF")
{
   digitalWrite(D3,LOW);
   Serial.println("AC is OFF in MANUAL");
     delay(100);
}
if(Mode == "MANUAL" && Light == "LIGHTON" && AC == "ACON")
{
  Serial.println("Light is ON and AC is ON in MANIUAL");
  digitalWrite(D0,HIGH);
  digitalWrite(D3,HIGH); 
  delay(100);
}
if(Mode == "MANUAL" && Light == "LIGHTOFF" && AC == "ACOFF")
{
    Serial.println("Light is OFF and AC is OFF in MANUAL");
  digitalWrite(D0,LOW);
  digitalWrite(D3,LOW); 
  delay(100);
}
 Mode = "";
 Light = "";
 AC = "";



}


