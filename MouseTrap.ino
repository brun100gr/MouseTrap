#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

#include "credentials.h"
#include "OTA.h"
#include "telegramBot.h"
#include "telnetDebug.h"
#include "rootCACertificate.h"

const char *ssid     = mySSID;
const char *password = myPASSWORD;

const uint32_t uS_TO_S_FACTOR = 1000000; /* Conversion factor for micro seconds to seconds */ 
const int32_t SLEEP_ONE_DAY = 86400; // 24 * 60 * 60 (1 day) Time ESP32 will go to sleep (in seconds)

// Time zone constants
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// Version SW
const String currentSWVersion = "0.1";

// How many times connection to AP is attempted
const uint32_t CONNECTTION_ATTEMPTS_LIMIT = 20;

// Daily wakeup time
const int32_t WAKEUP_TIME = 19*3600 + 0*60;	// 7:00 PM

// UpTime before entering in sleep mode
const uint32_t UPTIME_BEFORE_SLEEP = 5*60;	// 5 minutes

//Pushbuttons connected to GPIO32 & GPIO33
uint64_t const BUTTON_PIN_BITMASK_GPIO32 = 0x100000000;	// Manual trigger
uint64_t const BUTTON_PIN_BITMASK_GPIO33 = 0x200000000;	// Trap trigger

// Wakeup reasons
enum wakeUpReasonsEnum {
  GPIO_MANUAL,
  GPIO_TRAP,
  GPIO_UNEXPECTED,
  TIMER,
  OTHER,
  MAX
};

// Battery monitor is connected to GPIO 34 (Analog ADC1_CH6) 
const int batteryMonitorPin = 34;

// Number of ADC samples to be discarded to stabilize ADC
const int NUMBER_OF_SAMPLES = 3;

// Save Wakeup reason in this variable
wakeUpReasonsEnum wakeUpReasons = wakeUpReasonsEnum::MAX;

// Save raw ADC value for battery voltage
uint32_t batteryMonitorValue;

