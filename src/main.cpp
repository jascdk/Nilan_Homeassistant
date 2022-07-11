/**
  Nilan Modbus firmware for D1 Mini (ESP8266) together with a TTL to RS485 Converter https://www.aliexpress.com/item/32836213346.html?spm=a2g0s.9042311.0.0.27424c4dqnr5i7

  Written by Dan Gunvald
    https://github.com/DanGunvald/NilanModbus

  Modified to use with Home Assistant by Anders Kvist, Jacob Scherrebeck and other great people :)
    https://github.com/anderskvist/Nilan_Homeassistant
    https://github.com/jascdk/Nilan_Homeassistant

  Read from a Nilan Air Vent System (Danish Brand) using a Wemos D1
  Mini (or other ESP8266-based board) and report the values to an MQTT
  broker. Then use it for your home-automation system like Home Assistant.

  External dependencies. Install using the Arduino library manager:

     "Arduino JSON V6 by Benoît Blanchon https://github.com/bblanchon/ArduinoJson - IMPORTANT - Use latest V.6 !!! This code won´t compile with V.5
     "ModbusMaster by Doc Walker https://github.com/4-20ma/ModbusMaster
     "PubSubClient" by Nick O'Leary https://github.com/knolleary/pubsubclient

  Project inspired by https://github.com/DanGunvald/NilanModbus
*/

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include "configuration.h"
#define SERIAL_SOFTWARE 1
#define SERIAL_HARDWARE 2
#if SERIAL_CHOICE == SERIAL_SOFTWARE
// for some reason this library keeps beeing included when building
// #include <SoftwareSerial.h>
#endif
#define HOST "NilanGW-%s" // Change this to whatever you like.
#define MAX_REG_SIZE 26
#define VENTSET 1003
#define RUNSET 1001
#define MODESET 1002
#define TEMPSET 1004
#define PROGRAMSET 500
#define COMPILED __DATE__ " " __TIME__

#define DEBUG_SCAN_TIME // Turn on/off debugging of scan times
#ifdef DEBUG_SCAN_TIME
// Scan time variables
#define SCAN_COUNT_MAX 20000
int scanTime = -1; // Used to measure scan times of program
int scanLast = -1;
int scanMax = -1;
int scanMin = 5000; // Set to a fake high number
double scanMovingAvr = 20;
int scanCount = 0;
#endif

#if SERIAL_CHOICE == SERIAL_SOFTWARE
SoftwareSerial SSerial(SERIAL_SOFTWARE_RX, SERIAL_SOFTWARE_TX); // RX, TX
#endif

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
char chipID[12];
const char *mqttServer = MQTT_SERVER;
const char *mqttUsername = MQTT_USERNAME;
const char *mqttPassword = MQTT_PASSWORD;
WiFiServer server(80);
WiFiClient wifiClient;
String IPaddress;
PubSubClient mqttClient(wifiClient);
long lastMsg = -MQTT_SEND_INTERVAL;
long modbusCooldown = 0;   // Used to limit modbus read/write operations
int modbusCooldownHit = 0; // Used to limit modbus read/write operations
int16_t rsBuffer[MAX_REG_SIZE];
ModbusMaster node;

int16_t AlarmListNumber[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 70, 71, 90, 91, 92};
String AlarmListText[] = {"NONE", "HARDWARE", "TIMEOUT", "FIRE", "PRESSURE", "DOOR", "DEFROST", "FROST", "FROST", "OVERTEMP", "OVERHEAT", "AIRFLOW", "THERMO", "BOILING", "SENSOR", "ROOM LOW", "SOFTWARE", "WATCHDOG", "CONFIG", "FILTER", "LEGIONEL", "POWER", "T AIR", "T WATER", "T HEAT", "MODEM", "INSTABUS", "T1SHORT", "T1OPEN", "T2SHORT", "T2OPEN", "T3SHORT", "T3OPEN", "T4SHORT", "T4OPEN", "T5SHORT", "T5OPEN", "T6SHORT", "T6OPEN", "T7SHORT", "T7OPEN", "T8SHORT", "T8OPEN", "T9SHORT", "T9OPEN", "T10SHORT", "T10OPEN", "T11SHORT", "T11OPEN", "T12SHORT", "T12OPEN", "T13SHORT", "T13OPEN", "T14SHORT", "T14OPEN", "T15SHORT", "T15OPEN", "T16SHORT", "T16OPEN", "ANODE", "EXCH INFO", "SLAVE IO", "OPT IO", "PRESET", "INSTABUS"};

