/* Adaptado de el ejemplo original por Cesar Riojas entre otras miles de cosas
    www.arduinocentermx.blogspot.mx
    Autoconnect: https://www.hackster.io/hieromon-ikasamo/esp8266-esp32-connect-wifi-made-easy-d75f45
 */
#define BOT_AND_WIFI
#define AUTOCONNECT
#define ALERT_DEBUG

#include "git-version.h"
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#ifdef AUTOCONNECT
#include <AutoConnect.h>
AutoConnect portal;
#endif

int OTA_delay = 0;

// All Configuration options

#define MY_NAME            "ToI "
#define TELEGRAM_BOT_TOKEN "648272766:AAEkW5FaFMeHqWwuNBsZJckFEOdhlSVisEc"

#ifdef ALERT_DEBUG
#define DPRINTLN(X)  debug_log(X);
#define DPRINT(X)    debug_log(X, false);
#else
#define DPRINTLN(X)
#define DPRINT(X)
#endif 

const char config_file_name[] = "/botconfig.json";

class Config {
public:
  String owner_id;
  String token;
  String name;
  String password;
  bool polarity_inverted;

  Config() : 
    owner_id(F("25235518")),
    token(F(TELEGRAM_BOT_TOKEN)),
    name(String(F(MY_NAME)) + String(ESP.getChipId(), HEX)),
    password(F(MY_NAME "Password2020!")),
    polarity_inverted(true) 
  {
      SPIFFS.begin();
  }
  
  void load() {
    File configFile=SPIFFS.open(config_file_name, "r");
    if (configFile) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, configFile);
      configFile.close();

      if (error) {
        Serial.println(F("Failed to read file, using default configuration"));
      } else {
        owner_id          = doc["owner_id"] | owner_id;
        token             = doc["token"] | token;
        name              = doc["name"] | name;
        password          = doc["password"] | password;
        polarity_inverted = doc["polarity_inverted"] | polarity_inverted;
      }
    }
  }
  void save() {
    File configFile=SPIFFS.open(config_file_name,"w");
    if (configFile) {
      StaticJsonDocument<256> doc;
      doc["owner_id"] = owner_id;
      doc["token"] = token;
      doc["name"] = name;
      doc["password"] = password;
      doc["polarity_inverted"] = polarity_inverted;
      if (serializeJson(doc, configFile) == 0) {
        Serial.println(F("Failed to write to file"));
      }
      configFile.close();
    }
  }
} config;

// int digitalInputPin = 12;  // GPIO12 - NodemCU D6
int digitalInputPin = 5;  // GPIO5 - Relay module - optocoupled input
int digitalOutputPin = 4; // GPIO4 - Relay module - relay control

#ifndef AUTOCONNECT
char WiFi_ssid[] = "Your Fixed ESSID";
char WiFi_key[] = "Your Password";

#endif // AUTOCONNECT

//////////////////

void relay_set(int value);
void cmd_status(String &chat_id);
void Bot_first_time();

bool input_status;
int relay_state;

#define WIFI_RECHECK_ms           1000
#define NO_WIFI_THEN_RESET_ms     1000*30*60

bool WiFi_ok = false;

WiFiClientSecure client;

void debug_log(const String &msg, bool ln = true) {   
  String line("[");

  line += millis()/1000;
  line += "] ";
  line += msg;
  if (ln) Serial.println(line);
  else Serial.print(line);
}

void WiFi_setup() {
#ifdef AUTOCONNECT
  AutoConnectConfig portalConfig(config.name.c_str(), config.password.c_str());
  portalConfig.ota = AC_OTA_BUILTIN;
  portalConfig.autoReconnect = true;
  portalConfig.portalTimeout = 2*60*1000; // ms
  portal.config(portalConfig);
  do {
    if (portal.begin()) {
      Serial.println("connected:" + WiFi.SSID());
      Serial.println("IP:" + WiFi.localIP().toString());
    } else {
      Serial.println(F("connection failed. Trying again"));
    }
  } while (WiFi.status() != WL_CONNECTED);
#else // not AUTOCONNECT
  // Establecer el modo WiFi y desconectarse de un AP si fue Anteriormente conectada
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println(WiFi_ssid);
  WiFi.begin(WiFi_ssid, WiFi_key);
#endif // AUTOCONNECT
  WiFi.setAutoReconnect(true);
  WiFi_ok = WiFi.status() == WL_CONNECTED;
}

