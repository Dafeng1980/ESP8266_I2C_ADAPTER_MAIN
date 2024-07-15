void checkSubComm(){
  if (commandflag) {
      if (i2cbus_d[0] >= 0 && i2cbus_d[0] <=9) eeprom_transfer_sent(i2cbus_d[0]);                
      if(i2cbus_d[0] == 0xAA){
         if(i2cbus_d[1] == 0x00) set_custom(i2cbus_d[2]);    //set WiFI MQTT broker from EEPROM                                                   // set pmbus //sent smbus command by subscribe("***/pmbus/set")
         else if(i2cbus_d[1] == 0x01) lgInterval = (i2cbus_d[2]<<8)  + i2cbus_d[3];  //[AA 01 XX XX] Set pmbus poll time /ms;
         else if(i2cbus_d[1] == 0x02) scanflag = !scanflag;
         else if(i2cbus_d[1] == 0xBB) esprestar();                 //reset device
       }
       commandflag = false;
       previousMillis = millis();
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= lgInterval && !commandflag){
        previousMillis = currentMillis;
        ledflash();
        publishLog();
        if(scanflag) i2cdetects(0x03, 0x7F); 
        count++;             
     }
}

void publishLog(){
  DynamicJsonDocument doc(1024);
  JsonObject adapter = doc.createNestedObject("adapter"); 
  if ((count & 3) == 0) {
          snprintf (msg, MSG_BUFFER_SIZE, "ADAPTER_Refresh #%ld", count >> 2);
          pub("status", msg);
          adapter["refresh"] = msg;
        }
  adapter["counter"] =  count;
  adapter["date"] =  displaybuff;
  adapter["scanflag"] =  scanflag;
  // String jsonString;
  // serializeJson(doc, jsonString);
  pub("status/jsoninfo", adapter); 
}

void i2cdetects(uint8_t first, uint8_t last) {
  uint8_t i, address, rerror;
  int q = 0;
  char c[6];
  char addr[35];
  Log.notice("   ");            // table header
  for (i = 0; i < 16; i++) {
    sprintf(c, "%3x",  i);
    Log.notice("%s", c);
  }
  for (address = 0; address <= 127; address++) {             // addresses 0x00 through 0x77
    if (address % 16 == 0) {                            // table body
          sprintf(c, "0x%02x:", address & 0xF0);
          Log.notice(CR "%s", c);
      }
    if (address >= first && address <= last) {
        Wire.beginTransmission(address);
        rerror = Wire.endTransmission();
        delay(1);
        if (rerror == 0) {                           // device found
          sprintf(c, " %02x", address);
          Log.notice("%s", c);
          addr[3*q] = hex_table[address >> 4];
          addr[3*q + 1] = hex_table[address & 0x0f];
          addr[3*q + 2] = ' ';
          q++;
      } else if (rerror == 4) {    // other error      
        Log.notice(" XX");
      } else {                   // error = 2: received NACK on transmit of address              
        Log.notice(" --");    // error = 3: received NACK on transmit of data
      }
    } else {                 // address not scanned      
      Log.notice("   ");
    }
    if(q > 10) break;
  }
  addr[3*q] = '\0';
  Log.noticeln("" CR);
  snprintf (msg, MSG_BUFFER_SIZE, "Scan addr at:0x%s", addr);
  pub("i2c/info", msg); 
}

int8_t eepromwritebytes(int address, uint16_t offset, uint8_t wcount, uint8_t *databytes)
{
  uint8_t i = wcount;
  Wire.beginTransmission(address);
  if(eepromsize) Wire.write((int)(offset >> 8));
  Wire.write((int)(offset & 0xFF));
  do
      {
        i--;
      }
    while (Wire.write(databytes[wcount - 1 - i]) == 1 && i > 0);
    if (Wire.endTransmission(true)) // endTransmission returns zero on success
      {  
        Wire.endTransmission();
        return(1);
      }
    return(0);
}

