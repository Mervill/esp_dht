#include <string.h>
#include <math.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include "DHT.h"

#include "WiFiConfig.h"
#include "DeployConfig.h"

// DHT MIN INTERVAL: 2000

//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

#define DHTPIN D4

//#define NOSERIAL
#define SERIAL_BAUD 115200

/* wifi settings */
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

char* remoteAddress = DEPLOY_REMOTE_ADDDR;
char* sensorNickname = DEPLOY_SENSOR_NICKNAME;

ESP8266WebServer server(80);
               
// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

int poll_period = 4000;
unsigned long poll_time_now = 0;

int send_period = 30000;
unsigned long send_time_now = 0;

float Temperature = 0;
float Humidity = 0;

int ErrorReadNaN = 0;

void setup()
{
  #ifndef NOSERIAL
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  Serial.println();
  Serial.println(WiFi.macAddress());
  Serial.print("WiFi Connecting to ");
  Serial.println(ssid);
  #endif

  // connect to wifi
  WiFi.begin(ssid, password);

  // wait for wifi connection
  while (WiFi.status() != WL_CONNECTED)
    delay(1000);

  #ifndef NOSERIAL
  Serial.print("WiFi Connected as ");
  Serial.print(WiFi.hostname());
  Serial.print(" @ ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi RSSI: ");
  Serial.println(WiFi.RSSI());
  #endif

  dht.begin();
  delay(100);
  Temperature = dht.readTemperature();
  Humidity = dht.readHumidity();

  server.on("/", http_Index);
  server.on("/whois", http_WhoIs);
  server.onNotFound(http_NotFound);
  server.begin();

  #ifndef NOSERIAL
  Serial.println("HTTP Server started");
  
  Serial.println("setup() complete");
  #endif
}

void loop()
{
  if((unsigned long)(millis() - poll_time_now) > poll_period)
  {
    poll_time_now = millis();
    Loop_PollDHT();
  }
  
  if((unsigned long)(millis() - send_time_now) > send_period)
  {
    send_time_now = millis();
    Loop_SendData();
  }
  
  server.handleClient();
}

void Loop_PollDHT()
{
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
}

void Loop_SendData()
{
  String payload = "";
  payload += "{";

  payload += "\"nick\":";
  payload += "\"";
  payload += sensorNickname;
  payload += "\"";
  payload += ",";

  /*payload += "\"mac\":";
  payload += "\"";
  payload += WiFi.macAddress();
  payload += "\"";
  payload += ",";*/
  
  payload += "\"temp\":";
  payload += String(Temperature, 1);
  payload += ",";

  payload += "\"humi\":";
  payload += String(Humidity, 1);
  payload += ",";

  payload += "\"rssi\":";
  payload += String(WiFi.RSSI());
  
  payload += "}";

  #ifndef NOSERIAL
  // json over serial, what sins hath we wrought in pursuit of our madness?
  Serial.println(payload);
  #endif

  WiFiClient client;
  HTTPClient http;

  String remoteUrl = "";
  remoteUrl += "http://";
  remoteUrl += remoteAddress;
  remoteUrl += "/log";
  
  http.begin(client, remoteUrl.c_str());
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload.c_str());

  // httpCode will be negative on error
  if (httpCode > 0)
  {
    #ifndef NOSERIAL
    Serial.printf("[HTTP] POST %d\n", httpCode);
    #endif
    // file found at server
    /*if (httpCode == HTTP_CODE_OK) {
      const String& payload = http.getString();
      Serial.println("received payload:\n<<");
      Serial.println(payload);
      Serial.println(">>");
    }*/
  }
  else
  {
    #ifndef NOSERIAL
    Serial.printf("[HTTP] POST %d %s\n", httpCode, http.errorToString(httpCode).c_str());
    #endif
  }
  
  http.end();
}

void http_NotFound()
{
  server.send(404, "text/plain", "404 Not Found");
}

