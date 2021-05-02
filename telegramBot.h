/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/telegram-esp32-motion-detection-arduino/
  
  Project created using Brian Lough's Universal Telegram Bot Library: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// Initialize Telegram BOT
#define BOTtoken "1720331573:AAHc977QrVeVwehHcWIsGqOYvb8hOmkLkFY"  // your Bot Token (Get from Botfather)-->MouseTrap21013Bot

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#define CHAT_ID "1007315026"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOTtoken, secured_client);

void setupTelegramBot() {
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
}

void sendTelegramMessage(const String& message) {
  bot.sendMessage(CHAT_ID, message, "");
  delay(1000);
}