int8_t eepromreadbytes(int address, uint16_t offset, int count, uint8_t * dest)
{
  int8_t ret = 0;
  Wire.beginTransmission(address);

  if(eepromsize) Wire.write((int)(offset >> 8)); 
  Wire.write((int)(offset & 0xFF));

  if(Wire.endTransmission(I2C_NOSTOP)){
     Wire.endTransmission();
     return (1);
  }
  uint8_t i = 0;
  ret = Wire.requestFrom(address, count);
  while (Wire.available()) {
    dest[i++] = Wire.read();
  }
  if(ret == 0)
  return (1);   // Unsuccessful
  else
  return(0);    // Successful
}

int8_t i2c_WriteRead(uint8_t address, uint8_t clength, uint8_t *commands, uint8_t length, uint8_t *values)
 {
  uint8_t i = clength; 
  bool Protocol;
  if (length == 0) {
      Protocol = true;
    Wire.beginTransmission(address);
    do
      {
        i--;
      }
    while (Wire.write(commands[clength - 1 - i]) == 1 && i > 0);
    if (Wire.endTransmission(Protocol)) // endTransmission(false) is a repeated start; endTransmission returns zero on success
      {  
        Wire.endTransmission();
        return(1);
      }
    return(0);
  }

  else {  
    Protocol = false;
    Wire.beginTransmission(address);
    do
      {
        i--;
      }
    while (Wire.write(commands[clength - 1 - i]) == 1 && i > 0);
      if (Wire.endTransmission(Protocol))
        {        
          Wire.endTransmission();
          return(1);
        }    
      uint8_t readBack = 0;
      Protocol = true;
      readBack = Wire.requestFrom((uint8_t)address, (uint8_t)length, (uint8_t)true);
      if(readBack == length)
        {
          while (Wire.available())
        {
          values[i] = Wire.read();
          if (i == (length-1)) break;        
          i++;
        }
        return (0);
      }
    else
      {
        return (1);    
      }
   }
 }