void http_Index()
{
  String tempString = String(Temperature, 1);
  String payload = "<!DOCTYPE html>\n";
  
  payload += "<html>\n";
  payload += "<head lang=\"en\">\n";
  payload += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\n";
  payload += "<meta http-equiv=\"refresh\" content=\"60\">";
  payload += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  payload += "<link href=\"https://fonts.googleapis.com/css2?family=Roboto+Mono&display=swap\" rel=\"stylesheet\">";
  payload += "<title>";
  payload += tempString;
  payload += "C | ";
  payload += sensorNickname;
  payload += "</title>\n";
  payload += "<style>\n";
  payload += "html{font-family: 'Roboto Mono', monospace; display: block; margin: 0px auto; text-align: center;color: #333333;}\n";
  payload += "body{margin-top: 50px;}\n";
  //payload += "h1{margin: 50px auto 30px;}\n";
  payload += ".side-by-side{display: inline-block;vertical-align: middle;position: relative;}\n";
  payload += ".humidity-icon{background-color: #3498db;width: 30px;height: 30px;border-radius: 50%;line-height: 36px;margin-right: 10px;}\n";
  payload += ".humidity-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: auto;text-align: left;}\n";
  payload += ".humidity{font-weight: 300;font-size: 60px;color: #3498db;}\n";
  payload += ".temperature-icon{background-color: #f39c12;width: 30px;height: 30px;border-radius: 50%;line-height: 40px;margin-right: 10px;}\n";
  payload += ".temperature-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: auto;text-align: left;}\n";
  payload += ".temperature{font-weight: 300;font-size: 60px;color: #f39c12;}\n";
  payload += ".superscript{font-size: 17px;font-weight: 600;position: absolute;top: 15px;}\n";
  payload += ".data{padding: 10px;}\n";
  payload += "</style>\n";
  payload += "</head>\n";
  payload += "<body>\n";
  payload += "<div id=\"webpage\">\n"; // webpage
  
  payload += "<h1 style=\"margin:0;\">";
  payload += sensorNickname;
  payload += "</h1>\n";
  payload += "<h2 style=\"margin:0;\">DHT Sensor Report</h2>";
  payload += "<div class=\"data\">\n";
  
  payload += "<div class=\"side-by-side temperature-icon\">\n";
  payload += "<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
  payload += "width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
  payload += "<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
  payload += "c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
  payload += "c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
  payload += "c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
  payload += "c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
  payload += "</svg>\n";
  payload += "</div>\n";
  
  payload += "<div class=\"side-by-side temperature\">";
  payload += tempString;
  payload += "<span class=\"superscript\">&deg;C</span>";
  payload += "</div>\n";

  payload += "</div>\n"; // data
  payload += "<div class=\"data\">\n";
  
  payload += "<div class=\"side-by-side humidity-icon\">\n";
  payload += "<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
  payload +=  "width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
  payload += "<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
  payload += "c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
  payload += "</svg>\n";
  payload += "</div>\n";
  
  payload += "<div class=\"side-by-side humidity\">";
  payload += String(Humidity, 1);
  payload += "<span class=\"superscript\">%</span>";
  payload += "</div>\n";

  payload += "</div>\n"; // data

  payload += "<div>";
  //payload +=  "<span>";
  payload += String(WiFi.RSSI());
  payload += "&nbsp;";
  payload += String((float)millis()/1000.0f);
  payload += "&nbsp;";
  payload += String(ErrorReadNaN);
  //payload += "</span>";
  payload += "</div>";
  
  payload += "</div>\n"; // webpage
  payload += "</body>\n";
  payload += "</html>\n";
  
  server.send(200, "text/html", payload); 
}

void http_WhoIs()
{
  String payload = "";
  payload += "{";

  payload += "\"nickname\":";
  payload += "\"";
  payload += sensorNickname;
  payload += "\"";
  payload += ",";

  payload += "\"hostname\":";
  payload += "\"";
  payload += WiFi.hostname();
  payload += "\"";
  payload += ",";
  
  payload += "\"mac\":";
  payload += "\"";
  payload += WiFi.macAddress();
  payload += "\"";
  payload += ",";

  payload += "\"RSSI\":";
  payload += String(WiFi.RSSI());
  payload += ",";

  payload += "\"millis\":";
  payload += String(millis());
  payload += ",";

  payload += "\"ErrorReadNaN\":";
  payload += String(ErrorReadNaN);
  payload += ",";

  payload += "\"Temperature\":";
  payload += String(Temperature);
  payload += ",";

  payload += "\"Humidity\":";
  payload += String(Humidity);
  
  payload += "}";

  server.send(200, "application/json", payload);
}