String req[4]; // operation, group, address, value
enum ReqTypes
{
  reqtemp1 = 0,
  reqtemp2,
  reqtemp3,
  reqalarm,
  reqtime,
  reqcontrol,
  reqspeed,
  reqairtemp,
  reqairflow,
  reqairheat,
  reqprogram,
  requser,
  requser2,
  reqinfo,
  reqinputairtemp,
  reqapp,
  reqoutput,
  reqdisplay1,
  reqdisplay2,
  reqdisplay,
  reqmax
};

String groups[] = {"temp1", "temp2", "temp3", "alarm", "time", "control", "speed", "airtemp", "airflow", "airheat", "program", "user", "user2", "info", "inputairtemp", "app", "output", "display1", "display2", "display"};

// Start address to read from
int regAddresses[] = {203, 207, 221, 400, 300, 1000, 200, 1200, 1100, 0, 500, 600, 610, 100, 1200, 0, 100, 2002, 2007, 3000};

// How many values to read from based on start address
// byte regSizes[] = {23, 10, 6, 8, 2, 6, 2, 0, 1, 6, 6, 14, 7, 4, 26, 4, 4, 1};
byte regSizes[] = {2, 2, 1, 10, 6, 8, 2, 6, 2, 0, 1, 6, 6, 14, 1, 4, 26, 4, 4, 1};

// 0=raw, 1=x, 2 = return 2 characters ASCII,
// 4=xx, 8= return float dived by 1000,
byte regTypes[] = {8, 8, 8, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 2, 1, 4, 4, 8};

// Text translation of incoming data of the given address
char const *regNames[][MAX_REG_SIZE] = {
    // temp
    // {"T0_Controller", "T1_Intake", NULL, "T3_Exhaust", "T4_Outlet", NULL, NULL, "T7_Inlet", "T8_Outdoor", NULL, NULL, NULL, NULL, NULL, NULL, "T15_Room", NULL, NULL, NULL, NULL, NULL, "RH", NULL},
    {"T3_Exhaust", "T4_Outlet"},
    {"T7_Inlet", "T8_Outdoor", NULL, NULL, NULL, NULL, NULL, NULL, "T15_Room", NULL, NULL, NULL, NULL, NULL, "RH", NULL},
    {"RH"},
    // alarm
    {"Status", "List_1_ID", "List_1_Date", "List_1_Time", "List_2_ID", "List_2_Date", "List_2_Time", "List_3_ID", "List_3_Date", "List_3_Time"},
    // time
    {"Second", "Minute", "Hour", "Day", "Month", "Year"},
    // control
    {"Type", "RunSet", "ModeSet", "VentSet", "TempSet", "ServiceMode", "ServicePct", "Preset"},
    // speed
    {"ExhaustSpeed", "InletSpeed"},
    // airtemp
    {"CoolSet", "TempMinSum", "TempMinWin", "TempMaxSum", "TempMaxWin", "TempSummer"},
    // airflow
    {"AirExchMode", "CoolVent"},
    // airheat
    {},
    // program
    {"Program"},
    // program.user
    {"UserFuncAct", "UserFuncSet", "UserTimeSet", "UserVentSet", "UserTempSet", "UserOffsSet"},
    // program.user2 requires the optional print board
    {"User2FuncAct", "User2FuncSet", "User2TimeSet", "User2VentSet", "User2TempSet", "User2OffsSet"},
    // info
    {"UserFunc", "AirFilter", "DoorOpen", "Smoke", "MotorThermo", "Frost_overht", "AirFlow", "P_Hi", "P_Lo", "Boil", "3WayPos", "DefrostHG", "Defrost", "UserFunc_2"},
    // inputairtemp
    {"IsSummer", "TempInletSet", "TempControl", "TempRoom", "EffPct", "CapSet", "CapAct"},
    // app
    {"Bus.Version", "VersionMajor", "VersionMinor", "VersionRelease"},
    // output
    {"AirFlap", "SmokeFlap", "BypassOpen", "BypassClose", "AirCircPump", "AirHeatAllow", "AirHeat_1", "AirHeat_2", "AirHeat_3", "Compressor", "Compressor_2", "4WayCool", "HotGasHeat", "HotGasCool", "CondOpen", "CondClose", "WaterHeat", "3WayValve", "CenCircPump", "CenHeat_1", "CenHeat_2", "CenHeat_3", "CenHeatExt", "UserFunc", "UserFunc_2", "Defrosting"},
    // display1
    {"Text_1_2", "Text_3_4", "Text_5_6", "Text_7_8"},
    // display2
    {"Text_9_10", "Text_11_12", "Text_13_14", "Text_15_16"},
    // air bypass
    {"AirBypass/IsOpen"}};

