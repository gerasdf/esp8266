/* Adaptado de el ejemplo original por Cesar Riojas
    www.arduinocentermx.blogspot.mx
    Autoconnect: https://www.hackster.io/hieromon-ikasamo/esp8266-esp32-connect-wifi-made-easy-d75f45
        ESSID: esp8266ap / 12345678
 * */
#define BOT_AND_WIFI
#define AUTOCONNECT

#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#ifdef AUTOCONNECT
#include <AutoConnect.h>
AutoConnect portal;
#endif

// All Configuration options

#define MY_NAME     "Caldera_0"

bool polarity_inverted = false;

// int digitalInputPin = 12;  // GPIO12 - NodemCU D6
int digitalInputPin = 5;  // GPIO5 - Relay module - optocoupled input
int digitalOutputPin = 4; // GPIO4 - Relay module - relay control

#define TelegramBotToken "648272766:AAEkW5FaFMeHqWwuNBsZJckFEOdhlSVisEc"

String default_chat_id = "25235518"; // gera
// String default_chat_id = "268186747"; // Agu

#ifndef AUTOCONNECT
char WiFi_ssid[] = "Your Fixed ESSID";
char WiFi_key[] = "Your Password";
#endif // AUTOCONNECT

//////////////////

void relay_set(int value);
void cmd_status(String chat_id, String from_name);

bool WiFi_ok = false;

bool input_status;
int relay_state;

WiFiClientSecure client;

void WiFi_setup() {
#ifdef AUTOCONNECT
  portal.config(MY_NAME, MY_NAME "Password2020!");
  if (portal.begin()) {
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
  } else {
    Serial.println("connection failed:");
  }
#else // AUTOCONNECT
  // Establecer el modo WiFi y desconectarse de un AP si fue Anteriormente conectada
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println(WiFi_ssid);
  WiFi.begin(WiFi_ssid, WiFi_key);
#endif // AUTOCONNECT

  WiFi_ok = WiFi.status() == WL_CONNECTED;
}

void WiFi_loop() {
#ifdef AUTOCONNECT
  portal.handleClient();
#else // AUTOCONNECT
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
#endif // AUTOCONNECT
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
  cmd_unblink();
  Serial.println("\nI'm " MY_NAME "\n");
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

void cmd_polarity(String chat_id, String from_name) {
  polarity_inverted = !polarity_inverted;
  cmd_status(chat_id, from_name);
}

// Telegram Bot

UniversalTelegramBot bot(TelegramBotToken, client);

int Bot_mtbs_ms = 3000;
long Bot_nexttime = 0;
bool Bot_greeted = false;

// General

void send_message(String chat_id, String text) {
  bot.sendMessage(chat_id, text, "Markdown");    
}

// Greeter
void cmd_start(String chat_id, String from_name) {
  String welcome = "Hola, " + from_name + ". chat_id=" + chat_id + ". I'm " MY_NAME "\n";
  
  bot.sendMessage(chat_id, welcome);
  default_chat_id = chat_id;
  cmd_help(chat_id, from_name);
  cmd_status(chat_id, from_name);

  /*
  DynamicJsonDocument doc(200);
  deserializeJson(doc, "}{\"ok\":true, \"nok\":false}");

  bot.sendMessage(chat_id, String("ok: ")    + (bool)doc["ok"] +           " nok: "  + (bool)doc["mok"] +
                 " mokt: " + (bool)(doc["mok"] | true) + " mokf: " + (bool)(doc["mok"] | false)); 
  */
}

void cmd_help(String chat_id, String from_name) {
  String help =
    "`status` shows all status\n"
    "`polarity` changes input polarity\n"
    "`start` registers who will receive alerts\n"
    "`ron` turns on Relay\n"
    "`roff` turns off Relay\n"
    "`reset` resets the system\n";
  if (chat_id == "") chat_id = default_chat_id;
  bot.sendMessage(chat_id, help, "Markdown");    
}

void cmd_status(String chat_id, String from_name) {
  String in_st  = input_status?"Ok":"ALARMA!";
  String rel_st = relay_state?"On":"Off";
  String pol = polarity_inverted?"inverted":"normal";
  
  String msg = MY_NAME " status: " + in_st + " relay: " + rel_st + " polarity: " + pol + " last: " + bot.last_sent_message_id + "\n";
  
  if (chat_id == "") chat_id = default_chat_id;
  Serial.print(msg);
  bot.sendMessage(chat_id, msg);
}

void cmd_relay_on(String chat_id, String from_name) {
   relay_set(1);
   cmd_status(chat_id, from_name);
}

void cmd_relay_off(String chat_id, String from_name) {
   relay_set(0);
   cmd_status(chat_id, from_name);
}

void Bot_handleNewMessages(int numNewMessages) {
  static bool firstMsg = true;
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String cmd = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    cmd.toLowerCase();
    if (cmd[0] == '/') cmd.remove(0,1);
    if (from_name == "") from_name = "GUEST";

    Serial.print("Received \"" + cmd + "\" from " + from_name + "\n");

    if (cmd == "start") cmd_start(chat_id, from_name);
//    else if (cmd == "blink") cmd_blink();
//    else if (cmd == "unblink") cmd_unblink();
    else if (cmd == "status") cmd_status(chat_id, from_name);
    else if (cmd == "polarity") cmd_polarity(chat_id, from_name);

    else if (cmd == "ron") cmd_relay_on(chat_id, from_name);
    else if (cmd == "roff") cmd_relay_off(chat_id, from_name);

    else if (cmd == "reset") {
      if (!firstMsg) ESP.reset();
    }
    else cmd_help(chat_id, from_name);
    
    firstMsg = false;
  }
}

