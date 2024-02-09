#include <WiFi.h>  // lib
#include <HTTPClient.h>  // lib
#include <EEPROM.h>  // lib

const char server[] = "api.thingspeak.com";  //thingspeak server 
const char apiKey[] = "Enter your apikey";  // thingspeak api
const char ssid[] = "Enter your ssid";  // wifi user name
const char* password = "Enter your password"; //  wifi password

const byte buttonPins[] = {"enter your button pins"};  //4 pin 
const byte resetButtonPins[] = {"enter your button pins"};  // 4 pin
const byte counterLedPins[] = {"enter your button pins"};  // 4 pin
const byte wifiLedPin = "enter your button pins";  // 1 pin

const int numFields = 4; 

int counters[] = {0, 0, 0, 0};
bool buttonStates[4] = {false, false, false, false};
bool lastButtonStates[4] = {false, false, false, false};
bool resetButtonStates[4] = {false, false, false, false};
bool lastResetButtonStates[4] = {false, false, false, false};

bool wifiConnected = false;  // check the wifi is connected or not 
bool isPowerOn = false; // LED turn for power on

const unsigned long debounceDelay = 1;

const int counterSize = sizeof(counters); // Placed counterSize here after 'counters' array declaration


void connectToWiFi() {  //  wifi auto Reconnecting
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi!");
}

void saveCountersToEEPROM() {  // save the data in the eprom to recovery and the update
  EEPROM.put(0, counters);
  EEPROM.write(counterSize, isPowerOn ? 1 : 0);
  EEPROM.commit();
}

void readCountersFromEEPROM() {  // read from eeprom
  EEPROM.get(0, counters);
}

bool sendCountersToThingSpeak() {  //  Sending value to thingspeak
  WiFiClient client;

  String url = "http://";
  url += server;
  url += "/update?api_key=";
  url += apiKey;

  for (int i = 0; i < numFields; i++) {
    url += "&field";
    url += String(i + 1);
    url += "=";
    url += String(counters[i]);
  }

  HTTPClient http;
  http.begin(client, url);

  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    Serial.println("Data sent to ThingSpeak successfully!");
    http.end();
    return true;
  } else {
    Serial.print("Error sending data to ThingSpeak. HTTP error code: ");  //  MQQT protocol
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}

void resetCounter(int index) {  // Reset the counter 
  counters[index] = 0;
  saveCountersToEEPROM();
  sendCountersToThingSpeak();
}

void wifiTask(void* parameter) {  // Led turn OFF when wifi is connected 
  pinMode(wifiLedPin, OUTPUT);

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Disconnected from Wi-Fi. Reconnecting...");
      digitalWrite(wifiLedPin, HIGH);  // Led turn ON when wifi is not connected 
      saveCountersToEEPROM();
      connectToWiFi();
      readCountersFromEEPROM();
    } else {
      wifiConnected = true;
      digitalWrite(wifiLedPin, LOW);

      if (!isPowerOn) {
        readCountersFromEEPROM();
        sendCountersToThingSpeak();
        isPowerOn = true;
        saveCountersToEEPROM();
      } else {
        sendCountersToThingSpeak();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void counterTask(void* parameter) {  // Sending detail to the thingspeak field
  for (;;) {
    for (int i = 0; i < numFields; i++) {
      buttonStates[i] = digitalRead(buttonPins[i]);
      resetButtonStates[i] = digitalRead(resetButtonPins[i]);

      if (isPowerOn && buttonStates[i] != lastButtonStates[i]) {
        if (buttonStates[i] == HIGH) {
          counters[i]++;
          saveCountersToEEPROM();
          Serial.print("Field ");
          Serial.print(i + 1);
          Serial.print(" Value: ");
          Serial.println(counters[i]);
          digitalWrite(counterLedPins[i], HIGH);
            delay(10);
            digitalWrite(counterLedPins[i], LOW);
          if (wifiConnected) {
            sendCountersToThingSpeak();
          }
        }
        delay(debounceDelay);
      }

      if (resetButtonStates[i] != lastResetButtonStates[i]) {
        if (resetButtonStates[i] == LOW) {
          resetCounter(i);
          Serial.print("Field ");
          Serial.print(i + 1);
          Serial.println(" Reset");
        }
        delay(debounceDelay);
      }

      lastButtonStates[i] = buttonStates[i];
      lastResetButtonStates[i] = resetButtonStates[i];
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < numFields; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    pinMode(resetButtonPins[i], INPUT_PULLUP);
    pinMode(counterLedPins[i], OUTPUT);
  }

  EEPROM.begin(counterSize);

  byte powerFlag = EEPROM.read(counterSize);
  if (powerFlag == 1) {
    isPowerOn = true;
  } else {
    isPowerOn = false;
  }

  readCountersFromEEPROM();

  xTaskCreatePinnedToCore(
    counterTask,
    "CounterTask",
    4096,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    wifiTask,
    "WifiTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  // Empty loop
}
