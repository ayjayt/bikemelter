#include <WiFi.h>

#include <Wire.h>
#include "SSD1306Wire.h"
#include "page.h"

const char* ssid = "Bike Melter";
const char* password = "cultureownsisnotowned";
WiFiServer server(80);
String header;
IPAddress IP;

const int speedPinA = 16;
const int directionPinA = 2;
const int speedPinB = 14;
const int directionPinB = 12;
const int freq = 5000;
const int channelA = 0; // PWM connects to GPIO via "Channel"
const int channelB = 1;
const int resolution = 8;

volatile int interruptCounter;
int totalInterruptCounter;
int oldCounter;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}


String melterStatus = "finished";
bool paused = true;
String OnInterval = "10", OffInterval = "1", RepeatCount = "60";
int onint = OnInterval.toInt(), offint = OffInterval.toInt(), repint = RepeatCount.toInt();
int onCounter = 0, offCounter = 0, repeatCounter = 0;

void setOn() {
  if (melterStatus == "on") return;
  if (melterStatus != "paused") {
    setReset();
    onCounter = onint;
    repeatCounter = repint;
    portENTER_CRITICAL(&timerMux);
    interruptCounter = 0;
    portEXIT_CRITICAL(&timerMux);
  } else {
    portENTER_CRITICAL(&timerMux);
    interruptCounter = oldCounter;
    portEXIT_CRITICAL(&timerMux);
  }
  paused = false;
  melterStatus = "on";
  
  // Turn On
  Serial.println("Turning on");
  ReDraw();
}
void setPause() {
  paused = true;
  melterStatus = "paused";

  oldCounter = interruptCounter;
  // Stop Flow
  Serial.println("Pausing");
  ReDraw();
}
void setReset() {
  paused = true;
  melterStatus = "off";
  
  onint = OnInterval.toInt();
  offint = OffInterval.toInt();
  repint = RepeatCount.toInt();
  onCounter = 0;
  offCounter = 0;
  repeatCounter = 0;

  Serial.println("Reset and paused");
  ReDraw();
}

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
void DrawErr(String err) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, err);
  display.drawString(0, 20, "Please restart");
  display.display();
  while(true);
}
void motorsOn() {
  ledcWrite(channelA, 200);
  ledcWrite(channelB, 200);
}

void motorsOff() {
  ledcWrite(channelA, 0);
  ledcWrite(channelB, 0);
}

void motorSpeed(uint8_t speed) {
  Serial.print("Speed: ");
  Serial.println(String(speed));
  ledcWrite(channelA, speed);
  ledcWrite(channelB, speed);
}
void setup() {
  ledcSetup(channelA, freq, resolution);
  ledcSetup(channelB, freq, resolution);
  ledcAttachPin(speedPinA, channelA);
  ledcAttachPin(speedPinB, channelB);
  ledcWrite(channelA, 0);
  ledcWrite(channelB, 0);
  
  display.init();
  display.displayOff();
  display.clear();
  display.flipScreenVertically();
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

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);
}

void loop() {
  if (interruptCounter >= 60) {
      portENTER_CRITICAL(&timerMux);
      interruptCounter -= 60;
      portEXIT_CRITICAL(&timerMux);
      Serial.print(".");
      totalInterruptCounter++;
      if (!paused) {
        if (onCounter > 0) {
          Serial.println("onCounter = " + String(onCounter));
          if (!--onCounter) {
            Serial.println("repeatCounter = " + String(repeatCounter));
            if (--repeatCounter) {
              // Turning off.
              Serial.println("Turning off");
              offCounter = offint;
            } else {
              Serial.println("Finished...");
              paused = true;
              melterStatus = "finished";
            }
          }
        } else if (offCounter > 0) {
          Serial.println("offCounter = " + String(offCounter));
          if (!--offCounter) {
            Serial.println("Turning on");
            // Turning on.
            onCounter = onint;
          }
        }
      }
  }
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
            if (header.indexOf("GET /currentProgram") >= 0) {
              client.println(String(onint));
              client.println(String(offint));
              client.print(String(repint));
              break;
            } else if (header.indexOf("GET /nextProgram") >= 0) {
              client.println(OnInterval);
              client.println(OffInterval);
              client.print(RepeatCount);
              break;
            } else if (header.indexOf("GET /details") >= 0) {
              client.println(String(onCounter));
              client.println(String(offCounter));
              client.print(String(repeatCounter));
              break;
            } else if (header.indexOf("GET /status") >= 0) {
              client.print(melterStatus);
              break;
            } else if (header.indexOf("POST /speed") >= 0) {
              char number[4];
              memcpy(number, &header[header.indexOf("POST /speed")+ 14], 3);
              number[4] = '\0';
              motorSpeed(String(number).toInt());
            } else if (header.indexOf("POST /on") >= 0) {
              setOn();
              break;
            } else if (header.indexOf("POST /pause") >= 0) {
              setPause();
              break;
            } else if (header.indexOf("POST /reset") >= 0) {
              setReset();
              break;
            } else if (header.indexOf("POST /submit") >= 0) {
               char onString[20], offString[20], repeatString[20];
               char *statePtr = onString;
               bool done = false;
               int localCounter = 0;
               for (int i = 0+header.indexOf("POST /submit")+13; !done && i < header.length(); i++ ) {
                if (header[i] == '&' || header[i] == ' ') {  
                  statePtr[localCounter] = 0;
                  if (statePtr == onString) {
                    Serial.println("Onstring:");
                    Serial.println(statePtr);
                    OnInterval = String(statePtr).substring(3);
                    statePtr = offString;
                  } else if (statePtr == offString) {
                    Serial.println("Offstring:");
                    Serial.println(statePtr);
                    OffInterval = String(statePtr).substring(4);
                    statePtr = repeatString;
                  } else {
                    Serial.println("Repeatstring:");
                    Serial.println(statePtr);
                    RepeatCount = String(statePtr).substring(7);
                    done = true;
                    
                  }
                  localCounter = 0;
                  continue;
                }
                statePtr[localCounter++] = header[i];
                if (localCounter > 20) {
                  DrawErr("Number too big");
                }
               }
               
              break;
            }
            client.write((const char*)page_html, page_html_len);
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