void eeprom_transfer_sent(uint8_t com){      
      uint16_t offset;
      uint8_t eeprom_address_;
      uint8_t actual_size;
      uint8_t eeprom_byte;
      uint8_t eeprom_bytes[32];
      uint8_t read_bytes[256];
      uint8_t write_bytes[128];
      int eeprom_counts;
      char d[1024];
  switch (com)
    {
      case 0:
        Log.noticeln(F("EEP Write 1 Byte:"));
        eeprom_address_ = i2cbus_d[1];
        offset = i2cbus_d[2] << 8 | i2cbus_d[3];
        if(i2cbus_d[4] == 1) eepromsize = false;
        else eepromsize = true;
        eeprom_byte = i2cbus_d[5];   
        if(!eepromwritebytes(int(eeprom_address_),  offset, 1, &eeprom_byte)) {
          snprintf (msg, MSG_BUFFER_SIZE, "write %02X to offset: %04X; Done", eeprom_byte, offset);
        }
       else  snprintf (msg, MSG_BUFFER_SIZE, "write %02X to offset: %04X; Fail", eeprom_byte, offset);
       pub("eeprom/info", msg);      
       Log.noticeln("%s", msg);               
       delay(1);       
      break;
           
      case 1:
        Log.noticeln(F("EEP Write 8 Byte:"));
        eeprom_address_ = i2cbus_d[1];
        offset = i2cbus_d[2] << 8 | i2cbus_d[3];
        if(i2cbus_d[4] == 1) eepromsize = false;
        else eepromsize = true;
        for(int i=0; i<8; i++) {
          eeprom_bytes[i] = i2cbus_d[5+i];
        }
        if(!eepromwritebytes(int(eeprom_address_),  offset, 8, eeprom_bytes)) {
            snprintf (msg, MSG_BUFFER_SIZE, "write 8Bytes to offset: %04X; Done", offset);
        }
        else snprintf (msg, MSG_BUFFER_SIZE, "write 8Bytes to offset: %04X; Fail", eeprom_byte, offset);
        pub("eeprom/info", msg);      
        Log.noticeln("%s", msg);               
        delay(1);  
      break;
        
      case 2:
        Log.noticeln(F("EEP Write 16 Byte:"));
        eeprom_address_ = i2cbus_d[1];
        offset = i2cbus_d[2] << 8 | i2cbus_d[3];
        if(i2cbus_d[4] == 1) eepromsize = false;
        else eepromsize = true;
        for(int i=0; i<16; i++) {
          eeprom_bytes[i] = i2cbus_d[5+i];
        }
        if(!eepromwritebytes(int(eeprom_address_),  offset, 16, eeprom_bytes)) {
            snprintf (msg, MSG_BUFFER_SIZE, "write 8Bytes to offset: %04X; Done", offset);
        }
        else  snprintf (msg, MSG_BUFFER_SIZE, "write 8Bytes to offset: %04X; Fail", eeprom_byte, offset);
        pub("eeprom/info", msg);      
        Log.noticeln("%s", msg);               
        delay(1);
      break;
            
      case 3:
        Log.noticeln(F("EEP Write 32 Byte:"));
        eeprom_address_ = i2cbus_d[1];
        offset = i2cbus_d[2] << 8 | i2cbus_d[3];
        if(i2cbus_d[4] == 1) eepromsize = false;
        else eepromsize = true;
        for(int i=0; i<32; i++) {
          eeprom_bytes[i] = i2cbus_d[5+i];
        }
        if(!eepromwritebytes(int(eeprom_address_),  offset, 32, eeprom_bytes)){
            snprintf (msg, MSG_BUFFER_SIZE, "write 8Bytes to offset: %04X; Done", offset);
        }
        else  snprintf (msg, MSG_BUFFER_SIZE, "write 8Bytes to offset: %04X; Fail", eeprom_byte, offset);
        pub("eeprom/info", msg);      
        Log.noticeln("%s", msg);              
        delay(1);
        break;
                          
      case 4:
        Log.noticeln(F("EEP Read Byte:"));
        eeprom_address_ = i2cbus_d[1];
        offset = i2cbus_d[2] << 8 | i2cbus_d[3];
        if(eepromreadbytes(int(eeprom_address_), offset, 1, &eeprom_byte)){

          snprintf (msg, MSG_BUFFER_SIZE, "offset0x%04X:[%s]",offset, eeprom_byte);
        }
        else  snprintf (msg, MSG_BUFFER_SIZE, "EEPROM Read Bytes: Fail.");
        pub("eeprom/read", msg);      
        Log.noticeln("%s", msg);   
        break;
      
       case 5:
        Log.noticeln(F("EEP Read Bytes:"));
        eeprom_address_ = i2cbus_d[1];
        offset = i2cbus_d[2] << 8 | i2cbus_d[3];
        eeprom_counts = i2cbus_d[4];
        if(eeprom_counts > 256) {
         Log.errorln(F("Reads Counts Fail(too big)."));
         pub("eeprom/read", "Reads Counts Fail(too big).");
         break;
        }
        if(i2cbus_d[5] == 1) eepromsize = false;
        else eepromsize = true;
        if(!eepromreadbytes(int(eeprom_address_),  offset, eeprom_counts, eeprom_bytes)) {
          for (int n = 0; n < eeprom_counts; n++){
          d[3*n] = hex_table[eeprom_bytes[n] >> 4];
          d[3*n + 1] = hex_table[eeprom_bytes[n] & 0x0f];
          d[3*n + 2] = ' ';
        }
        d[3*eeprom_counts - 1] = '\0';
        snprintf (msg, MSG_BUFFER_SIZE, "offset0x%04X:[%s]",offset, d);
        }
        else snprintf (msg, MSG_BUFFER_SIZE, "EEPROM Read Bytes: Fail.");
        pub("eeprom/read", msg);      
        Log.noticeln("%s", msg);
        break;
                
       case 6:
        Log.noticeln(F("Generic I2C W/R Bytes"));
        eeprom_address_ = i2cbus_d[1];
        eeprom_byte = i2cbus_d[2];
        if(eeprom_byte > 127) {
         Log.errorln(F("Write Blocks: Fail size too big."));
         pub("i2c/info", "Write Blocks: Fail size too big.");
         break;
        }
        for(int i = 0; i < eeprom_byte; i++) {
          write_bytes[i] = i2cbus_d[i+3];         
          d[3*i] = hex_table[write_bytes[i] >> 4];
          d[3*i + 1] = hex_table[write_bytes[i] & 0x0f];
          d[3*i + 2] = ' ';     
        }
        d[3*eeprom_byte - 1] = '\0'; 
        actual_size = i2cbus_d[eeprom_byte+3];
        if(actual_size == 0)
        snprintf (msg, MSG_BUFFER_SIZE, "Addr:0x%02x WriteQut:0x%02X [%s No Readback]", eeprom_address_, eeprom_byte, d, actual_size);
        else snprintf (msg, MSG_BUFFER_SIZE, "Addr:0x%02x WriteQut:0x%02X [%s Read:0x%02X]", eeprom_address_, eeprom_byte, d, actual_size);
        pub("i2c/info", msg);
        Log.noticeln("%s", msg);
        if(actual_size > 255) {
        Log.errorln(F("Read Blocks: Fail size too big."));
        pub("i2c/info", "Read Blocks: Fail size too big.");
        break;
        }
        if(i2c_WriteRead(eeprom_address_, eeprom_byte, write_bytes, actual_size, read_bytes)){
        Log.errorln(F("I2C Write/Read Block Fail."));
        pub("i2c/info", "Write/Read Block Fail.");
        break;
        }
        if(actual_size != 0)
        {
          for (int n = 0; n < actual_size; n++){           
            d[3*n] = hex_table[read_bytes[n] >> 4];
            d[3*n + 1] = hex_table[read_bytes[n] & 0x0f];
            d[3*n + 2] = ' ';
          }
          d[3*actual_size - 1] = '\0';       
          snprintf (msg, MSG_BUFFER_SIZE, "ReadQut:0x%02X [%s]", actual_size, d);
          pub("i2c/read", msg);
          Log.noticeln("%s", msg);
        }
        delay(1);
        break;

      default:
        snprintf (msg, MSG_BUFFER_SIZE, "Invalid Command");
        pub("status", msg);
        Log.noticeln("%s", msg);
        break;
    }
}