char const *getName(ReqTypes type, int address)
{
  if (address >= 0 && address <= regSizes[type])
  {
    return regNames[type][address];
  }
  return NULL;
}

void modbusCool(int coolDownTimeMS)
{
  // Fix for breaking out of modbus error loop
  if ((long)millis() < (long)modbusCooldown)
  {
    if (modbusCooldownHit > 50)
    {
      ESP.reset();
    }
    modbusCooldownHit++;
    while ((long)millis() < (long)modbusCooldown)
    {
      delay(20);
    }
  }
  else if (modbusCooldownHit > 0)
  {
    modbusCooldownHit = 0;
  }
  modbusCooldown = millis() + coolDownTimeMS;
}

char WriteModbus(uint16_t addr, int16_t val)
{
  modbusCool(200);
  node.setTransmitBuffer(0, val);
  char result = 0;
  result = node.writeMultipleRegisters(addr, 1);
  return result;
}

char ReadModbus(uint16_t addr, uint8_t sizer, int16_t *vals, int type)
{
  modbusCool(200);
  char result = 0;
  // Make sure type is either 0 or 1
  switch (type & 1)
  {
  case 0:
    result = node.readInputRegisters(addr, sizer);
    break;
  case 1:
    result = node.readHoldingRegisters(addr, sizer);
    break;
  }
  if (result == node.ku8MBSuccess)
  {
    for (int j = 0; j < sizer; j++)
    {
      vals[j] = node.getResponseBuffer(j);
    }
    return result;
  }
  return result;
}

