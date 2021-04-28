#include <WiFi.h>

#include "credentials.h"
#include "OTA.h"
#include "telegramBot.h"

const char *ssid     = mySSID;
const char *password = myPASSWORD;

void setup() {
  // Start serial port for debugging
  Serial.begin(115200);
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Print local IP address
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup Telegram bot
  setupTelegramBot();


  // Setup OTA
  ArduinoOTA.setHostname("MouseTrap");
  otaSetup();

}

void loop() {
  // Check OTA
  ArduinoOTA.handle();
//  sendTelegramMessage();
}
