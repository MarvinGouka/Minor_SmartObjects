#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include "MQ135.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#define ANALOGPIN A0    //  Define Analog PIN on Arduino Board
#define RZERO 206.85    //  Define RZERO Calibration Value
#define DHTPIN 2
#define DHTTYPE DHT22
#define BMP_SCK 13
#define BMP_MISO 12
#define BMP_MOSI 11 
#define BMP_CS 10
#define D0 16

Adafruit_BMP280 bme; // I2C

DHT dht(DHTPIN, DHTTYPE);
HTTPClient http;

//Hotspot information
const char *ssid = "AndroidAP";
const char *password = "owor8627";

//Create empty values, to not cause errors
String dataArray = "";
int windReading = 0;
int rounds = 0;
float windspeed = 0;
const unsigned long oneMinute = 1 * 60 * 1000UL;
static unsigned long lastSampleTime = 0 - oneMinute;

//The IP address of the binnen esp
IPAddress server(192, 168, 43, 78); 
WiFiClient client;

//Url to the server and the token for my account
String httpPostUrl = "http://iot-open-server.herokuapp.com/data";
String apiToken = "57f1c91fa0d6835fe0765888"; 

void setup() {
  Serial.begin(115200);
  pinMode(D0, INPUT);
  
  //Start the barometer
  if (!bme.begin()) {  
      Serial.println("Could not find a valid BMP280 sensor, check wiring!");
      while (1);
    }
  //Start the dht
  dht.begin();

  //Connect to wifi so we can send data to the server esp.
  WiFi.begin(ssid, password);
}

void loop() {
  int state = digitalRead(D0);
  
  unsigned long now = millis();

  //Detects when the wind module does one round, and will keep adding 1 till a minute passes where it will reset.
  detectRounds();

    //Do this once every minute
   if (now - lastSampleTime >= oneMinute)
   {
       //Calculate windspeed
       lastSampleTime += oneMinute;
       windspeed = (2*3,14*70)*rounds; // mm/min (2*pi*straal)
       windspeed = windspeed/1000000; // bereken km/min
       windspeed = windspeed*60; // km/hour
       Serial.println(windspeed);

       //Reset the rounds
       rounds = 0;

       //Get the information fromthe dht and barometer
       float humidity = dht.readHumidity();
       float temperature = dht.readTemperature();
        
       float pressure = bme.readPressure();
       float altitude = bme.readAltitude();
        
       delay(1000);

       //Turn all the values to string so we can send it to the server esp
       String Pressure = String (pressure);
       String Humidity = String (humidity);
       String Temperature = String(temperature);
       String Altitude = String(altitude);
       String Windspeed = String(windspeed);

       // Create StringObject for the server
       dataArray = "{first;" + Temperature + ";" + Humidity + ";" + Pressure + ";" + Altitude + ";" + Windspeed + ";" "}";    

       // Connect to the server esp
       client.connect(server, 80);
       client.println(dataArray);
       String answer = client.readStringUntil('\r');
       client.flush();
       delay(5000);
   }
}

void detectRounds() {
  if (state == LOW) {
      if(windReading != 1){
        rounds +=1;
        Serial.println("New anemometer round");
        Serial.println(rounds);
        delay(500);
      }
      windReading = 1;
    } 
    else {
       windReading = 0;
    }
}