void Bot_setup() {
  client.setInsecure();
  Bot_nexttime = millis() - 1;
  bot.longPoll = 1;
}

void Bot_first_time() {
  String commands = "["
    "{\"command\":\"help\",  \"description\":\"Get bot usage help\"},"
    "{\"command\":\"reset\", \"description\":\"reset device\"},"
    "{\"command\":\"start\", \"description\":\"register with (all) devices as their user\"},"
    "{\"command\":\"status\",\"description\":\"answer device current status\"},"
    "{\"command\":\"polarity\",\"description\":\"changes input polarity\"},"
    "{\"command\":\"ron\",\"description\":\"turn relay on\"},"
    "{\"command\":\"roff\",\"description\":\"turn relay off\"}"
  "]";

  bot.setMyCommands(commands);
  
  if (default_chat_id != "") {
    cmd_start(default_chat_id, "");
  }
}

void Bot_loop() {
  if (!WiFi_ok) return;
  if (!Bot_greeted) {
    Bot_greeted = true;
    Bot_first_time();
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
    Bot_nexttime = millis() + Bot_mtbs_ms;
  }
}

///////////////////////

bool input_read() {
  return digitalRead(digitalInputPin) ^ polarity_inverted;
}

void input_setup() {
  pinMode(digitalInputPin, INPUT_PULLUP);
  input_status = input_read();
}

void input_loop() {
  bool new_status = input_read();
  
  if (input_status != new_status) {
      input_status = new_status;
#ifdef BOT_AND_WIFI
      cmd_status("","");
#endif
  }
}

//// OTA

void OTA_setup() {
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(MY_NAME);
  ArduinoOTA.setPasswordHash("fff9a16a632c7daa86f7a4a8ce1929d6");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void OTA_loop() {
  ArduinoOTA.handle();
}

/////////// Relay

void relay_setup() {
   pinMode(digitalOutputPin, OUTPUT);
   relay_set(0);
}

void relay_set(int value) {
   relay_state = value;
   digitalWrite(digitalOutputPin, value);
}

///////////////////////

void setup() {
  blink_setup();
#ifdef BOT_AND_WIFI
  WiFi_setup();
  OTA_setup();
  Bot_setup();
#endif
  input_setup();
  relay_setup();
}

void loop() {
#ifdef BOT_AND_WIFI
  WiFi_loop();
  OTA_loop();
  Bot_loop();
#endif
  // blink_loop();
  input_loop();
}