JsonObject HandleRequest(JsonDocument &doc)
{
  JsonObject root = doc.to<JsonObject>();
  ReqTypes r = reqmax;
  if (req[1] != "")
  {
    for (int i = 0; i < reqmax; i++)
    {
      if (groups[i] == req[1])
      {
        r = (ReqTypes)i;
      }
    }
  }
  char type = regTypes[r];
  if (req[0] == "read")
  {
    int address = 0;
    int nums = 0;
    char result = -1;
    address = regAddresses[r];
    nums = regSizes[r];

    result = ReadModbus(address, nums, rsBuffer, type);
    if (result == 0)
    {
      root["status"] = "Modbus connection OK";
      for (int i = 0; i < nums; i++)
      {
        char const *name = getName(r, i);
        if (name != NULL && strlen(name) > 0)
        {
          if ((type == 2 && i > 0) || type == 4)
          {
            String str = "";
            str += (char)(rsBuffer[i] & 0x00ff);
            str = (char)(rsBuffer[i] >> 8) + str;
            // Remove leading space from one character string
            str.trim();
            root[name] = str;
          }
          else if (type == 8)
          {
            root[name] = rsBuffer[i] / 100.0;
          }
          else
          {
            root[name] = rsBuffer[i];
          }
        }
      }
    }
    else
    {
      root["status"] = "Modbus connection failed";
    }
    root["requestAddress"] = address;
    root["requestNumber"] = nums;
  }
  else if (req[0] == "set" && req[2] != "" && req[3] != "")
  {
    int address = atoi(req[2].c_str());
    int value = atoi(req[3].c_str());
    char result = WriteModbus(address, value);
    root["result"] = result;
    root["address"] = address;
    root["value"] = value;
  }
  else if (req[0] == "get" && req[1] >= "0" && req[2] > "0")
  {
    int address = atoi(req[1].c_str());
    int nums = atoi(req[2].c_str());
    int type = atoi(req[3].c_str());
    char result = ReadModbus(address, nums, rsBuffer, type);
    if (result == 0)
    // if (true)
    {
      root["status"] = "Modbus connection OK";
      for (int i = 0; i < nums; i++)
      {
        root[String("address" + String(address + i))] = rsBuffer[i];
      }
    }
    else
    {
      root["status"] = "Modbus connection failed";
    }
    root["result"] = result;
    root["requestAddress"] = address;
    root["requestNumber"] = nums;
    switch (type)
    {
    case 0:
      root["type"] = "Input register";
      break;
    case 1:
      root["type"] = "Holding register";
      break;
    default:
      root["type"] = "Should be 0 or 1 for input/holding register";
    }
  }
  else if (req[0] == "help" || req[0] == "")
  {
    for (int i = 0; i < reqmax; i++)
    {
      root[groups[i]] = "http://../read/" + groups[i];
    }
  }
  root["operation"] = req[0];
  root["group"] = req[1];
  return root;
}

