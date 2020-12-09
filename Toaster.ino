#include <ArduinoJson.h>

#include <ESPAsyncWebServer.h>


#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include "/home/bregg/wifi.h"
// In the above wifi.h file, place 
//  const char* ssid = "SSID";
//  const char* password = "PASS";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

const char* http_username = "admin";
const char* http_password = "admin";

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

 
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

const int led = 2;
const int magnetPin = 13;
bool magnetOn = true;

int set_bread_temp = 220;
int read_ambient_temp = 0;
int read_bread_temp = 0;
int test_flux = 0;
String GetHtml() {
  String html = R"(
  
  <!DOCTYPE html> <html>
  <head><meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>Toastamatic!</title>
  <style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
  body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}
  .button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}
  .button-on {background-color: #3498db;}
  .button-on:active {background-color: #2980b9;}
  .button-off {background-color: #34495e;}
  .button-off:active {background-color: #2c3e50;}
  p {font-size: 14px;color: #888;margin-bottom: 10px;}
  </style>
  </head>
  <body>
  <h1> Ambient Temp: <span id="ambient_temp"></span></h1>
  <h1> Bread Temp: <span id="bread_temp"></span></h1>
  <h1> Bread Temp: <span id="set_bread_temp"></span></h1>
  <h1> Flux: <span id="flux"></span></h1>
  <script>
    setInterval(function() {
      // Call a function repetatively with 2 Second interval
      getData();
    }, 1000); //2000mSeconds update rate
    
    function getData() {
      var xhttp = new XMLHttpRequest();
      // Why 4?
      // https://stackoverflow.com/questions/30522565/what-is-meaning-of-xhr-readystate-4/30522680
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          let json = JSON.parse(this.responseText);
          console.log(this.responseText);
          let magnet = json["magnet"];
          let flux = json["flux"];
          let bread_temp = json["bread"];
          let set_bread_temp = json["set_bread_temp"];
          let ambient_temp = json["ambient"];
          document.getElementById("bread_temp").innerHTML = bread_temp;
          document.getElementById("set_bread_temp").innerHTML = set_bread_temp;
          document.getElementById("ambient_temp").innerHTML = ambient_temp;
          document.getElementById("flux").innerHTML = flux;
        }
      };
      xhttp.open("GET", "status_json", true);
      xhttp.send();
    }
  </script>
  <form action="/set_temp" method="GET">
    Set Bread Temp: <input type="text" name="set_temp">
    <input type="submit" value="Submit">
  </form><br>
  
  )";
  if (magnetOn) {
    html += R"(
      <p>Magnet Status: ON</p><a class="button button-off" href="magnetoff">OFF</a>
    )";
  } else {
    html += R"(
      <p>Magnet Status: OFF</p><a class="button button-on" href="magneton">ON</a>
    )";
  }
  html += "</body>";
  return html;
}
void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

String getStatusAsJson() {
  const int capacity= JSON_OBJECT_SIZE(5);
  StaticJsonDocument<capacity> doc;
  doc["magnet"] = magnetOn;
  doc["bread"] = read_bread_temp;
  doc["ambient"] = read_ambient_temp;
  doc["flux"] = test_flux;
  doc["set_bread_temp"] = set_bread_temp;
  String output;
  serializeJson(doc, output);
  return output;
}

void setup(){
  mlx.begin();
  Serial.begin(115200);
  pinMode(magnetPin,OUTPUT);
  digitalWrite(magnetPin,HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  // attach AsyncEventSource
  server.addHandler(&events);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", GetHtml());
  });
  server.on("/status_json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getStatusAsJson());
  });
  // respond to GET requests on URL $1....
  server.on("/magnetoff", HTTP_GET, [](AsyncWebServerRequest *request){
    magnetOn = false;
    digitalWrite(magnetPin,LOW);
    request->send(200, "text/html", GetHtml());
  });
  server.on("/magneton", HTTP_GET, [](AsyncWebServerRequest *request){
    magnetOn = true;
    digitalWrite(magnetPin,HIGH);
    request->send(200, "text/html", GetHtml());
  });
  server.on("/set_temp", HTTP_GET, [](AsyncWebServerRequest *request){
    set_bread_temp = request->getParam("set_temp")->value().toInt();
    request->send(200, "text/html", GetHtml());
  });
  

  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  server.onNotFound(onRequest);
  server.begin();
}
long last_read_temp_at = 0;
void loop(){
  // If the bread is hotter than our set point, or the sensor is in danger of overheating, stop cooking!
  if ( millis() - last_read_temp_at > 1000 ) {
    read_ambient_temp = mlx.readAmbientTempC();
    read_bread_temp = mlx.readObjectTempC();
    last_read_temp_at = millis();
    ++test_flux;
  }
  if ( read_bread_temp > set_bread_temp || read_ambient_temp > 70 ) {
     // Disengage the magnet
     magnetOn = false;
     digitalWrite(magnetPin,LOW);
  }
 
}