uint8_t read_data()
{
  uint8_t index = 0; //index to hold current location in ui_buffer
  int c; // single character used to store incoming keystrokes
  while (index < UI_BUFFER_SIZE-1)
  {
    ESP.wdtFeed();      // wdtFeed or delay can solve the ESP8266 SW wdt reset while is waiting for serial data
//    delay(1);
    c = Serial.read(); //read one character
    if (((char) c == '\r') || ((char) c == '\n')) break; // if carriage return or linefeed, stop and return data
    if ( ((char) c == '\x7F') || ((char) c == '\x08') )   // remove previous character (decrement index) if Backspace/Delete key pressed      index--;
    {
      if (index > 0) index--;
    }
    else if (c >= 0)
    {
      ui_buffer[index++]=(char) c; // put character into ui_buffer
    }
  }
  ui_buffer[index]='\0';  // terminate string with NULL

  if ((char) c == '\r')    // if the last character was a carriage return, also clear linefeed if it is next character
  {
    delay(10);  // allow 10ms for linefeed to appear on serial pins
    if (Serial.peek() == '\n') Serial.read(); // if linefeed appears, read it and throw it away
  }

  return index; // return number of characters, not including null terminator
}

int32_t read_int()
{
  int32_t data;
  read_data();
  if ((ui_buffer[0] == 'B') || (ui_buffer[0] == 'b'))
  {
    data = strtol(ui_buffer+1, NULL, 2);
  }
  else
    data = strtol(ui_buffer, NULL, 0);
  return(data);
}

int8_t read_char()
{
  read_data();
//  delay(1);
  return(ui_buffer[0]);
}

char *read_string()
{
  read_data();
  return(ui_buffer);
}