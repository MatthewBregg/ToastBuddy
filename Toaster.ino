#include <SPIFFS.h>

#include <ArduinoJson.h>

#include <ESPAsyncWebServer.h>

#include <Update.h>


#include <WiFi.h>
#include <Button_Debounce.h>
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
const int switchPin = 4;
BasicDebounce trayLoweredSwitch(4,20);
bool magnetOn;

int set_bread_temp = 220;
int read_ambient_temp = 0;
int read_bread_temp = 0;
int test_flux = 0;
String GetHtml() {
  String html = R"(
  
  <!DOCTYPE html> <html>
  <meta content="text/html;charset=utf-8" http-equiv="Content-Type">
  <meta content="utf-8" http-equiv="encoding">
  <head><meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>!!!The ToastBuddy Toastamatic Toaster!!!</title>
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
  <h1> Target Temp: <span id="set_bread_temp"></span></h1>
  <h1> Flux: <span id="flux"></span></h1>
  <p> Magnet Status:  <p id="magnet_status">UNKNOWN</p></p> <a class="button" onclick="handleMagnet();" id="magnet_button">X</a>
  <script>
    setInterval(function() {
      // Call a function repetatively with 2 Second interval
      getData();
    }, 1000); 

    function set_magnet(magnet) {
      let magnetButton = document.getElementById("magnet_button");
      if (magnet) {
            magnetButton.classList.remove('button-on');
            magnetButton.classList.add('button-off');
            magnetButton.innerHTML = "TURN OFF";
            document.getElementById("magnet_status").innerHTML = "ON";
          } else {
            magnetButton.classList.remove('button-off');
            magnetButton.classList.add('button-on');
            magnetButton.innerHTML = "TURN ON";
            document.getElementById("magnet_status").innerHTML = "OFF";
          }
    }
    
    function handleMagnet() {
      let magnetButton = document.getElementById("magnet_button");
      var xhttp = new XMLHttpRequest();
      // Why 4?
      // https://stackoverflow.com/questions/30522565/what-is-meaning-of-xhr-readystate-4/30522680
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          let json = JSON.parse(this.responseText);
          console.log("magnet");  
          console.log(this.responseText);
          let magnet = json["magnet"];
          set_magnet(magnet);
        }
      };
      
      if (magnetButton.classList.contains('button-on')) {
        console.log("Turning magnet on");
        xhttp.open("GET", "set_magnet_ajax_on", true);
      } else {
        console.log("Turning magnet off");
        xhttp.open("GET", "set_magnet_ajax_off", true);
      }
      xhttp.send();
    }

    function handleTemp() {
      let setTemp = document.getElementById("set_temp_number");
      let temp = setTemp.value
      var xhttp = new XMLHttpRequest();
      // Why 4?
      // https://stackoverflow.com/questions/30522565/what-is-meaning-of-xhr-readystate-4/30522680
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          console.log("Ok, changed set temp!");
          let json = JSON.parse(this.responseText);
          let set_bread_temp = json["set_bread_temp"];
          document.getElementById("set_bread_temp").innerHTML = set_bread_temp;
        }
      };
      
      
      console.log("Setting temp");
      console.log(temp);
      let url = "set_temp?set_temp="+temp;
      console.log(url);
      xhttp.open("GET", url, true);
      xhttp.send();
    }
   
    
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
          set_magnet(magnet);
        }
      };
      xhttp.open("GET", "status_json", true);
      xhttp.send();
    }
    getData();
  </script>
  <div>
    <input type="number" name="set_temp" id="set_temp_number">
    <a class="button" onclick="handleTemp();" id="set_temp_button">Set Temp</a>
  </div>
  <div> Version 1.3 </div>
  </body>
  )";
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

bool tempsTooHigh() {
  return read_bread_temp > set_bread_temp || read_ambient_temp > 70;
}

bool cooking = false;
bool started_cooking = 0;
void handleLoweredTray(BasicDebounce* ) {
  if (tempsTooHigh()) {
    // Return and do nothing if the temps are too high
    return;
  }
  digitalWrite(magnetPin,HIGH);
  magnetOn = true;
  cooking = true;
  started_cooking = millis();
}

void handleRaisedTray(BasicDebounce*) {
   cooking = false;
}

void setup(){
  mlx.begin();
  Serial.begin(115200);
  pinMode(led,OUTPUT);
  pinMode(magnetPin,OUTPUT);
  pinMode(switchPin,INPUT_PULLUP);
  digitalWrite(magnetPin,LOW);
  trayLoweredSwitch.set_pressed_command(&handleLoweredTray);
  trayLoweredSwitch.set_released_command(&handleRaisedTray);
  magnetOn = false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Set the hostname: https://github.com/espressif/arduino-esp32/issues/3438
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE); // required to set hostname properly
  WiFi.setHostname("toast-buddy");
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  // We must initialize spiffs in order to read the favicon/any other files.
  // IF WE DON'T INIT SPIFFS any send(SPIFFS,...) will fail mysteriously!
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
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
  server.on("/set_magnet_ajax_off", HTTP_GET, [](AsyncWebServerRequest *request){
    magnetOn = false;
    digitalWrite(magnetPin,LOW);
    request->send(200, "text/plain", getStatusAsJson());
  });
  server.on("/set_magnet_ajax_on", HTTP_GET, [](AsyncWebServerRequest *request){
    magnetOn = true;
    digitalWrite(magnetPin,HIGH);
    request->send(200, "text/plain", getStatusAsJson());
  });
  server.on("/set_temp", HTTP_GET, [](AsyncWebServerRequest *request){
    set_bread_temp = request->getParam("set_temp")->value().toInt();
    request->send(200, "text/plain", getStatusAsJson());
  });

  // Place a 16x16 png in the data folder named favicon.png in order for a favicon to be loader.
  // Then follow the directions on https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/ to flash the flash memory.
  // Favicon!
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.png", "image/png");
  });

    // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });

  // From the WebServers examples list: https://github.com/me-no-dev/ESPAsyncWebServer. 
  // Can export compiled binary for this from Sketch menu (Ctrl-Alt-S).. 
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
      shouldReboot = !Update.hasError();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
      response->addHeader("Connection", "close");
      request->send(response);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
      Serial.printf("Update Start: %s\n", filename.c_str());
      Serial.printf("Update Start: %s\n", filename.c_str());
      if(!Update.begin()) {
        Update.printError(Serial);
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
      }
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
      }
    }
  });

  
  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  server.onNotFound(onRequest);
  server.begin();

 
}

long last_read_temp_at = 0;
long began_toasting_at = 0;
void loop(){
  trayLoweredSwitch.update();
  // Debug, have the onboard LED match this switch.
  digitalWrite(led,trayLoweredSwitch.query());
  // If the bread is hotter than our set point, or the sensor is in danger of overheating, stop cooking!
  if ( millis() - last_read_temp_at > 1000 ) {
    read_ambient_temp = mlx.readAmbientTempC();
    read_bread_temp = mlx.readObjectTempC();
    last_read_temp_at = millis();
    ++test_flux;
  }
  // If we our at our temp limit, override the switch reading and attempt to disengage the magnet!
  if ( tempsTooHigh() ) {
     // Disengage the magnet
     magnetOn = false;
     digitalWrite(magnetPin,LOW);
  } 

  if(shouldReboot){
    // Just in case!
    magnetOn = false;
    digitalWrite(magnetPin,LOW);
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
 
 
}
