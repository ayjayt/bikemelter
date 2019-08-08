#include <WiFi.h>

#include <Wire.h>
#include "SSD1306Wire.h"
#include "page.h"

const char* ssid = "Bike Melter";
const char* password = "cultureownsisnotowned";

WiFiServer server(80);

String header;
IPAddress IP;
String melterStatus = "off";

String lOne() {
  return "WIFI: Bike Melter";
}
String lTwo() {
  return "IP: " + String(IP[0]) + "." + String(IP[1]) + "." + String(IP[2]) + "." + String(IP[3]);
}
String lThree() {
  return "Status: " + melterStatus;
}
String lFour() {
  return "Turn off phone data.";
}

String lFive() {
  return "Connect to Bike Melter WiFi.";
}

String lSix() {
  return "Go to http://" + String(IP[0]) + "." + String(IP[1]) + "." + String(IP[2]) + "." + String(IP[3]);
}

SSD1306Wire display(0x3c, 5, 4);

void ReDraw() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, lOne());
  display.drawString(0, 10, lTwo());
  display.drawString(0, 20, lThree());
  display.drawString(0, 30, lFour());
  display.drawString(0, 40, lFive());
  display.drawString(0, 50, lSix());
  display.display();
}

void setup() {
  display.init();
  display.displayOff();
  display.clear();
  display.display();
  display.displayOn();

  
  Serial.begin(115200);
  // put your setup code here, to run once:
  WiFi.softAP(ssid, password);
  if (WiFi.softAPsetHostname("bike.melt")) {
    Serial.println("Set hostname");
  } else {
    Serial.println("Failed to set hostname");
  }
  IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();
  ReDraw();
  }

void loop() {
  // put your main code here, to run repeatedly:
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New Client.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type: text/html");
            client.println("Connection: close");
            client.println();
            // Interpret Controls
            // Print webpage (use xxd)
            client.print((const char*)page_html);
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