void mqttReconnect()
{
  int numberRetries = 0;
  while (!mqttClient.connected() && numberRetries < 3)
  {
    if (mqttClient.connect(chipID, mqttUsername, mqttPassword, "ventilation/alive", 1, true, "0"))
    {
      mqttClient.publish("ventilation/alive", "1", true);
      mqttClient.subscribe("ventilation/cmd/+");
      return;
    }
    else
    {
      delay(1000);
    }
    numberRetries++;
  }
  if (numberRetries >= 3)
  {
    delay(5000);
    // Give up and do a reboot
    ESP.restart();
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String inputString;
  for (unsigned int i = 0; i < length; i++)
  {
    inputString += (char)payload[i];
  }
  // Check if topic is equal to string
  if (strcmp(topic, "ventilation/cmd/ventset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t speed = payload[0] - '0';
      WriteModbus(VENTSET, speed);
      mqttClient.publish("ventilation/cmd/ventset", "", true);
    }
  }
  else if (strcmp(topic, "ventilation/cmd/modeset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t mode = payload[0] - '0';
      WriteModbus(MODESET, mode);
      mqttClient.publish("ventilation/cmd/modeset", "", true);
    }
  }
  else if (strcmp(topic, "ventilation/cmd/runset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '1')
    {
      int16_t run = payload[0] - '0';
      WriteModbus(RUNSET, run);
      mqttClient.publish("ventilation/cmd/runset", "", true);
    }
  }
  else if (strcmp(topic, "ventilation/cmd/tempset") == 0)
  {
    if (length == 4 && payload[0] >= '0' && payload[0] <= '2')
    {
      WriteModbus(TEMPSET, inputString.toInt());
      mqttClient.publish("ventilation/cmd/tempset", "", true);
    }
  }
  else if (strcmp(topic, "ventilation/cmd/programset") == 0)
  {
    if (length == 1 && payload[0] >= '0' && payload[0] <= '4')
    {
      int16_t program = payload[0] - '0';
      WriteModbus(PROGRAMSET, program);
      mqttClient.publish("ventilation/cmd/programset", "", true);
    }
  }
  else if (strcmp(topic, "ventilation/cmd/update") == 0)
  {
    // Enter mode in 60 seconds to prioritize OTA
    if (payload[0] == '1')
    {
      mqttClient.publish("ventilation/cmd/update", "2");
      for (unsigned int i = 0; i < 300; i++)
      {
        ArduinoOTA.handle();
        delay(200);
      }
      if (!mqttClient.connected())
      {
        mqttReconnect();
      }
    }
    mqttClient.publish("ventilation/cmd/update", "0");
  }
  else if (strcmp(topic, "ventilation/cmd/reboot") == 0)
  {
    if (payload[0] == '1')
    {
      mqttClient.publish("ventilation/cmd/reboot", "0");
      ESP.restart();
    }
  }
  else if (strcmp(topic, "ventilation/cmd/version") == 0)
  {
    if (inputString != String(COMPILED))
    {
      mqttClient.publish(topic, String(COMPILED).c_str());
    }
  }
  else
  {
    mqttClient.publish("ventilation/error/topic", topic);
  }
  lastMsg = -MQTT_SEND_INTERVAL;
}

bool readRequest(WiFiClient &client)
{
  req[0] = "";
  req[1] = "";
  req[2] = "";
  req[3] = "";

  int n = -1;
  while (client.connected())
  {
    if (client.available())
    {
      char c = client.read();
      if (c == '\n')
      {
        return false;
      }
      else if (c == '/')
      {
        n++;
      }
      else if (c != ' ' && n >= 0 && n < 4)
      {
        req[n] += c;
      }
      else if (c == ' ' && n >= 0 && n < 4)
      {
        return true;
      }
    }
  }

  return false;
}

void writeResponse(WiFiClient &client, const JsonDocument &doc)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  // Fix: To adhere to RFC2616 section 14.13. Calculate length of data to client
  String response = "";
  serializeJsonPretty(doc, response);
  client.print("Content-Length: ");
  client.println(response.length());
  client.println();
  client.print(response);
}

void setup()
{
  char host[64];
  sprintf(chipID, "%08X", ESP.getChipId());
  sprintf(host, HOST, chipID);
#if USE_WIFI_LED
  pinMode(WIFI_LED, OUTPUT);
  digitalWrite(WIFI_LED, LOW); // Reverse meaning. LOW=LED ON
#endif
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(host);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(5000);
    // Give up and do a reboot
    ESP.restart();
  }
#if USE_WIFI_LED
  digitalWrite(WIFI_LED, HIGH);
#endif
  ArduinoOTA.setHostname(host);
  ArduinoOTA.begin();
  server.begin();

#if SERIAL_CHOICE == SERIAL_SOFTWARE
#warning Compiling for software serial
  SSerial.begin(19200, SWSERIAL_8E1);
  node.begin(MODBUS_SLAVE_ADDRESS, SSerial);
#elif SERIAL_CHOICE == SERIAL_HARDWARE
#warning Compiling for hardware serial
  Serial.begin(19200, SERIAL_8E1);
  node.begin(MODBUS_SLAVE_ADDRESS, Serial);
#else
#error hardware og serial serial port?
#endif

  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(mqttCallback);
  mqttReconnect();
  mqttClient.publish("ventilation/gateway/boot", String(millis()).c_str());
  IPaddress = WiFi.localIP().toString();
  mqttClient.publish("ventilation/gateway/ip", IPaddress.c_str());
}

