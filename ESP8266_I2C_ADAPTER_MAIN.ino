
/*ESP8266 I2C ADAPTER: ESP-01S Board SDA = 0; SCL = 2; ESP-12F Board SDA = 4; SCl = 0;            
 *  Author Dafeng 2024
*/
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ArduinoLog.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
// #include <EasyNTPClient.h>
// #include <WiFiUdp.h>

#define DEVICE_ID_Topic "i2cAdapter/"
#define TWI_BUFFER_SIZE (256)
#define MSG_BUFFER_SIZE  (1024)
#define UI_BUFFER_SIZE (256)
#define LOG_LEVEL  LOG_LEVEL_VERBOSE
#define LOG_SILENT LOG_LEVEL_SILENT
#define LOG_ERROR  LOG_LEVEL_ERROR
#define I2C_NOSTOP 0
#define SECONDS_FROM_1970_TO_2000  946684800 //< Unixtime for 2000-01-01 00:00:00, useful for initialization
//#define MQTT_MAX_PACKET_SIZE 1024  
WiFiClient eClient;
PubSubClient client(eClient);
// WiFiUDP udp;
// // EasyNTPClient ntpClient(udp, "pool.ntp.org", ((5*60*60)+(30*60))); // IST = GMT + 5:30
// EasyNTPClient ntpClient(udp, "ntp.aliyun.com", ((8*60*60)+(0*60))); // CNT = GMT + 8:00
// EasyNTPClient ntpClient(udp, "ntp.aliyun.com", ((0*60*60)+(0*60))); // GMT + 0:00

const int SDA_PIN = 14;         //ESP-01S Board SDA = 2; SCL = 0;   
const int SCL_PIN = 0;          //ESP8266 HEKR 1.1 Board  SDA = 14; SCL = 0; kLedPin = 4;
const uint8_t kLedPin = 4;      // ESP-12F Board SDA = 4; SCl = 0; kLedPin = 12;
const uint8_t kButtonPin = 13;     

static struct DateTime
{
  uint16_t year;
  uint8_t month;
  uint8_t week;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  unsigned long unixtime; 
}dt;

static struct eeprom
{
  uint8_t host;
  char ssid[16];
  char password[16];
  char mqtt_broker[31];
  char mqtt_user[16];
  char mqtt_password[16];
}eep;

uint8_t i2cbus_d[256];
static uint8_t eepbuffer[256];
static uint16_t lgInterval = 1000;  //i2c refresh rate (miliseconds) 
static bool eepromsize = true;   // true (eeprom data size > 0x100). 
static bool Protocol = true;   // If true, endTransmission() sends a stop message after transmission, releasing the I2C bus.
static bool wifistatus = true;
static bool mqttflag = false;
static bool scanflag = false;
static bool commandflag = false;       
const char* ssid = "XXXXX";           // Enter your WiFi name Local Network
const char* password = "XXXXXX";    // Enter WiFi password
const char* mqtt_user = "homeiot";    
const char* mqtt_password = "abcd1234";
const char* mqtt_server = "192.168.1.88";   //Local broker service   //Raspberry MQTT Broker
const uint16_t mqtt_port =  1883;
const char* clientID = "i2cadapter_";
const char hex_table[]{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
// const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30};

char mqtt_topic[50] = DEVICE_ID_Topic;
char msg[MSG_BUFFER_SIZE];
char ui_buffer[UI_BUFFER_SIZE];
char displaybuff[16];
unsigned long previousMillis = 0;
unsigned long count = 0;

void setup() { 
    pinMode(kButtonPin, INPUT_PULLUP);
    pinMode(kLedPin, OUTPUT);
    digitalWrite(kLedPin, LOW);
    Serial.begin(38400);
    EEPROM.begin(512);
    Log.begin(LOG_LEVEL, &Serial, false);  //
    Wire.begin(SDA_PIN, SCL_PIN);
    printhelp(); 
    if (digitalRead(kButtonPin) == 0){
          delay(20);
        if(digitalRead(kButtonPin) == 0) 
        {
          delay(100);
          renew_wifi_broker();
        }       
      } 
    setWifiMqtt();
    // dt.unixtime = ntpClient.getUnixTime();
    // unixtime2datatime(dt.unixtime);
    // snprintf (displaybuff, 16, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
 }

void loop() {   
   mqttLoop();
   checkSubComm();
}

// void unixtime2datatime(uint32_t t) {
//   t -= SECONDS_FROM_1970_TO_2000; // bring to 2000 timestamp from 1970
//   dt.sec = t % 60;
//   t /= 60;
//   dt.min = t % 60;
//   t /= 60;
//   dt.hour = t % 24;
//   uint16_t days = t / 24;
//   dt.week = (days + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
//   uint8_t leap;
//   for (dt.year = 0;; ++dt.year) {
//     leap = dt.year % 4 == 0;
//     if (days < 365U + leap){
//       dt.year = dt.year + 2000;
//       break;
//     }
//     days -= 365 + leap;
//   }
//   for (dt.month = 1; dt.month < 12; ++dt.month) {
//     uint8_t daysPerMonth = daysInMonth[dt.month - 1];
//     if (leap && dt.month == 2)
//       ++daysPerMonth;
//     if (days < daysPerMonth)
//       break;
//     days -= daysPerMonth;
//   }
//   dt.day = days + 1;
// }
