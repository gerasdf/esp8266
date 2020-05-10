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
#else
// All WiFi

//char WiFi_ssid[] = "Your ESSID";
//char WiFi_key[] = "Your Password";
#endif // AUTOCONNECT

bool WiFi_ok = false;

String my_name = "Caldera #1";

bool input_read();
bool polarity_inverted = false;

// int digitalInputPin = 12;  // GPIO12 - NodemCU D6
int digitalInputPin = 5;  // GPIO5 - Relay module - optocoupled input
int digitalOutputPin = 4; // GPIO4 - Relay module - relay control

WiFiClientSecure client;

void WiFi_setup() {
#ifdef AUTOCONNECT
  portal.config("ViveroHirose","viverohirose");
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
  Serial.println("\nI'm " + my_name + "\n");
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

#define BOTtoken "648272766:AAEkW5FaFMeHqWwuNBsZJckFEOdhlSVisEc"

UniversalTelegramBot bot(BOTtoken, client);

String default_chat_id = "25235518"; // gera
// String default_chat_id = "268186747"; // Agu

int Bot_mtbs_ms = 5000;
long Bot_nexttime = 0;
bool Bot_greeted = false;

// Greeter
void cmd_hola(String chat_id, String from_name) {
  String welcome = "Hola, " + from_name + ". chat_id=" + chat_id + ". I'm "+ my_name +"\n";
  bot.sendMessage(chat_id, welcome);
  default_chat_id = chat_id;
  cmd_status();
}

void cmd_status(String chat_id, String from_name) {
  String st  = input_read()?"Ok":"ALARMA!";
  String pol = polarity_inverted?"inverted":"normal";
  String msg = my_name + " status: " + st + " polarity: " + pol + "\n";
  
  if (chat_id == "") chat_id = default_chat_id;
  Serial.print(msg);
  bot.sendMessage(chat_id, msg);
}

void cmd_relay_on() {
}

void cmd_relay_off() {
}

void Bot_handleNewMessages(int numNewMessages) {
  static bool firstMsg = true;
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String cmd = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    cmd.toLowerCase();
    if (from_name == "") from_name = "GUEST";

    Serial.print("Received \"" + cmd + "\" from " + from_name + "\n");

    if (cmd == "hola") {
      cmd_hola(chat_id, from_name);
      cmd_status(chat_id, from_name);
    }
//    if (cmd == "blink") cmd_blink();
//    if (cmd == "unblink") cmd_unblink();
    if (cmd == "status") cmd_status(chat_id, from_name);
    if (cmd == "polarity") cmd_polarity(chat_id, from_name);

    if (cmd == "ron") cmd_relay_on();
    if (cmd == "roff") cmd_relay_off();

    if (!firstMsg && cmd == "reset") ESP.reset();
    firstMsg = false;
  }
}

void Bot_setup() {
  client.setInsecure();
  Bot_nexttime = millis() - 1;
}

void Bot_loop() {
  if (!WiFi_ok) return;
  if (!Bot_greeted) {
    Bot_greeted = true;
    if (default_chat_id != "") {
      cmd_hola(default_chat_id, "?");
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
    Bot_nexttime = millis() + Bot_mtbs_ms;
  }
}

///////////////////////

bool input_status = false;

void input_setup() {
  pinMode(digitalInputPin, INPUT_PULLUP);
  input_status = input_read() ^ polarity_inverted;
}

bool input_read() {
  return digitalRead(digitalInputPin);
}

void input_loop() {
  bool new_status = input_read();
  
  if (input_status != new_status) {
  // Serial.print(String(new_status)+"\r");
#ifdef BOT_AND_WIFI
      cmd_status("","");
#endif
      input_status = new_status;
  }
}

//// OTA

void OTA_setup() {
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("caldera1");
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

///////////////////////

void setup() {
  blink_setup();
#ifdef BOT_AND_WIFI
  WiFi_setup();
  OTA_setup();
  Bot_setup();
#endif
  input_setup();
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