void WiFi_loop() {
#ifdef AUTOCONNECT
  portal.handleClient();
#endif // not AUTOCONNECT

  static long lastCheck = 0;
  static long lastOk    = millis();
  bool next_ok;

  if (millis() - lastCheck > WIFI_RECHECK_ms) {
    next_ok = WiFi.status() == WL_CONNECTED;

    if (!next_ok) {
      DPRINT(".");
    }

    if (next_ok && !WiFi_ok) {
      Serial.println(F("Connected!"));
    }
    WiFi_ok = next_ok;
    lastCheck = millis();
  }
  
  if (WiFi_ok) {
    lastOk = millis();
  } else {
    if (millis() - lastOk > NO_WIFI_THEN_RESET_ms) {
      DPRINTLN(F("WiFi timed out. I'll be back"));
      ESP.reset();
    }
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
  cmd_unblink();
  Serial.println("I'm " + config.name);
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

UniversalTelegramBot bot(config.token, client);

int Bot_mtbs_ms = 3000;
unsigned long Bot_nexttime = 0;
bool Bot_greeted = false;

// General

void send_message(String &chat_id, String &text) {
  String msg = F("*");
  msg += config.name;
  msg += "*: ";
  
  if (chat_id == "") chat_id = config.owner_id;
  
  msg += text;
  debug_log(msg);
  bot.sendMessage(chat_id, msg, "Markdown");
}

void clean_last_message() {
  bot.getUpdates(bot.last_message_received + 1);
}

bool is_from_owner(telegramMessage &msg) {
  return (msg.chat_id == config.owner_id);
}

bool is_for_me(telegramMessage &msg) {
  return msg.reply_to_text.startsWith(config.name + ":");
}

// Greeter
void cmd_start(String &chat_id) {
  String welcome = F("Hola, your `chat_id` is ");
  welcome += chat_id;

  // config.owner_id = chat_id;
  send_message(chat_id, welcome);
  cmd_status(chat_id);
}

void cmd_help(String &chat_id) {
  String help = F(" See commands help when pressing the `/` button");

  send_message(chat_id, help);
}

void cmd_status(String &chat_id) {
  String in_st  = input_status?"Ok":"ALARMA!";
  String rel_st = relay_state?"On":"Off";
  String pol = config.polarity_inverted?"-":"+";
  
  String msg = F("status: *");
  msg += in_st;
  msg += F("* relay: *");
  msg += rel_st;
  msg += F("* polarity: *");
  msg += pol;
  msg += F("*");
  
  send_message(chat_id, msg);
}

void cmd_sysinfo(String &chat_id) {
  String msg = F("\nIP: *");    msg += WiFi.localIP().toString();
  msg += F("*\nChipID: *");     msg += String(ESP.getChipId(), HEX);
  msg += F("*\nESSID: *");      msg += WiFi.SSID();
  msg += F("*\nBSSID: *");      msg += WiFi.BSSIDstr();
  msg += F("*\nSignal: *");     msg += WiFi.RSSI();
  msg += F(" dBm*\nRAM: *");    msg += ESP.getFreeHeap();
  msg += F("*\nuptime: *");     msg += millis()/1000;
  msg += F("*\nowner: *");      msg += config.owner_id;
  msg += F("*\nversion: *" GIT_VERSION "*");
  // msg += F("*");
  
  send_message(chat_id, msg);
  if (OTA_delay) OTA_delay--;
}

void cmd_keyboard(String &chat_id) {
  String keyboard = F("[["
     "{\"text\":\"sysinfo\",\"callback_data\":\"sysinfo\"},"
     "{\"text\":\"status\",\"callback_data\":\"status\"},"
     "{\"text\":\"polarity\",\"callback_data\":\"polarity\"}],"
     "[{\"text\":\"ron\",\"callback_data\":\"ron\"},"
     "{\"text\":\"roff\",\"callback_data\":\"roff\"}],"
     "[{\"text\":\"ronoff\",\"callback_data\":\"ronoff\"},"
     "{\"text\":\"roffon\",\"callback_data\":\"roffon\"}"
     "]]}"
  );
  String msg = F("*");
  msg += config.name;
  msg += F("*: use the keyboard");
  bot.sendMessageWithInlineKeyboard(chat_id, msg, "Markdown", keyboard); //, false, true, false);
}

void cmd_polarity(telegramMessage &msg) {
  config.polarity_inverted = !config.polarity_inverted;
  config.save();
  
  if (msg.query_id) {
    String answer = F("Polarity is now ");
    answer += config.polarity_inverted?"-":"+";
    bot.answerCallbackQuery(msg.query_id, answer);
  } else {
    cmd_status(msg.chat_id);
  }
}

void cmd_setname(telegramMessage &msg) {
  int first_space = msg.text.indexOf(' ');

  if (-1 == first_space) return;

  const String &new_name = msg.text.substring(first_space+1);
  if (new_name.length() <= 3) return;

  config.name = new_name;
  config.save();
  cmd_status(msg.chat_id);
}

void cmd_setowner(telegramMessage &msg) {
  int first_space = msg.text.indexOf(' ');

  if (-1 == first_space) return;

  const String &new_owner_id = msg.text.substring(first_space+1);
  if (new_owner_id.length() <= 3) return;

  String granted_msg = String(F("Transferred ownership to ")) + new_owner_id;

  if (msg.query_id) {
    bot.answerCallbackQuery(msg.query_id, granted_msg);
  }
  send_message(config.owner_id, granted_msg);

  config.owner_id = new_owner_id;
  config.save();
  cmd_start(config.owner_id);
}

void cmd_settoken(telegramMessage &msg) {
  int first_space = msg.text.indexOf(' ');

  if (-1 == first_space) return;

  const String new_token = msg.text.substring(first_space+1);
  if (new_token.length() <= 3) return;

  clean_last_message();
  bot.updateToken(new_token);

  String keyboard = String(F("[["
     "{\"text\":\"confirm\",\"callback_data\":\"confirmtoken ")) + new_token + F("\"}"
     ",{\"text\":\"deny\",\"callback_data\":\"confirmtoken ") + config.token + F("\"}"
     "]]}"
  );
  String answer = F("*");
  answer += config.name;
  answer += F("*: User ");
  answer += msg.chat_id;
  answer += F(" (");
  answer += msg.from_name;
  answer += F(") is asking to connect the device to this Bot");

  bot.sendMessageWithInlineKeyboard(config.owner_id, answer, "Markdown", keyboard); //, false, true, false);
  // save old token and start timeout to revert token
}

void cmd_confirmtoken(telegramMessage &msg) {
  int first_space = msg.text.indexOf(' ');

  if (-1 == first_space) return;

  const String new_token = msg.text.substring(first_space+1);
  if (new_token.length() <= 3) return;

  if (new_token == bot.getToken()) {
    config.token = new_token;
    config.save();
    cmd_status(msg.chat_id);
  } else {
    DPRINTLN(("Received /confirmtoken for " + new_token + " and current token is " + bot.getToken()));
  }
}

void cmd_relay_set(telegramMessage &msg, int first_state, int second_state) {
   relay_set(first_state);
   if (second_state != -1) {
     delay(1000);
     relay_set(second_state);
   }
   if (msg.query_id) {
     String answer = F("Relay turned ");
     answer += first_state?"On":"Off";
     if (second_state != -1)
       answer += second_state?F(" then On"):F(" then Off");
     bot.answerCallbackQuery(msg.query_id, answer);
   } else {
     cmd_status(msg.chat_id);
   }
}

void cmd_sent_file(int i) {
  DPRINTLN((String("Received document: ") + bot.messages[i].file_caption + " size: " + bot.messages[i].file_size));
  DPRINTLN((String("URL: ") + bot.messages[i].file_path));
  t_httpUpdate_return ret = ESPhttpUpdate.update(bot.messages[i].file_path);
  #ifdef ALERT_DEBUG
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf_P(PSTR("HTTP_UPDATE_FAILD Error (%d): %s"), ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
  #endif
}

void cmd_own(telegramMessage &msg) {
  String keyboard = String(F("[["
     "{\"text\":\"grant\",\"callback_data\":\"setowner ")) + msg.chat_id + F("\"}"
     // ",{\"text\":\"deny\",\"callback_data\":\"denyown ")) + msg.chat_id + F("\"}"
     "]]}"
  );
  String answer = F("*");
  answer += config.name;
  answer += F("*: User ");
  answer += msg.chat_id;
  answer += F(" (");
  answer += msg.from_name;
  answer += F(") is asking ownership");
  bot.sendMessageWithInlineKeyboard(config.owner_id, answer, "Markdown", keyboard); //, false, true, false);
}

void delay_next_poll() {
  Bot_nexttime += Bot_mtbs_ms;
}

void Bot_handleNewMessages(int numNewMessages) {
  static bool firstMsg = true;
  bool message_for_other_device = false;
  
  for (int i = 0; i < numNewMessages; i++) {
    telegramMessage &msg = bot.messages[i];
    String cmd = msg.text;

    cmd.toLowerCase();
    if (cmd[0] == '/') cmd.remove(0,1);

    debug_log(String(F("Received \"")) + cmd + F("\" from ") + msg.chat_id);

    // Global messages (acceptable for all devices at the same time, without any filtering)
    if (cmd == "allstatus") {
      cmd_status(msg.chat_id);
      message_for_other_device = true;
    }
    else if (cmd == "allsysinfo") {
      cmd_sysinfo(msg.chat_id);
      message_for_other_device = true;
    }
    else if (is_from_owner(msg) && (cmd == "start")) {
      cmd_start(msg.chat_id);
      message_for_other_device = true;
    }
    else if (is_for_me(msg) && (cmd == "own")) {
      cmd_own(msg);
    }
    // Device commands (only acceptable if directed to a particular device and from the owner)
    else if (is_for_me(msg) && is_from_owner(msg)) {
      if      (cmd == "status") cmd_status(msg.chat_id);
      else if (cmd == "polarity") cmd_polarity(msg);
      else if (cmd == "ron") cmd_relay_set(msg, 1, -1);
      else if (cmd == "roff") cmd_relay_set(msg, 0, -1);
      else if (cmd == "ronoff") cmd_relay_set(msg, 1, 0);
      else if (cmd == "roffon") cmd_relay_set(msg, 0, 1);
      else if (cmd == "sysinfo") cmd_sysinfo(msg.chat_id);
      else if (cmd == "keyboard") cmd_keyboard(msg.chat_id);
      else if (cmd.startsWith(F("setname "))) cmd_setname(msg);
      else if (cmd.startsWith(F("setowner "))) cmd_setowner(msg);
      else if (cmd.startsWith(F("settoken "))) cmd_settoken(msg);
      else if (cmd.startsWith(F("confirmtoken "))) cmd_confirmtoken(msg);
      else if (cmd == "reset") {
        if (!firstMsg) ESP.reset();
      }
      else if (bot.messages[i].hasDocument) cmd_sent_file(i);
      else cmd_help(msg.chat_id);
    } else {
      message_for_other_device = true;
    }
    
    firstMsg = false;
  }
  if (message_for_other_device) delay_next_poll();
}

void Bot_setup() {
  client.setInsecure();
  Bot_nexttime = millis() - 1;
  bot.longPoll = 1;
  bot.maxMessageLength = 3000;
}

void Bot_first_time() {
  String commands = F("["
    "{\"command\":\"allstatus\",\"description\":\"answer all devices current status\"},"
    "{\"command\":\"allsysinfo\",\"description\":\"answer all devices system info\"},"
    "{\"command\":\"help\",  \"description\":\"Get bot usage help\"},"
    "{\"command\":\"keyboard\",  \"description\":\"display keyboard\"},"
    "{\"command\":\"own\",  \"description\":\"request device ownership\"},"
    "{\"command\":\"polarity\",\"description\":\"changes input polarity\"},"
    "{\"command\":\"reset\", \"description\":\"reset device\"},"
    "{\"command\":\"roff\",\"description\":\"turn relay off\"},"
    "{\"command\":\"ron\",\"description\":\"turn relay on\"},"
    "{\"command\":\"roffon\",\"description\":\"turn relay off then on\"},"
    "{\"command\":\"ronoff\",\"description\":\"turn relay on then off\"},"
    "{\"command\":\"setname\",\"description\":\"changes device's name\"},"
    // "{\"command\":\"setowner\",\"description\":\"changes device's owner id. *dangerous*\"}," // private command
    "{\"command\":\"settoken\",\"description\":\"changes device's bot token. *dangerous*\"},"
    "{\"command\":\"start\", \"description\":\"register with (all) devices as their user\"},"
    "{\"command\":\"status\",\"description\":\"answer device current status\"},"
    "{\"command\":\"sysinfo\",\"description\":\"answer device system info\"}"
  "]");

  bot.setMyCommands(commands);
  
  cmd_start(config.owner_id);
}

void Bot_loop() {
  if (!WiFi_ok) return;
  if (!Bot_greeted) {
    Bot_greeted = true;
    Bot_first_time();
  }
  if (millis() > Bot_nexttime)  {
    Bot_nexttime = millis() + Bot_mtbs_ms;

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    DPRINTLN((
      String("numNewMessages: ") + numNewMessages +
      " last: " + bot.last_message_received) +
      " mem: " + ESP.getFreeHeap() +
      F(" - ") + ESP.getHeapFragmentation() +
      F(" - ") + ESP.getMaxFreeBlockSize()
    );
    Bot_handleNewMessages(numNewMessages);
  }
}

///////////////////////

bool input_read() {
  return digitalRead(digitalInputPin) ^ config.polarity_inverted;
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
      cmd_status(config.owner_id);
#endif
  }
}

//// OTA

void OTA_setup() {
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(config.name.c_str());
  ArduinoOTA.setPasswordHash(String(F("fff9a16a632c7daa86f7a4a8ce1929d6")).c_str());
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
  if (OTA_delay == 0) ArduinoOTA.handle();
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
  // config.save(); // Uncomment to reset to default config
  config.load();
  bot.updateToken(config.token);
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
