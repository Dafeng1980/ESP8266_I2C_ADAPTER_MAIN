void setWifiMqtt(){
  int k = 0; 
  Log.notice(CR );
  Log.notice(F("ESP8266 Device ID topic: %s." CR), DEVICE_ID_Topic);
  delay(10);
  eeprom_read_setup();
  Log.noticeln("Connecting to %s", eep.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(eep.ssid, eep.password);
  while (WiFi.status() != WL_CONNECTED) {
    wifistatus = true;
    delay(500);
    k++;
    ledflash();
    // Log.notice(".");
    Serial.print(".");
    if( k >= 60){
        wifistatus = false;
        break;
     }
  }   
      // randomSeed(micros());
      Serial.println(".");
      if(wifistatus){
      Log.errorln("WiFi Connected.");
      Log.errorln("IP address: %s", WiFi.localIP().toString().c_str());			
      client.setCallback(callback);
      String client_id;
      client_id = clientID + String(WiFi.macAddress());
      Log.errorln("Client_id = %s", client_id.c_str());
      client.setServer(eep.mqtt_broker, mqtt_port);
      delay(100);
      if(client.connect(client_id.c_str(), eep.mqtt_user, eep.mqtt_password)) {
          mqttflag = true;
          Log.errorln("MQTT Broker Connected.");
          snprintf (msg, MSG_BUFFER_SIZE, "IP address: %s, Broker: %s Connected", WiFi.localIP().toString().c_str(), eep.mqtt_broker);
          pub("sysTopic", msg);
          sub("i2cpub/#");
          i2cdetects(0x03, 0x7F);
          Log.begin(LOG_ERROR, &Serial, false);
        } 
      else{
          mqttflag = false;        
          Log.errorln("MQTT Broker Connected Failed");
      }
    }
  else Log.errorln("WiFi Connecting Failed");
  delay(100);
}

void eeprom_read_setup(){
    char c[6];
    uint8_t host = EEPROM.read(0x00);
    if(host == 0x80) EEPROM.get(0, eep);
    else if(host == 0x40) EEPROM.get(100, eep);
    else {
        strncpy(eep.ssid, ssid, 16);
        strncpy(eep.password, password, 16);
        strncpy(eep.mqtt_broker, mqtt_server, 31);
        strncpy(eep.mqtt_user, mqtt_user, 16);
        strncpy(eep.mqtt_password, mqtt_password, 16);
    }
    sprintf(c, "0x%02X:", host);
    Log.noticeln("EE read Host:%s", c);
}

void subMQTT(const char* topic) {
   if(client.subscribe(topic)){
     Log.traceln(F("Subscription OK to the subjects %s"), topic);;
  } else {
    Log.traceln(F("Subscription Failed, rc=%d"), client.state());
  }
}

void sub(const char* topicori) {
  String topic = String(mqtt_topic) + String(topicori);
  subMQTT(topic);
}

void subMQTT(String topic) {
  subMQTT(topic.c_str());
}

void pubMQTT(const char* topic, const char* payload, bool retainFlag) {
  if (client.connected()) {
    Log.traceln(F("[MQTT_publish] topic: %s msg: %s "), topic, payload);
    client.publish(topic, payload, retainFlag);
  } else {
    Log.traceln(F("Client not connected, aborting thes publication"));
  }
}

void pubMQTT(const char* topic, const char* payload) {
  pubMQTT(topic, payload, false);
}

void pubMQTT(String topic, const char* payload) {
  pubMQTT(topic.c_str(), payload);
}

void pubMQTT(String topic, String payload) {
  pubMQTT(topic.c_str(), payload.c_str());
}

void pub(const char* topicori, const char* payload) {
  String topic = String(mqtt_topic) + String(topicori);
  pubMQTT(topic, payload);
}

void pub(const char* topicori, JsonObject& data) {
  String dataAsString = "";
  serializeJson(data, dataAsString);
  String topic = String(mqtt_topic) + String(topicori);
  pubMQTT(topic, dataAsString.c_str());
}

void callback(char* topic, byte* payload, unsigned int length) {
    // String inPayload = "";
    // String currtopic = String(mqtt_topic) + "scpi/set/curr";
   //    byte* p = (byte*)malloc(length + 1);
   //    memcpy(p, payload, length);
   //    p[length] = '\0';
    Log.noticeln(F("Message arrived [ %s ]" ), topic);
   if ((char)payload[0] == '[') {
      for(int i = 1; i < length; i++){   
          i2cbus_d[i-1] = tohex(payload[3*i-2])*16 + tohex(payload[3*i-1]);
          if (i >= 127) {
              Log.errorln(F("Iic_pub Invalid format"));
              pub("status", "Iic_pub Invalid format");
              commandflag = false;
              delay(10);
              break; 
            }
          if (payload[3*i] == ']'){
              commandflag = true;
              break;
          }                       
          if(payload[3*i] != ' ' && 3*i >= (length-1)) {
             Log.errorln(F("Space Fail, or No ] Fuffix, Iic_pub Invalid Format"));
             pub("status", "Space Fail, or No ] Suffix, Iic_pub Invalid Format");
              commandflag = false;
              delay(10);
              break;
          }
      }        
    }
  Log.noticeln("");
}

void mqttLoop(){
  if(wifistatus){
    if (!client.connected()) {
            reconnect();
            sub("i2cpub/#");       
        }
    if (mqttflag){    
          client.loop();      
        }
    }
  else {
       setWifiMqtt();
    }
}

void reconnect() {
    // Loop until we're reconnected  
  int k = 0;//client.connect(clientID, mqtt_user, mqtt_password);
  while (!client.connected()) {
    Serial.println(F("Attempting MQTT connection..."));
    //  Log.noticeln(F("Attempting MQTT connection...")); // Attempt to connect   
       //     String clientId = "ESP8266Client-";
       //     clientId += String(random(0xffff), HEX);
      String client_id;
      client_id = clientID + String(WiFi.macAddress());
      client.setServer(eep.mqtt_broker, mqtt_port);
    if (client.connect(client_id.c_str(), mqtt_user, mqtt_password)) {
      Serial.println(F("connected to broker"));
      pub("sysTopic", "Broker reconnected");  // Once connected, publish an announcement...
      mqttflag = true;   
    }
    else {
      // Log.notice("Failed, rc= %d", client.state());
      Serial.printf("Failed, rc= %d\n", client.state());
      Serial.println(F(" try again in 2 seconds"));
      k++;
      delay(2000);   // Wait 2 seconds before retrying
        if( k >= 5){
                mqttflag = false;
                wifistatus = false;
                Serial.println(F("WiFi connect Failed!!"));
                // Log.begin(LOG_LEVEL, &Serial, false);
                delay(100);
                break;
            }                   
        }
    }
}

uint8_t tohex(uint8_t val){
  uint8_t hex;
  if ( val - '0' >= 0 && val - '0' <=9){
     hex = val - '0';
  }
  else if( val == 'a' || val == 'A') hex = 10;
  else if( val == 'b' || val == 'B') hex = 11;
  else if( val == 'c' || val == 'C') hex = 12;
  else if( val == 'd' || val == 'D') hex = 13;
  else if( val == 'e' || val == 'E') hex = 14;
  else if( val == 'f' || val == 'F') hex = 15;
  else {
      snprintf (msg, MSG_BUFFER_SIZE, "Invalid Data Format");
      pub("status", msg);
      Log.errorln("%s", msg);
    // Log.noticeln(F("Invalid Hex Data" ));
    // dataflag = false;
    delay(10);
    return 0;
  }
  // dataflag = true;
  return hex;
}

void ledflash(){
  digitalWrite(kLedPin, !digitalRead(kLedPin));
}

void set_custom(uint8_t hostval){
  EEPROM.write(0x00, hostval);
  EEPROM.commit();
  Log.noticeln("Host Write:%x", hostval);
  delay(20);
}

void renew_wifi_broker(){
      Log.noticeln("");
      // uint8_t host;
      if(EEPROM.read(0) & 0x40)
        EEPROM.get(100, eep);
      else EEPROM.get(0, eep);
      Log.noticeln(F("Old host config: %x"), eep.host);
      Serial.println("Old values read: "+String(eep.ssid)+", "+String(eep.password)+", "+String(eep.mqtt_broker));
      Log.noticeln(F("Input host set: (0x00 default; 0x80 update from EE1; 0x40 update from EE2.)"));
      eep.host = read_int();     
      Log.noticeln(F("New host set: %x"), eep.host);

      Log.noticeln(F("Do you want to update the next wifi seting. Yes(Y), No(N):"));
      char val = read_char(); 
      if ((char)val == 'y'  || (char)val == 'Y') { 
      Log.noticeln(F("Input WiFi ssid:"));
      strncpy(eep.ssid, read_string(), 16);
      Log.noticeln(F("New WiFi ssid: %s"), eep.ssid);
      Log.noticeln(F("Input WiFi password:"));
      strncpy(eep.password, read_string(), 16);
      Log.noticeln(F("New WiFi password: %s"), eep.password);
      Log.noticeln(F("Do you want to update the broker. Yes(Y), No(N):"));
      char val = read_char(); 
      if ((char)val == 'y'  || (char)val == 'Y') {
        set_broker();
      }

      else {
        if(eep.host & 0x40)
            EEPROM.put(100,eep);
        else EEPROM.put(0,eep);
        Log.noticeln(F("Commit the new data to EEPROM. Yes(Y), No(N):"));
          char val = read_char(); 
        if ((char)val == 'y'  || (char)val == 'Y') {
            EEPROM.commit();
            Log.noticeln(F("EEprom Write Save"));
            delay(20);
          }
      else Log.noticeln(F("Without Save Exit"));
      }
    }

    else {
         set_custom(eep.host);  
    }
}

void set_broker(){
     Log.noticeln(F("Input New MQTT broker:"));
     strncpy(eep.mqtt_broker, read_string(), 31);
     Log.noticeln(F("New MQTT broker: %s"), eep.mqtt_broker);
     Log.noticeln(F("Input New MQTT user:"));
     strncpy(eep.mqtt_user, read_string(), 16);
     Log.noticeln(F("New MQTT user: %s"), eep.mqtt_user);
     Log.noticeln(F("Input New MQTT password:"));
     strncpy(eep.mqtt_password, read_string(), 16);
     Log.noticeln(F("New MQTT password: %s"), eep.mqtt_password);
     if(eep.host & 0x40)
          EEPROM.put(100,eep);
     else EEPROM.put(0,eep);
     Log.noticeln(F("Commit the new data to EEPROM. Yes(Y), No(N):"));
      char val;
      val = read_char(); 
      if ((char)val == 'y'  || (char)val == 'Y') {
      EEPROM.commit();
      Log.noticeln(F("Data Commit"));
      delay(20);
      }
      else Log.noticeln(F("Without Save/Commit Exit "));
}

void esprestar(){
    Log.noticeln(F("delay 3S Reset "));
    for(int i = 0; i < 6; i++){
      delay(500);
      Log.notice(" .");
    }
    ESP.restart();
}

void printhelp(){
      Log.notice("" CR); 
      Log.notice("" CR);  
      Log.noticeln(F("************* I2C MQTT ADAPTER **************"));
      Log.notice(F(" - > ESP8266 Device ID topic: %s " CR), DEVICE_ID_Topic);
      Log.notice(F(" 0 > pub [AA 00] 00/40/80 Set the host val" CR));  // 0x00 for default, 0x80 Custom eep1, 0x40 Custom eep2
      Log.notice(F(" 1 > pub [AA 01 xx xx] Set adapter status poll time /ms" CR));
      Log.notice(F(" 3 > pub [AA BB] RESET DEVICE" CR));     
      delay(100);  
}
