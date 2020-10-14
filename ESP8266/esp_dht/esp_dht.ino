#include <string.h>
#include <math.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include "DHT.h"

#include "WiFiConfig.h"

// https://lastminuteengineers.com/esp8266-dht11-dht22-web-server-tutorial/

// DHT MIN INTERVAL: 2000

// Uncomment one of the lines below for whatever DHT sensor type you're using!
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

#define DHTPIN D4

//#define NOSERIAL
#define SERIAL_BAUD 115200

/* wifi settings */
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

char* remoteAddress = "";

ESP8266WebServer server(80);
               
// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

int poll_period = 4000;
unsigned long poll_time_now = 0;

int send_period = 30000;
unsigned long send_time_now = 0;

#define AVGSET_LEN 15

float TempAverageSet[AVGSET_LEN];
float TempAverage = 0;

float HumiAverageSet[AVGSET_LEN];
float HumiAverage = 0;

float Temperature = 0;
float Humidity = 0;

int ErrorReadNaN = 0;

float AverageOfSet(float valueSet[], int size)
{
    float total = 0;
    for (int x = 0; x < size; x++)
      total += valueSet[x];
    return total/((float)size);
}

void setup()
{
  #ifndef NOSERIAL
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  #endif

  #ifndef NOSERIAL
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  #endif

  // connect to wifi
  WiFi.begin(ssid, password);

  // wait for wifi connection
  while (WiFi.status() != WL_CONNECTED)
    delay(1000);

  #ifndef NOSERIAL
  Serial.print("WiFi Connected - localIP: ");
  Serial.println(WiFi.localIP());
  #endif

  pinMode(DHTPIN, INPUT);
  dht.begin();

  // Fill the average sets on startup
  delay(100);
  Temperature = dht.readTemperature();
  Humidity = dht.readHumidity();
  for (int x = 0; x < AVGSET_LEN; x++)
  {
    TempAverageSet[x] = Temperature;
    HumiAverageSet[x] = Humidity;
  }
  
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();

  #ifndef NOSERIAL
  Serial.println("HTTP server started");
  #endif
}

void loop()
{
  if((unsigned long)(millis() - poll_time_now) > poll_period)
  {
    poll_time_now = millis();

    float readValueTemp = dht.readTemperature();
    if (isnan(readValueTemp))
    {
      readValueTemp = Temperature; // use previous value
      ErrorReadNaN++;
    }
    Temperature = readValueTemp;

    float readValueHumi = dht.readHumidity();
    if (isnan(readValueHumi))
    {
      readValueHumi = Humidity; // use previous value
      ErrorReadNaN++;
    }
    Humidity = readValueHumi;
    
    memmove(&TempAverageSet[1], &TempAverageSet[0], sizeof(float) * (AVGSET_LEN - 1));
    TempAverageSet[0] = Temperature;
    TempAverage = AverageOfSet(&TempAverageSet[0], AVGSET_LEN);

    memmove(&HumiAverageSet[1], &HumiAverageSet[0], sizeof(float) * (AVGSET_LEN - 1));
    HumiAverageSet[0] = Humidity;
    HumiAverage = AverageOfSet(&HumiAverageSet[0], AVGSET_LEN);
  }
  if((unsigned long)(millis() - send_time_now) > send_period)
  {
    send_time_now = millis();
    
    WiFiClient client;
    HTTPClient http;

    String remoteUrl = "";
    remoteUrl += "http://";
    remoteUrl += remoteAddress;
    remoteUrl += "/log";

    String payload = "";
    payload += "{\"temp\":";  
    payload += String(TempAverage, 1);
    payload += "}";
    
    http.begin(client, remoteUrl.c_str());
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload.c_str());

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = http.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
  }
  server.handleClient();
}

void handle_OnConnect()
{
  server.send(200, "text/html", SendHTML(Temperature, TempAverage, Humidity, HumiAverage)); 
}

