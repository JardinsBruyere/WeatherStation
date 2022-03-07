#include <Arduino.h>

  //http
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>

  //Sensor wiring Libraries
  #include <Wire.h> //I2C
  #include <SPI.h> //SPI
  
  

  #include "SHTSensor.h"
  #include "Adafruit_TCS34725.h"

  //Json lib
  #include <ArduinoJson.h>


//#define SERVER_IP "http://192.168.1.49:5000"
#define SERVER_IP "http://192.168.1.49:5050"//pi ip
//#define SERVER_IP "http://192.168.1.31:5000" //main

#ifndef STASSID
//#define STASSID "ligma"
//#define STAPSK  "ligmaballs"
#define STASSID "CPL wifi 500_9B84CC"
#define STAPSK  "12345678"
#endif


//sensor vars
  
  #define SEALEVELPRESSURE_HPA (1013.25)
  
  
  SHTSensor sht;//saddly, no i2c adress config on this sensor

  /* Initialise with specific int time and gain values */
  Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_1X);


void init_sensors()
{
  Serial.print("Initializing Wire lib...");//for SHT
  Wire.begin();
  Serial.begin(115200);
  delay(1000); // let serial console settle
  Serial.println("OK");

  Serial.print("Initializing SHT...");
  if (sht.init()) {
      Serial.print("init(): success\n");
  } else {
      Serial.print("init(): failed\n");
  }
  sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM); // only supported by SHT3x
  Serial.println("OK");

  Serial.print("Initializing TCS...");
  if (tcs.begin()) {
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }
  Serial.println("OK");
}

String BuildJson()
{
  uint16_t r, g, b, c, colorTemp, lux;
  tcs.getRawData(&r, &g, &b, &c);
  colorTemp = tcs.calculateColorTemperature_dn40(r, g, b, c);
  lux = tcs.calculateLux(r, g, b);

  sht.readSample();

  Serial.print("SHT:\n");
  Serial.print("  RH: ");
  Serial.print(sht.getHumidity(), 2);
  Serial.print("\n");
  Serial.print("  T:  ");
  Serial.print(sht.getTemperature(), 2);
  
  Serial.print("Color Temp: "); Serial.print(colorTemp, DEC); Serial.print(" K - ");
  Serial.print("Lux: "); Serial.print(lux, DEC); Serial.print(" - ");
  Serial.print("R: "); Serial.print(r, DEC); Serial.print(" ");
  Serial.print("G: "); Serial.print(g, DEC); Serial.print(" ");
  Serial.print("B: "); Serial.print(b, DEC); Serial.print(" ");
  Serial.print("C: "); Serial.print(c, DEC); Serial.print(" ");

  
  StaticJsonDocument<1024> json_doc; //KISS
  JsonArray json_sensor_array = json_doc.createNestedArray("sensor_array");
    
  JsonObject json_sensor1 = json_sensor_array.createNestedObject();
  json_sensor1["id"]  = 9;
  json_sensor1["type"]= "humidity";
  json_sensor1["val"] = sht.getHumidity();

  JsonObject json_sensor2 = json_sensor_array.createNestedObject();
  json_sensor2["id"]  = 10;
  json_sensor2["type"]= "temp";
  json_sensor2["val"] = sht.getTemperature();

  JsonObject json_sensor3 = json_sensor_array.createNestedObject();
  json_sensor3["id"]  = 11;
  json_sensor3["type"]= "red";
  json_sensor3["val"] = r;

  JsonObject json_sensor4 = json_sensor_array.createNestedObject();
  json_sensor4["id"]  = 12;
  json_sensor4["type"]= "green";
  json_sensor4["val"] = g;

  JsonObject json_sensor5 = json_sensor_array.createNestedObject();
  json_sensor5["id"]  = 13;
  json_sensor5["type"]= "blue";
  json_sensor5["val"] = b;

  JsonObject json_sensor6 = json_sensor_array.createNestedObject();
  json_sensor6["id"]  = 14;
  json_sensor6["type"]= "c";
  json_sensor6["val"] = c;
  
  JsonObject json_sensor7 = json_sensor_array.createNestedObject();
  json_sensor7["id"]  = 15;
  json_sensor7["type"]= "colorTemp";
  json_sensor7["val"] = colorTemp;

  JsonObject json_sensor8 = json_sensor_array.createNestedObject();
  json_sensor8["id"]  = 16;
  json_sensor8["type"]= "Lux";
  json_sensor8["val"] = lux;

  json_doc["station"]["status"] = "status_ok";
  json_doc["station"]["MAC"] = WiFi.macAddress();

  String output;

  serializeJson(json_doc, output); 

  return output;
}


void setup() {

  Serial.begin(115200);

  init_sensors();

  Serial.println();
  Serial.println();
  Serial.println();

  WiFi.begin(STASSID, STAPSK);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

}

int attempts = 0;

int send_data(String json, String *retString)
{
  int httpCode = 0;
  // wait for WiFi connection
  
  if ((WiFi.status() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    attempts++;
    Serial.printf("[HTTP] attempt : %d\n", attempts);
    
    Serial.print("[HTTP] begin...\n");
    // configure traged server and url
    http.begin(client, SERVER_IP); //HTTP
    http.addHeader("Content-Type", "application/json");

   
    Serial.print("[HTTP] Sending POST request...\n");
    
    // start connection and send HTTP header and body
    httpCode = http.POST(json);
  

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] Received POST code: %d\n", httpCode);
      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        *retString = http.getString();
        
      }
      else
      {
        *retString = http.errorToString(httpCode).c_str();
      }
    }
    http.end();
  }
  return httpCode;
}

void loop() {

  String json = BuildJson();
  String response;
  int httpCode = send_data(json, &response);
  switch(httpCode)
  {
      case HTTP_CODE_OK:
      Serial.print("SUCCESS : received payload: ");
      Serial.println(response);
      break;
      default:
      Serial.print("FAILURE : received payload: ");
      Serial.println(response);
      break;
  }
  
  delay(5000);
}