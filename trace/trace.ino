#include <SPI.h>
#include <RF24.h>
#include <RF24BLE.h>
#include <printf.h>

#include "irk.h

#define IRK_LIST_NUMBER 11
char * IrkListName[IRK_LIST_NUMBER] = {"xxxx"};

// 修正原代码中 irk 数组的格式定义
uint8_t irk[IRK_LIST_NUMBER][ESP_BT_OCTET16_LEN]= 
{
  //IRK of A
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
  //

};

#define MAC_LEN 6
#define RECV_PAYLOAD_SIZE 28


RF24 radio(4, 5);
RF24BLE BLE(radio);

/**********************************************************/

void BleDataCheckTask();
unsigned char input[32]={0};
//there are 3 channels at which BLE broadcasts occur
//hence channel can be 0,1,2 
byte channel = 0;
//using single channel to receive

void setup()
{
    Serial.begin(115200);
    Serial.println(F("RF24_BLE_address_NoCheck"));
    printf_begin();

    // 启动接收
    BLE.recvBegin(RECV_PAYLOAD_SIZE, channel);
}

void loop()
{
  BleDataCheckTask();
} 

void BleDataCheckTask()
{
  // 接收数据包
  byte status = BLE.recvPacket((uint8_t*)input, RECV_PAYLOAD_SIZE, channel);
  

  
  unsigned char AdMac[MAC_LEN];


  // 无论包类型如何，都尝试提取地址

  // Get the MAC address.
  // Reverse order in BT payload .
  for (byte i = 0; i < MAC_LEN; i++)
  {
  
    AdMac[MAC_LEN-1-i] = input[i+2];
  }

  // 打印当前提取出的地址
  printf("Check = %02X %02X %02X %02X %02X %02X\r\n"
    ,AdMac[0],AdMac[1],AdMac[2],AdMac[3],AdMac[4],AdMac[5]);

  for (byte i = 0; i < IRK_LIST_NUMBER; i++)
  {
    // Check with all IRK we got one by one.
    if(btm_ble_addr_resolvable(AdMac, irk[i]))
    {
      printf("MacAdd= %02X %02X %02X %02X %02X %02X Belongs to:%s\r\n"
        ,AdMac[0],AdMac[1],AdMac[2],AdMac[3],AdMac[4],AdMac[5]
      ,IrkListName[i]);
    }
  }
  return;
}