#ifdef DEBUG_SCAN_TIME
// Scan time is the time then looping part of a program runs in miliseconds.
// Rule of thumb is to allow max 20ms to rule the program as runnning "live" and not async
// Live running programs are relevant when expecting non buffered IO operations with the real world
void scanTimer()
{
  if (scanCount > SCAN_COUNT_MAX)
  {
    return;
  }
  if (scanLast == -1)
  {
    scanLast = millis();
    return;
  }
  scanTime = millis() - scanLast;
  if (scanTime > scanMax)
    scanMax = scanTime;
  if (scanTime < scanMin)
    scanMin = scanTime;
  scanCount++;
  scanMovingAvr = scanTime * (0.3 / (1 + scanCount)) + scanMovingAvr * (1 - (0.3 / (1 + scanCount)));
  if (scanCount > SCAN_COUNT_MAX)
  {
    mqttClient.publish("ventilation/debug/scanMovingAvr", String(floor(scanMovingAvr * 100) / 100).c_str());
  }
  scanLast = millis();
}
#endif

void loop()
{
  ArduinoOTA.handle();
  WiFiClient client = server.available();
  if (client)
  {
    bool success = readRequest(client);
    if (success)
    {
      StaticJsonDocument<1000> doc;
      HandleRequest(doc);
      writeResponse(client, doc);
    }
    client.stop();
  }

  if (!mqttClient.connected())
  {
    mqttReconnect();
  }

  if (mqttClient.connected())
  {
    mqttClient.loop();
    long now = millis();
    if (now - lastMsg > MQTT_SEND_INTERVAL)
    {
      //  ReqTypes rr[] = {reqtemp, reqcontrol, reqtime, reqoutput, reqspeed, reqalarm, reqinputairtemp, reqprogram, requser, reqdisplay, reqinfo}; // put another register in this line to subscribe
      ReqTypes rr[] = {reqtemp1, reqtemp2, reqtemp3, reqcontrol, reqalarm, reqinputairtemp, reqprogram, reqdisplay, requser}; // put another register in this line to subscribe
      for (unsigned int i = 0; i < (sizeof(rr) / sizeof(rr[0])); i++)
      {
        ReqTypes r = rr[i];
        char result = ReadModbus(regAddresses[r], regSizes[r], rsBuffer, regTypes[r]);
        if (result == 0)
        {
          mqttClient.publish("ventilation/error/modbus", "0"); // no error when connecting through modbus
          for (int i = 0; i < regSizes[r]; i++)
          {
            char const *name = getName(r, i);
            char numberString[10];
            if (name != NULL && strlen(name) > 0)
            {
              String mqttTopic;
              switch (r)
              {
              case reqcontrol:
                mqttTopic = "ventilation/control/"; // Subscribe to the "control" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case reqtime:
                mqttTopic = "ventilation/time/"; // Subscribe to the "output" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case reqoutput:
                mqttTopic = "ventilation/output/"; // Subscribe to the "output" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case reqdisplay:
                mqttTopic = "ventilation/display/"; // Subscribe to the "input display" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case reqspeed:
                mqttTopic = "ventilation/speed/"; // Subscribe to the "speed" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case reqalarm:
                mqttTopic = "ventilation/alarm/"; // Subscribe to the "alarm" register

                switch (i)
                {
                case 1: // Alarm.List_1_ID
                case 4: // Alarm.List_2_ID
                case 7: // Alarm.List_3_ID
                  if (rsBuffer[i] > 0)
                  {
                    // itoa((rsBuffer[i]), numberString, 10);
                    sprintf(numberString, "UNKNOWN"); // Preallocate unknown if no match if found
                    for (unsigned int p = 0; p < (sizeof(AlarmListNumber)); p++)
                    {
                      if (AlarmListNumber[p] == rsBuffer[i])
                      {
                        //   memset(numberString, 0, sizeof numberString);
                        //   strcpy (numberString,AlarmListText[p].c_str());
                        sprintf(numberString, AlarmListText[p].c_str());
                        break;
                      }
                    }
                  }
                  else
                  {
                    sprintf(numberString, "None"); // No alarm, output None
                  }
                  break;
                case 2: // Alarm.List_1_Date
                case 5: // Alarm.List_2_Date
                case 8: // Alarm.List_3_Date
                  if (rsBuffer[i] > 0)
                  {
                    sprintf(numberString, "%d", (rsBuffer[i] >> 9) + 1980);
                    sprintf(numberString + strlen(numberString), "-%02d", (rsBuffer[i] & 0x1E0) >> 5);
                    sprintf(numberString + strlen(numberString), "-%02d", (rsBuffer[i] & 0x1F));
                  }
                  else
                  {
                    sprintf(numberString, "N/A"); // No alarm, output N/A
                  }
                  break;
                case 3: // Alarm.List_1_Time
                case 6: // Alarm.List_2_Time
                case 9: // Alarm.List_3_Time
                  if (rsBuffer[i] > 0)
                  {
                    sprintf(numberString, "%02d", rsBuffer[i] >> 11);
                    sprintf(numberString + strlen(numberString), ":%02d", (rsBuffer[i] & 0x7E0) >> 5);
                    sprintf(numberString + strlen(numberString), ":%02d", (rsBuffer[i] & 0x11F) * 2);
                  }
                  else
                  {
                    sprintf(numberString, "N/A"); // No alarm, output N/A
                  }

                  break;
                default: // used for Status bit (case 0)
                  itoa((rsBuffer[i]), numberString, 10);
                }
                break;
              case reqinputairtemp:
                mqttTopic = "ventilation/inputairtemp/"; // Subscribe to the "inputairtemp" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case reqprogram:
                mqttTopic = "ventilation/weekprogram/"; // Subscribe to the "week program" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case requser:
                mqttTopic = "ventilation/user/"; // Subscribe to the "user" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case requser2:
                mqttTopic = "ventilation/user/"; // Subscribe to the "user2" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case reqinfo:
                mqttTopic = "ventilation/info/"; // Subscribe to the "info" register
                itoa((rsBuffer[i]), numberString, 10);
                break;
              case reqtemp1:
                if (strncmp("RH", name, 2) == 0)
                {
                  mqttTopic = "ventilation/moist/"; // Subscribe to moisture-level
                }
                else
                {
                  mqttTopic = "ventilation/temp/"; // Subscribe to "temp" register
                }
                dtostrf((rsBuffer[i] / 100.0), 5, 2, numberString);
                break;
              case reqtemp2:
                if (strncmp("RH", name, 2) == 0)
                {
                  mqttTopic = "ventilation/moist/"; // Subscribe to moisture-level
                }
                else
                {
                  mqttTopic = "ventilation/temp/"; // Subscribe to "temp" register
                }
                dtostrf((rsBuffer[i] / 100.0), 5, 2, numberString);
                break;
              case reqtemp3:
                if (strncmp("RH", name, 2) == 0)
                {
                  mqttTopic = "ventilation/moist/"; // Subscribe to moisture-level
                }
                else
                {
                  mqttTopic = "ventilation/temp/"; // Subscribe to "temp" register
                }
                dtostrf((rsBuffer[i] / 100.0), 5, 2, numberString);
                break;
              default:
                // If not all enumerations possibilities are handled then message are added to the unmapped topic
                mqttTopic = "ventilation/unmapped/";
                break;
              }
              mqttTopic += (char *)name;
              mqttClient.publish(mqttTopic.c_str(), numberString);
            }
          }
        }
        else
        {
          mqttClient.publish("ventilation/error/modbus", "1"); // error when connecting through modbus
        }
      }
      lastMsg = now;
      if (now > (long)1288490187)
      {
        // Fix to make sure the command millis() dont overflow. This happens after 50 days and would mess up some logic above
        // Reboot if ESP has been running for approximately 30 days.
        ESP.restart();
      }
    }
  }
#ifdef DEBUG_SCAN_TIME
  scanTimer();
#endif
}