import paho.mqtt.client as mqtt
from intelhex import IntelHex
import zlib
import time
import sys

INPUTNAME = r'./input/' + "eepdump.hex"
ih = IntelHex(INPUTNAME)
ihread = IntelHex()
MQTTBROKER = '192.168.1.88'
EEPROMADDR = 0x50
PAGE8B = 0x01
PAGE16B = 0x02
PAGE32B = 0x03
I2C_R_W = 0x06
READS = 0x05
READ8B = 0x08
READ16B = 16
READ32B = 32
READ64B = 64
READ128B = 128
OFFSET_D = 0x00
OFFSET_S = 0x01
HEXSTART = 0x00
M24C01 = 128
M24C02 = 256
M24C04 = 512
M24C08 = 1024
M24C16 = 2048
M24C32 = 4096
M24C64 = 8192
M24C128 = 16384
# client = mqtt.Client()
def crc(fileName):
     prev =0
     for eachLine in open(fileName, "rb"):
          prev = zlib.crc32(eachLine, prev)
     return "%08X" % (prev & 0xFFFFFFFF)

class MQTT2EEPROM:
    def __init__(self, mqtt_server):
        self.client = mqtt.Client()
        self.mqtt_server = mqtt_server
        self.port = 1883
        self.topic_prefix = "i2cAdapter/"
        self.addr = EEPROMADDR
        self.eep_pub = "i2cpub"
        self.username = 'homeiot'
        self.password = 'abcd1234'
        self.offset = 0 
        self.no_read = 0  
        self.crc32 = 0
        self.wcrc32 = 0
        self.eeprom_buff = []
        self.outputname = r'./output/' + "eeprom.hex" 
        self.mqtt_read = ''
        self.mqtt_topic = ''

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.sub('eeprom/#')
            self.sub('i2c/#')
            print(self.topic_prefix + '#')
            print("Connected success,and sub-eeprom-i2c/read ")
        else:
            print(f"Connected fail with code {rc}")

    def on_message(self, client, userdata, msg):
        self.mqtt_topic = msg.topic
        self.mqtt_read = msg.payload
        # print(f"{msg.topic}        {msg.payload}")
        if(msg.topic.find('/read') > 0):
            first = str(msg.payload).find('[')
            end   = str(msg.payload).find(']')
            sums = end -first
            first = first + 1
            for i in range(int(sums/3)):
                 self.add_buff(int(str(msg.payload)[first+3*i : first+3*i+2], 16))
            print(str(msg.payload)[first-1:end+1])

    def pub(self, topic, msg):
        self.client.publish(self.topic_prefix + topic, msg, qos=0, retain=False)
        print(msg)

    def pub_eep(self, msg):
        self.pub(self.eep_pub, msg)

    def add_buff(self, element):
         self.eeprom_buff.append(element)
         
    def sub(self, topic):
        self.client.subscribe(self.topic_prefix + topic, qos=0)
    
    def setTopicPrefix(self, topic_prefix): 
        self.topic_prefix = topic_prefix

    def setEepromAddress(self, addr):
        self.addr = addr

    def playload_read(self):
        return self.mqtt_read

    def topic_read(self):
        return self.mqtt_topic
    
    def get_eeprombuff(self):
        return self.eeprom_buff
    
    def get_outputname(self):
        return self.outputname

    def mqtt_init_start(self):
        # client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.username_pw_set(self.username, self.password)
        self.client.connect(self.mqtt_server, self.port, 60)
        self.client.loop_start()
        print('mqtt start')

    def mqttloop(self):
        #client.loop_forever()
        self.client.loop()

    def mqtt_disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()
        print('disconnect')
        sys.exit("Complete ")
    
    def eepromread2hex(self):
         i = 0
         for element in self.eeprom_buff:
              ihread[i] = element
              self.crc32 = zlib.crc32(bytes(element), self.crc32)
              i=i+1
         ihread.tofile(self.outputname, format='hex')
        #  return crc('self.outputname') 
    def eeprom_wr_str(self, mode, etype, offset, counts):
        lsb = (offset & 0xff)
        msb = (offset >> 8)
        list_str = '['
        list_str += '{:02X}'.format(mode)
        list_str += ' {:02X}'.format(self.addr)
        list_str += ' {:02X}'.format(msb)
        list_str += ' {:02X}'.format(lsb)
        if mode == 0x05:
             list_str += ' {:02X}'.format(counts)
             list_str += ' {:02X}'.format(etype)
        else:     
            list_str += ' {:02X}'.format(etype)
            for i, data in enumerate(ih.tobinarray(start=HEXSTART+offset, size=counts)):
                    list_str += ' {:02X}'.format(data)
                    self.wcrc32 = zlib.crc32(bytes(data), self.wcrc32)
        list_str += ']'
        return list_str
    
    def i2c_wr_str(self, addr, w_count, w_data, r_count):
        list_str = '['
        list_str += '{:02X}'.format(0x06)    # mode / I2C_R_W
        list_str += ' {:02X}'.format(addr)
        list_str += ' {:02X}'.format(w_count)
        for i in range(w_count):
             list_str += ' {:02X}'.format(w_data[i])
        list_str += ' {:02X}'.format(r_count)
        list_str += ']'
        return list_str
    
    def get_crc32(self):
         return "%08X" % (self.crc32 & 0xFFFFFFFF)
    
    def get_wcrc32(self):
         return "%08X" % (self.wcrc32 & 0xFFFFFFFF)
              
