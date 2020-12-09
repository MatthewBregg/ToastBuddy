#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "/home/bregg/wifi.h"
// In the above wifi.h file, place 
//  const char* ssid = "SSID";
//  const char* password = "PASS";


WebServer server(80);

const int led = 2;
const int magnetPin = 13;
bool magnetOn = false;

void handleRoot() {
  digitalWrite(led, 1);
  
  server.send(200, "text/html", GetHtml());
  digitalWrite(led, 0);
}

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

void handleMagnetOn() {
   magnetOn = true;
   digitalWrite(magnetPin,HIGH);
   handleRoot();
}
void handleMagnetOff() {
   magnetOn = false;
   digitalWrite(magnetPin,LOW);
   handleRoot();
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void) {
  pinMode(magnetPin,OUTPUT);
  digitalWrite(magnetPin,LOW);
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/magneton", handleMagnetOn);
  server.on("/magnetoff", handleMagnetOff);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
}
