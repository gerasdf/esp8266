/* Adaptado de el ejemplo original por Cesar Riojas
    www.arduinocentermx.blogspot.mx
 * */
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// All WiFi

char WiFi_ssid[] = "barrio.rawson@gmail.com";
char WiFi_key[] = "paramisamigosconamor";
bool WiFi_ok = false;

WiFiClientSecure client;

void WiFi_setup() {
  // Establecer el modo WiFi y desconectarse de un AP si fue Anteriormente conectada
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println(WiFi_ssid);
  WiFi.begin(WiFi_ssid, WiFi_key);

  WiFi_ok = WiFi.status() == WL_CONNECTED;
}

void WiFi_loop() {
  static long lastCheck = 0;
  bool next_ok;

  if (millis() - lastCheck > 500) {
    next_ok = WiFi.status() == WL_CONNECTED;
    
    if (!next_ok) {
      Serial.print(".");
    }

    if (next_ok && !WiFi_ok) {
      Serial.println("Connected!");
    }
    WiFi_ok = next_ok;
    lastCheck = millis();
  }
}

// Blinker
bool blinking = true;

void cmd_blink() {
  blinking = true;
  pinMode(1, OUTPUT);
}

void cmd_unblink() {
  blinking = false;
  Serial.begin(115200, SERIAL_8N1); 
}

void blink_setup() {
  Serial.begin(115200, SERIAL_8N1);
}

void blink_loop() {
  static int value = 0;

  if (!blinking) return;

  value++;
  if (value == 1000) value = -1000;
  analogWrite(1, abs(value));
  delay(1);
  Serial.println(value);
}

// Telegram Bot

#define BOTtoken "648272766:AAEkW5FaFMeHqWwuNBsZJckFEOdhlSVisEc"

UniversalTelegramBot bot(BOTtoken, client);

String default_chat_id = "25235518";
int Bot_mtbs_ms = 5000;
long Bot_nexttime = 0;
bool Bot_greeted = false;

// Greeter
void cmd_hola(String chat_id, String from_name) {
  String welcome = "Hola, " + from_name + ". chat_id=" + chat_id + "\n";
  bot.sendMessage(chat_id, welcome);
}

void Bot_handleNewMessages(int numNewMessages) {
  static bool firstMsg = true;
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String cmd = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    cmd.toLowerCase();
    if (from_name == "") from_name = "GUEST";

    if (cmd == "hola") cmd_hola(chat_id, from_name);
    if (cmd == "blink") cmd_blink();
    if (cmd == "unblink") cmd_unblink();
    if (!firstMsg && cmd == "reset") ESP.reset();
    firstMsg = false;
  }
}

void Bot_setup() {
  Bot_nexttime = millis() - 1;
}

void Bot_loop() {
  if (!WiFi_ok) return;
  if (!Bot_greeted) {
    Bot_greeted = true;
    if (default_chat_id != "") {
      bot.sendMessage(default_chat_id, "HOLA!!!");
    }
  }
  if (millis() > Bot_nexttime)  {
    Serial.print("/");
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (!numNewMessages) {
      Serial.print("-");
    }
    while (numNewMessages) {
      Serial.print("+");
      Bot_handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    Bot_nexttime = millis()+Bot_mtbs_ms;
  }
}

void setup() {
  blink_setup();
  WiFi_setup();
  Bot_setup();
}

void loop() {
  WiFi_loop();
  Bot_loop();
  blink_loop();
}
