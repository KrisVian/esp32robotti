#include <Wire.h>
#include "Grove_I2C_Motor_Driver.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
 
const char* ssid = "R15-CAAC";
const char* password = "nccrg48898";
 
// Backend
const char* BACKEND_URL = "http://192.168.0.212:8000/robotit/";
 
// Laitteen tiedot
const char* DEVICE_NAME = "ESP32_Device";
const char* DEVICE_SERIAL = "ESP32-0001";
 
WebServer server(80);
 
#define I2C_ADDRESS 0x0F   // default Grove I2C motor driver address
 
int currentSpeed = 50;
unsigned long lastLocationSend = 0;
float x = 0.0;
float y = 0.0;
int commandedLeftSpeed = 0;
int commandedRightSpeed = 0;
unsigned long lastMovementUpdate = 0;
const float MOVEMENT_PER_SECOND = 0.20;
 
void updateMovement() {
  unsigned long now = millis();
  unsigned long elapsedMs = now - lastMovementUpdate;
  lastMovementUpdate = now;
 
  if (commandedLeftSpeed != 0 || commandedRightSpeed != 0) {
    float elapsedSeconds = elapsedMs / 1000.0;
    float averageSpeed = (commandedLeftSpeed + commandedRightSpeed) / 200.0;
    if ((commandedLeftSpeed <0 && commandedRightSpeed >0) || (commandedLeftSpeed >0 && commandedRightSpeed <0))
    {
      //eteen tai taakse
      averageSpeed = (commandedLeftSpeed * 2.0) / 200.0;
      x += MOVEMENT_PER_SECOND * averageSpeed * elapsedSeconds;
    }
    else {
      //vasen tai oikea
      y += MOVEMENT_PER_SECOND * averageSpeed * elapsedSeconds;
    }
  }
}
 
void setMotorSpeed(int leftMotorSpeed, int rightMotorSpeed) {
  updateMovement();
  commandedLeftSpeed = leftMotorSpeed;
  commandedRightSpeed = rightMotorSpeed;
  Motor.speed(MOTOR1, leftMotorSpeed);
  Motor.speed(MOTOR2, rightMotorSpeed);
}
 
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
      html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; }
      .button {
        -webkit-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        background-color: #4CAF50;
        border: none;
        color: white;
        padding: 12px 28px;
        text-decoration: none;
        font-size: 26px;
        margin: 1px;
        cursor: pointer;
      }
      .button2 { background-color: #555555; }
    </style>
    <script>
      function moveForward() { fetch('/forward'); }
      function moveLeft() { fetch('/left'); }
      function stopRobot() { fetch('/stop'); }
      function moveRight() { fetch('/right'); }
      function moveReverse() { fetch('/reverse'); }
 
      function updateMotorSpeed(pos) {
        document.getElementById('motorSpeed').innerHTML = pos;
        fetch('/speed?value=' + pos);
      }
    </script>
  </head>
  <body>
    <h1>ESP32 Motor Control</h1>
 
    <p><button class="button" onclick="moveForward()">FORWARD</button></p>
 
    <div style="clear: both;">
      <p>
        <button class="button" onclick="moveLeft()">LEFT</button>
        <button class="button button2" onclick="stopRobot()">STOP</button>
        <button class="button" onclick="moveRight()">RIGHT</button>
      </p>
    </div>
 
    <p><button class="button" onclick="moveReverse()">REVERSE</button></p>
    <p>Motor Speed: <span id="motorSpeed">50</span></p>
    <input type="range" min="0" max="100" step="5" id="motorSlider" oninput="updateMotorSpeed(this.value)" value="50" />
  </body>
  </html>
  )rawliteral";
 
  server.send(200, "text/html", html);
}
 
void handleForward() {
  Serial.println("Forward");
  Serial.println(currentSpeed);
  setMotorSpeed(-currentSpeed, currentSpeed);
  server.send(200);
}
 
void handleReverse() {
  Serial.println("Reverse");
  Serial.println(currentSpeed);
  setMotorSpeed(currentSpeed, -currentSpeed);
  server.send(200);
}
 
void handleLeft() {
  Serial.println("Left");
  Serial.println(currentSpeed);
  setMotorSpeed(-currentSpeed, -currentSpeed * 0.7);
  server.send(200);
}
 
void handleRight() {
  Serial.println("Right");
  Serial.println(currentSpeed);
  setMotorSpeed(currentSpeed, currentSpeed * 0.7);
  server.send(200);
}
 
void handleStop() {
  Serial.println("Stop");
  
  commandedLeftSpeed = 0;
  commandedRightSpeed = 0;
  updateMovement();
  Motor.stop(MOTOR1);
  Motor.stop(MOTOR2);
  server.send(200);
}
 
void handleSpeed() {
  if (server.hasArg("value")) {
    currentSpeed = server.arg("value").toInt();
    currentSpeed = constrain(currentSpeed, 0, 100);
 
    Serial.print("Motor speed set to: ");
    Serial.println(currentSpeed);
 
    if (currentSpeed == 0) {
      updateMovement();
      commandedLeftSpeed = 0;
      commandedRightSpeed = 0;
      Motor.stop(MOTOR1);
      Motor.stop(MOTOR2);
    }
  }
  server.send(200);
}
 
bool sendLocation(float location_x, float location_y) {
 
    if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected.");
    return false;
  }
  HTTPClient http;
  WiFiClient client;
 
  http.begin(client, BACKEND_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Token d5a702ba491d77442611327d13a7903cc634abe7");
  http.setTimeout(1000);
 
  StaticJsonDocument<256> doc;
  doc["location_x"] = location_x;
  doc["location_y"] = location_y;
  doc["modelname"] = DEVICE_NAME;
  doc["serialnumber"] = DEVICE_SERIAL;
 
  String jsonPayload;
  serializeJson(doc, jsonPayload);
 
  Serial.println("Sending POST:");
  Serial.println(jsonPayload);
 
  int httpResponseCode = http.POST(jsonPayload);
 
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response: ");
    Serial.println(httpResponseCode);
 
    //String response = http.getString();
    //Serial.println("Backend response:");
    //Serial.println(response);
 
    http.end();
    return httpResponseCode >= 200 && httpResponseCode < 300;
  }
  else {
    Serial.print("POST failed: ");
    Serial.println(http.errorToString(httpResponseCode));
    http.end();
    return false;
    }
}
 
void setup() {
  Serial.begin(115200);
 
  Motor.begin(I2C_ADDRESS);
 
  Serial.print("Connecting to WiFi");
  Serial.println(ssid);
 
  //WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
 
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 
  server.on("/", handleRoot);
  server.on("/forward", handleForward);
  server.on("/left", handleLeft);
  server.on("/stop", handleStop);
  server.on("/right", handleRight);
  server.on("/reverse", handleReverse);
  server.on("/speed", handleSpeed);
 
  server.begin();
  Serial.println("Web server started");
}
 
void loop() {
  server.handleClient();
  updateMovement();
 
  unsigned long now = millis();
  if (now - lastLocationSend >= 10000) {
    lastLocationSend = now;
 
    bool success = sendLocation(x, y);
    if (success) {
      Serial.println("Location sent successfully!");
    }
  }
}