void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float _temperature, float _avgTemperature, float _humidity, float _avgHumidity)
{
  #ifndef NOSERIAL
  /*for (int x = 0; x < AVGSET_LEN; x++)
  {
    Serial.print(TempAverageSet[x]);
    if (x != (AVGSET_LEN - 1))
      Serial.print(", ");
  }
  Serial.print(" avg: ");
  Serial.print(_avgTemperature);
  Serial.println();*/
  #endif

  /*WiFiClient client;
  HTTPClient http;
  if (http.begin(client, "http://192.168.1.67:8888/now"))
  {
      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();
      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
  }*/
  
  String ptr = "<!DOCTYPE html>\n";
  
  ptr +="<html>\n";
  ptr +="<head>\n";
  ptr +="<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<link href=\"https://fonts.googleapis.com/css2?family=Roboto+Mono&display=swap\" rel=\"stylesheet\">";
  ptr +="<title>Sensor Report</title>\n";
  ptr +="<style>\n";
  ptr +="html{font-family: 'Roboto Mono', monospace; display: block; margin: 0px auto; text-align: center;color: #333333;}\n";
  ptr +="body{margin-top: 50px;}\n";
  //ptr +="h1{margin: 50px auto 30px;}\n";
  ptr +=".side-by-side{display: inline-block;vertical-align: middle;position: relative;}\n";
  ptr +=".humidity-icon{background-color: #3498db;width: 30px;height: 30px;border-radius: 50%;line-height: 36px;margin-right: 10px;}\n";
  ptr +=".humidity-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: auto;text-align: left;}\n";
  ptr +=".humidity{font-weight: 300;font-size: 60px;color: #3498db;}\n";
  ptr +=".temperature-icon{background-color: #f39c12;width: 30px;height: 30px;border-radius: 50%;line-height: 40px;margin-right: 10px;}\n";
  ptr +=".temperature-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: auto;text-align: left;}\n";
  ptr +=".temperature{font-weight: 300;font-size: 60px;color: #f39c12;}\n";
  ptr +=".superscript{font-size: 17px;font-weight: 600;position: absolute;top: 15px;}\n";
  ptr +=".data{padding: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  
  ptr +="<div id=\"webpage\">\n";
  ptr +="<h1>Sensor Report</h1>\n";
  ptr +="<div class=\"data\">\n";
  ptr +="<div class=\"side-by-side temperature-icon\">\n";
  
  ptr +="<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
  ptr +="width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
  ptr +="<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
  ptr +="c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
  ptr +="c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
  ptr +="c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
  ptr +="c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
  ptr +="</svg>\n";
  
  ptr +="</div>\n";
  //ptr +="<div class=\"side-by-side temperature-text\"></div>\n";
  ptr +="<div class=\"side-by-side temperature\">";
  ptr +=String(_temperature, 1);
  ptr +="<span class=\"superscript\">&deg;C</span>";
  ptr +="&nbsp;";
  ptr +=String(_avgTemperature, 1);
  ptr +="<span class=\"superscript\">&deg;Cavg</span>&nbsp;&nbsp;</div>\n";
  ptr +="</div>\n";
  ptr +="<div class=\"data\">\n";
  ptr +="<div class=\"side-by-side humidity-icon\">\n";
  
  ptr +="<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n\"; width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
  ptr +="<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
  ptr +="c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
  ptr +="</svg>\n";
  
  ptr +="</div>\n";
  //ptr +="<div class=\"side-by-side humidity-text\"></div>\n";
  ptr +="<div class=\"side-by-side humidity\">";
  ptr +=String(_humidity, 1);
  ptr +="<span class=\"superscript\">%</span>";
  ptr +="&nbsp;";
  ptr +=String(_avgHumidity, 1);
  ptr +="<span class=\"superscript\">%avg</span>&nbsp;&nbsp;</div>\n";
  ptr +="</div>\n";
  
  ptr +="</div>\n";
  
  ptr +="</body>\n";
  ptr +="</html>\n";
  
  return ptr;
}
