/* 
Anemometer

   Author :             F. Guiet 
   Creation           : 20200216
   Last modification  : 
  
  Version            : 1.0
  History            : 1.0 - First version                       
                       
References :   

Arduino Board used to program : LOLIN (WEMOS) D1 R2 & Mini


Deep sleep mode consumption (Wemos D1 Mini only) : 64 uA
Wake up mode (Wemos D1 Mini only)                : 68 mA

Wemos + hall sensor + voltage divider (without WiFi) : 75 mA
Deep sleep : Wemos + hall sensor + voltage divider : 1.6mA


For deep sleep to work D0 must be connected to RESET 

*/

#include <ArduinoJson.h>
//Light Mqtt library
#include <PubSubClient.h>
//Wifi library
#include <ESP8266WiFi.h>

#define MQTT_SERVER "192.168.1.25"
// WiFi settings
const char* ssid = "DUMBLEDORE";
const char* password = "frederic";

#define MQTT_CLIENT_ID "AnemometerSensor"
#define MAX_RETRY 100

WiFiClient espClient;
PubSubClient client(espClient);

#define DEBUG 1
#define FIRMWARE_VERSION "1.0"

const int VOLTAGE_PIN = A0;
const int SENSOR_PIN = D2;

void ICACHE_RAM_ATTR OnRotation();

// one hour = 3600 s
const int sleepTimeS = 5; //One Hour
volatile long debouncing_time = 15; //Debouncing Time in Milliseconds
volatile unsigned long last_micros;
unsigned long last_millis = 0;
volatile int rpmcount = 0;

bool hasStarted = true;
char message_buff[200];

const int analyseWindSpeedTime = 5000; //5s

struct Sensor {
    String Name;    
    String SensorId;
    String Mqtt_topic;
};

#define SENSORS_COUNT 1
Sensor sensors[SENSORS_COUNT];

void setup() {

 pinMode(SENSOR_PIN, INPUT_PULLUP);  
  
  // Initialize Serial Port
  if (DEBUG)
    Serial.begin(115200);

  InitSensors();
    
  debug_message("Setup completed, starting...",true); 
}

void loop() {

  /*
    float vin = ReadVoltage();
    debug_message("Voltage : " + String(vin,2),true);
    delay(5000);
    return;
  */

  if (hasStarted) {
    hasStarted = false;    
    rpmcount = 0; 
    //Start recording...
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN),OnRotation, RISING);    
    
    debug_message("Recording !!", true);
  }
  
  //Get Wind speed for analyseWindSpeedTime
  if (millis() - last_millis >= analyseWindSpeedTime ) { 
    detachInterrupt(SENSOR_PIN);
    
    debug_message("Rotation within 5s : " + String(rpmcount), true);

    rpmcount = 0;
    
    //Five second between two recording...
    delay(2000);      
    hasStarted = true;
    last_millis = millis() ; //Not useful (will restart)    
  }

  //debug_message("Sending results...", true);
  //SendResult();
  debug_message("Going to sleep...na night", true);
  ESP.deepSleep(sleepTimeS * 1000000);
}

void SendResult() {
   if (WiFi.status() != WL_CONNECTED) {
    if (!connectToWifi())
      return;
  }  

  if (!client.connected()) {
    if (!connectToMqtt()) {
      return;
    }
  }

  float vin = ReadVoltage();
  String mess = ConvertToJSon(String(vin,2));
  debug_message("JSON Sensor : " + mess + ", topic : " +sensors[0].Mqtt_topic, true);
  mess.toCharArray(message_buff, mess.length()+1);
    
  client.publish(sensors[0].Mqtt_topic.c_str(),message_buff);

  disconnectMqtt();
  delay(100);
  disconnectWifi();
  delay(100);
}

void OnRotation() { 

  // debounce the switch contact.
  if((long)(micros() - last_micros) >= debouncing_time * 1000) {
    rpmcount++;
    last_micros = micros();
  }  
}

void InitSensors() {
  
  sensors[0].Name = "Anemometer";
  sensors[0].SensorId = "20";
  sensors[0].Mqtt_topic = "guiet/outside/sensor/20";
}

float ReadVoltage() {

  //AnalogRead = 246 pour 4.2v

  //R1 = 33kOhm
  //R2 = 7.5kOhm

  float sensorValue = 0.0f;  
  
  delay(100); //Tempo so analog reading will be correct!
  sensorValue = analogRead(VOLTAGE_PIN);
  //analog_vcc = sensorValue;
  
  debug_message("Analog Reading : " + String(sensorValue,2), true);

  return (sensorValue * 4.2) / 246;

}

String ConvertToJSon(String battery) {
    //Create JSon object
    DynamicJsonDocument  jsonBuffer(200);
    JsonObject root = jsonBuffer.to<JsonObject>();
    
    root["id"] = sensors[0].SensorId;
    root["name"] = sensors[0].Name;
    root["firmware"]  = FIRMWARE_VERSION;
    root["battery"] = battery;
    //Rotation per minute
    //root["rpm"] = liter;    
    //root["cft"] = String(literConsumedFromStart);
    
    String result;    
    serializeJson(root, result);

    return result;
}

boolean connectToMqtt() {

   client.setServer(MQTT_SERVER, 1883); 

  int retry = 0;
  // Loop until we're reconnected
  while (!client.connected() && retry < MAX_RETRY) {
    debug_message("Attempting MQTT connection...", true);
    
    if (client.connect(MQTT_CLIENT_ID)) {
      debug_message("connected to MQTT Broker...", true);
    } else {
      retry++;
      // Wait 5 seconds before retrying
      delay(500);
      //yield();
    }
  }

  if (retry >= MAX_RETRY) {
    debug_message("MQTT connection failed...", true);  
    return false;
  }

  return true;
}

boolean connectToWifi() {

  // WiFi.forceSleepWake();
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(20.5); // this sets wifi to highest power 
  
  int retry = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && retry < MAX_RETRY) {
    retry++;
    delay(500);
    debug_message(".", false);
  }

  if (WiFi.status() == WL_CONNECTED) {  
     debug_message("WiFi connected", true);  
     // Print the IP address
     if (DEBUG) {
      Serial.println(WiFi.localIP());
     }
     
     return true;
  } else {
    debug_message("WiFi connection failed...", true);   
    return false;
  }  
}

void disconnectMqtt() {
  debug_message("Disconnecting from mqtt...", true);
  client.disconnect();
}

void disconnectWifi() {
  debug_message("Disconnecting from wifi...", true);
  WiFi.disconnect();
}

void debug_message(String message, bool doReturnLine) {
  if (DEBUG) {
    if (doReturnLine)
      Serial.println(message);
    else
      Serial.print(message);
  }
}