m2e = MQTT2EEPROM(MQTTBROKER)

def eeprom_page_write(mode, etype, offset, eepromsize):
        if mode == 0:
            blocks = 1
        elif mode == 1:
            blocks = 8
        elif mode == 2:
            blocks = 16
        elif mode == 3:
            blocks = 32
        for i in range(0, eepromsize, blocks):
            # print(eepromwrites_str(mode, etype, addr, offset+i, blocks))
            m2e.pub_eep(m2e.eeprom_wr_str(mode, etype, offset+i, blocks))
            time.sleep(0.01)
            
def readeeprom(blocks, etype, eepromsize): 
        for i in range(0, eepromsize, blocks):  
            m2e.pub_eep(m2e.eeprom_wr_str(READS, etype, i, blocks))
            time.sleep(0.25)
        m2e.eepromread2hex()

def i2c_write_read(addr, w_cont, w_data, r_data):
     m2e.pub_eep(m2e.i2c_wr_str(addr, w_cont, w_data, r_data))
     time.sleep(0.01)
     

def main():
     m2e.mqtt_init_start()
     time.sleep(1)
    #  for i in range(128):
    #     i2c_write_read(0x38, 0x01, [(i&0x03)], 0x02)  #  read 0x38 sensor DEVICE 0x00 - 0x02  
    #     time.sleep(2)     
     eeprom_page_write(PAGE32B, OFFSET_D, 0x00, M24C08)
     time.sleep(2)
     readeeprom(READ64B, OFFSET_D, M24C08)
     time.sleep(2)
    #  print("Reads_EEPROM_Bytes_CRC32: 0x%s" % m2e.get_crc32())
    #  print("Write_EEPROM_Bytes_CRC32: 0x%s" % m2e.get_wcrc32())
     if m2e.get_crc32() == m2e.get_wcrc32():
        print("EEPROM PROMGRAM VERIFY OK")
     else:
        print("EEPROM PROMGRAM VERIFY FAIL!") # promgram verify checksum.
     
    #  print("Input_EEPROM_Hexfile_CRC32: 0x%s" % crc(INPUTNAME)) # program Hex file checksum.
    #  print("Output_EEPROM_Hexfile_CRC32: 0x%s" % crc(m2e.get_outputname()))
     time.sleep(1)
     m2e.mqtt_disconnect()    
if __name__ == '__main__':
    main()
