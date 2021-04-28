#include <WiFi.h>

#include "credentials.h"
#include "OTA.h"
#include "telegramBot.h"

const char *ssid     = mySSID;
const char *password = myPASSWORD;

const int uS_TO_S_FACTOR = 1000000; /* Conversion factor for micro seconds to seconds */ 
const int TIME_TO_SLEEP = 60; /* Time ESP32 will go to sleep (in seconds) */

const int AWAKE_TIME = 60;  // seconds
int loopCounter = AWAKE_TIME;

const int BUTTON_PIN_BITMASK = 0x200000000; // 2^33 in hex

void setup() {
  // Start serial port for debugging
  Serial.begin(115200);

  print_wakeup_reason();

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

  // Setup wekeup sources
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_32,1); //1 = High, 0 = Low
  esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
  sendTelegramMessage();
}

void loop() {
  // Check OTA
  ArduinoOTA.handle();

  if (stayAwake() == false) {
    Serial.println("Start to sleep");
    esp_deep_sleep_start();
  }
}

bool stayAwake(void) {
  if (loopCounter > 0) {
    loopCounter--;
    delay(1000);
    return true;
  } else {
    return false;
  }
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason){
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}