// Set time via NTP, as required for x.509 validation
void setClock() {
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");  // Rome

  Serial.print(F("Waiting for NTP time sync: "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    yield();
    delay(500);
    Serial.print(F("."));
    debugA("Connecting to NTP Server.");
    now = time(nullptr);
  }

  Serial.println(F(""));
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
  debugA("Current time: %s", asctime(&timeinfo));
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print("Local time: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setNextWakeUp()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  int32_t currentTime = timeinfo.tm_hour*3600 + timeinfo.tm_min*60 + timeinfo.tm_sec;

  if(WAKEUP_TIME - currentTime > 0) {
	esp_sleep_enable_timer_wakeup((WAKEUP_TIME - currentTime) * uS_TO_S_FACTOR);
	Serial.print("Next wakeup in "); Serial.println(WAKEUP_TIME - currentTime);
  } else {
	esp_sleep_enable_timer_wakeup((SLEEP_ONE_DAY - (currentTime - WAKEUP_TIME)) * uS_TO_S_FACTOR);
	uint32_t tmp = (SLEEP_ONE_DAY - (currentTime - WAKEUP_TIME));
	Serial.printf("Next wakeup tomorrow in %d seconds - %d:%d:%d\n",  tmp, tmp/3600, (tmp - (tmp/3600)*3600)/60, tmp/3600);
  }
  Serial.flush(); 
}

void checkVersion()
{
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(rootCACertificate);

    // Reading data over SSL may be slow, use an adequate timeout
    client -> setTimeout(12000 / 1000); // timeout argument is defined in seconds for setTimeout
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      https.useHTTP10(true);
      if (https.begin(*client, "https://brun100gr.netsons.org/ESP32/version.json")) {  // HTTPS
        Serial.println("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            // Parse response
            DynamicJsonDocument doc(128);
            deserializeJson(doc, https.getStream());
            
            // Read values
            Serial.println(doc["version"].as<float>());
            sendTelegramMessage("Version: " + String(float(doc["version"])) + " - url: " + (const char*)doc["url"]);
            // Check if a newer version is available
            if (float(doc["version"]) > currentSWVersion.toFloat()) {
              Serial.println("New SW version available!");
              debugA("New SW version available!");
              
              t_httpUpdate_return ret = httpUpdate.update(*client, doc["url"]);
              switch (ret) {
                case HTTP_UPDATE_FAILED:
                  Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                  break;
          
                case HTTP_UPDATE_NO_UPDATES:
                  Serial.println("HTTP_UPDATE_NO_UPDATES");
                  break;
          
                case HTTP_UPDATE_OK:
                  Serial.println("HTTP_UPDATE_OK");
                  delay(1000);
                  ESP.restart();
                  break;
              }
            }
            // Disconnect
            https.end();
          }
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
 
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
      // End extra scoping block
    }
  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
}

void setup() {
  // Start serial port for debugging
  Serial.begin(115200);
  
  wakeUpReasons = static_cast<wakeUpReasonsEnum>(check_wakeup_reason());

  Serial.print("Wakeup reason: "); Serial.println(int(wakeUpReasons));
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  uint32_t connectionAttempts = 0; 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    connectionAttempts++;
    if (connectionAttempts > CONNECTTION_ATTEMPTS_LIMIT) {
      ESP.restart();
    }
  }

  // Print local IP address
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup remote debug
  remoteDebugSetup();

  // Set time via NTP, as required for x.509 validation
  setClock();
  
  // Setup Telegram bot
  setupTelegramBot();

  // Setup OTA
  ArduinoOTA.setHostname("MouseTrap");
  otaSetup();

  // Check if a new version is available
  checkVersion();
  // Setup wekeup sources
//  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK_GPIO32 + BUTTON_PIN_BITMASK_GPIO33, ESP_EXT1_WAKEUP_ANY_HIGH);

  // Reading battery voltage
  // Discard same samples to stabilize ADC
  for (int i = 0; i < NUMBER_OF_SAMPLES + 1; i++)
  {
    batteryMonitorValue = analogRead(batteryMonitorPin);
  }
  Serial.printf("Version: %s - ADC %d, battery voltage: %f\n", currentSWVersion, batteryMonitorValue, (3.3/4096*batteryMonitorValue*2));
  debugA("Version: %s - ADC %d, battery voltage: %f", currentSWVersion, batteryMonitorValue, (3.3/4096*batteryMonitorValue*2));
//  sendTelegramMessage(String("Version: " + currentSWVersion + " - ADC: " + String(batteryMonitorValue) + ", battery voltage: " + String(3.3/4096*batteryMonitorValue*2)));

  printLocalTime();
}

uint32_t counterBeforeSleep = UPTIME_BEFORE_SLEEP;
void loop() {
  if ((wakeUpReasons == wakeUpReasonsEnum::GPIO_MANUAL) && (counterBeforeSleep > 0)) {
  	Serial.printf("Seconds before sleep: %d\n", counterBeforeSleep);
  	counterBeforeSleep--;

    // Check OTA
    ArduinoOTA.handle();

    Debug.handle();

    delay(1000);
  } else {
	setNextWakeUp();

	// Start to sleep
    esp_deep_sleep_start();
  }
}

int print_wakeup_reason(){
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
  return wakeup_reason;
}

int check_wakeup_reason() {
   switch(print_wakeup_reason()){
    case ESP_SLEEP_WAKEUP_EXT1:
      if (BUTTON_PIN_BITMASK_GPIO32 & esp_sleep_get_ext1_wakeup_status()) {
        Serial.println("Wakeup from GPIO_MANUAL(32).");
        return GPIO_MANUAL;
      } else if (BUTTON_PIN_BITMASK_GPIO33 & esp_sleep_get_ext1_wakeup_status()) {
        Serial.println("Wakeup from GPIO_TRAP(33).");
        return GPIO_TRAP;
      } else {
        Serial.println("Wakeup from unexpected GPIO.");
        return GPIO_UNEXPECTED;
      }
    break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup from TIMER");
      return TIMER;
    break;
    default:
      Serial.println("Other wakeup reason.");
      return OTHER;
    break;
  }
}
