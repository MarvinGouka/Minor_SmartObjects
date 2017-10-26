#include <SPI.h>
#include <Wire.h>
#include "MQ135.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "SH1106.h"
#include "SH1106Ui.h"

//Define LCD pins
#define OLED_DC     D4
#define OLED_CS     D8
#define OLED_RESET  D0
#define IR D3 

//Fill in LCD information
SH1106 display(true, OLED_RESET, OLED_DC, OLED_CS); // FOR SPI
SH1106Ui ui     ( &display );

#define ANALOGPIN A0    //  Define Analog PIN on Arduino Board

MQ135 gasSensor = MQ135(ANALOGPIN);
HTTPClient http;

//Information for mobile hotspot and the server. With the token for my acount
const char *ssid = "AndroidAP";
const char *password = "owor8627";
String httpPostUrl = "http://iot-open-server.herokuapp.com/data";
String apiToken = "57f1c91fa0d6835fe0765888";

//Detection for the infrared motion sensor
int detection = HIGH;    // no obstacle

//Empty strings to prevent errors if the client hasn't send anything yet
String humidity = "";
String temperature = "";
String pressure = "";
String altitude = "";
String ppm = "";
String windspeed = "";

// LCD frames config
// =====================================================================

//Create frames for the various data dat will be shown on screen
bool drawFrame1(SH1106 *display, SH1106UiState* state, int x, int y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 34 + y, temperature);

  return false;
}

bool drawFrame2(SH1106 *display, SH1106UiState* state, int x, int y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 34 + y, humidity);

  return false;
}

bool drawFrame3(SH1106 *display, SH1106UiState* state, int x, int y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 34 + y, windspeed);

  return false;
}

//Amount of frames that will be shown
int frameCount = 3;

bool (*frames[])(SH1106 *display, SH1106UiState* state, int x, int y) = { drawFrame1,drawFrame2,drawFrame3 };

// Wifi Config
// =====================================================================

//Server information for connecting the client esp to this one
WiFiServer server(80);
IPAddress ip(192, 168, 43, 78);            // IP address of the server
IPAddress gateway(192, 168, 0, 1);        // gateway of your network
IPAddress subnet(255, 255, 255, 0);       // subnet mask of your network

unsigned long clientTimer = 0;


// =====================================================================

void setup() {
  Serial.begin(115200);

  //Connect to the mobile hotspot data described above.
  connectToWifiNetwork();
  float rzero = gasSensor.getRZero();

  //Set initial LCD settings.
  ui.setTargetFPS(60);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, frameCount);
  ui.init();
  
  display.flipScreenVertically();
  
  pinMode(IR, INPUT);
  pinMode(5, OUTPUT); 
}

void loop() {
  //When detection is low, something is hovering over the sensor
  checkDetection():

  // Get information from the air quality sensor.
  ppm = gasSensor.getPPM();

  // Get the information from the client and update the LCD
  getClientData();

  //Change data to the correct format and send the data to the server
  sendAllData();

  // Check the data from the server to see if the actuator has been triggered.
  getData();

  if (millis() - clientTimer > 30000) {    // stops and restarts the WiFi server after 30 sec
    WiFi.disconnect();                     // idle time
    delay(5000);
    server_start(1);
  }

  delay(1000);
}

void sendAllData(){
   StaticJsonBuffer<600> jsonBuffer;
  JsonObject& jsonRoot = jsonBuffer.createObject();
  
  jsonRoot["token"] = apiToken;
  
  JsonArray& data = jsonRoot.createNestedArray("data");
  
  JsonObject& ppmObject = jsonBuffer.createObject();
  ppmObject["key"] = "ppm";
  ppmObject["value"] = ppm;

    
  JsonObject& humidityObject = jsonBuffer.createObject();
  humidityObject["key"] = "humidity";
  humidityObject["value"] = humidity;

  JsonObject& temperatureObject = jsonBuffer.createObject();
  temperatureObject["key"] = "temperature";
  temperatureObject["value"] = temperature;

  JsonObject& altitudeObject = jsonBuffer.createObject();
  altitudeObject["key"] = "altitude";
  altitudeObject["value"] = altitude;

  JsonObject& pressureObject = jsonBuffer.createObject();
  pressureObject["key"] = "pressure";
  pressureObject["value"] = pressure;

  JsonObject& windObject = jsonBuffer.createObject();
  windObject["key"] = "windspeed";
  windObject["value"] = windspeed;

  data.add(temperatureObject);
  data.add(humidityObject);
  data.add(pressureObject);
  data.add(altitudeObject);
  data.add(windObject);
  data.add(ppmObject);

  String dataToSend;
  jsonRoot.printTo(dataToSend);

  Serial.println(dataToSend);
  
  postData(dataToSend);
}

void getClientData() {
  WiFiClient client = server.available();
  
  if (client) {
    if (client.connected()) {
      Serial.println("in client");            // Dot Dot try connecting
      String request = client.readStringUntil('\r');    // receives the message from the client

      temperature = getValue(request , ';' , 1);       // Set temp to incoming data
      humidity = getValue(request , ';' , 2);        // Set hum to incoming data
      pressure = getValue(request , ';' , 3);       // Set AirPressure to incoming data
      altitude = getValue(request , ';' , 4);       // Set altitude to incoming data
      windspeed = getValue(request , ';' , 5);       // Set altitude to incoming data

      ui.update();      // Update UI of the oled screen;

      client.flush();
    }
    client.stop();                // terminates the connection with the client
    clientTimer = millis();
  }
}

void checkDetection() {
  detection = digitalRead(IR);
  if(detection == LOW){
    // Update the LCD when that happends.
     ui.update();      // Update UI of the oled screen;
  }
}
void connectToWifiNetwork() {
  Serial.println ( "Trying to establish WiFi connection" );
  WiFi.begin ( ssid, password );
  Serial.println ( "" );
  
  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );

//  if ( MDNS.begin ( "esp8266" ) ) {
//    Serial.println ( "MDNS responder started" );
//  }
}

void server_start(byte restart) {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  server.begin();
  delay(500);
  clientTimer = millis();
}

void getData() {
   HTTPClient http;  //Declare an object of class HTTPClient
 
    http.begin("http://iot-open-server.herokuapp.com/actuator/59b25f83f728010004dd6090");  //Specify request destination
    int httpCode = http.GET();                                                                  //Send the request
 
    if (httpCode > 0) { //Check the returning code
      String payload = http.getString();   //Get the request response payload
      Serial.println(payload);                     //Print the response payload
      
      StaticJsonBuffer<300> JSONBuffer;
      JsonObject&  parsed= JSONBuffer.parseObject(payload);
      const char * actuatorTrigger = parsed["trigger"];
      
      if(String(actuatorTrigger) == "true"){
          Serial.println("trigger is true");  
          digitalWrite(5, HIGH);   // turn the LED on (HIGH is the voltage level)
      } else {
          digitalWrite(5, LOW);   // turn the LED on (LOW is the voltage level)
      }
    }
    
    http.end();   //Close connection
}

void postData(String stringToPost) {
  if(WiFi.status()== WL_CONNECTED){
    http.begin(httpPostUrl);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(stringToPost);
    String payload = http.getString();
  
    Serial.println(httpCode);
    Serial.println(payload);
    http.end();
  } else {
    Serial.println("Wifi connection failed, retrying.");   
    connectToWifiNetwork();
  }
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


