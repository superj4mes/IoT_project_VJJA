/*
 * Copyright (c) 2006-2020 Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 ***********************************
 * NTP time client example
 * A microcontroller board and MOD WIFI ESP8266
 * https://os.mbed.com/docs/mbed-os/v6.15/apis/wi-fi.html
 * https://os.mbed.com/teams/ESP8266/code/esp8266-driver/
 * https://www.olimex.com/Products/IoT/ESP8266/MOD-WIFI-ESP8266/open-source-hardware
 * https://os.mbed.com/teams/ESP8266/code/esp8266-driver/docs/tip/classESP8266Interface.html
 *
 * L432KC --- MOD WIFI ESP8266 from OLIMEX
 * L432KC D5=PB6=UART1TX --- 3 RXD
 * L432KC D4=PB7=UART1RX --- 4 TXD
 * or
 * L432KC D1=PA9=UART1TX --- 3 RXD
 * L432KC D0=PA10=UART1RX --- 4 TXD
 * L432KC 3V3 --- 1 3.3V
 * L432KC GND --- 2 GND
 *  
 * Timo Karppinen 12.12.2021  Apache-2.0
 ***********************************/
#include "mbed.h"

// MOD WIFI ESP8266
#include "ESP8266Interface.h"   // included in the OS6

#include "ntp-client/NTPClient.h"
#include "MQTTClientMbedOs.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <time.h>
#include <string>

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1331.h"

// PmodOLEDrgb
Adafruit_SSD1331 OLED(D9, D6, D10, D11, NC, D13); // cs, res, dc, mosi, (nc), sck  

// Definition of colors on the OLED display
#define Black 0x0000
#define White 0xFFFF

DigitalOut ledGreen(D2);   
DigitalOut ledYellow(D3);  
DigitalOut ledRed1(D4);

ESP8266Interface esp(MBED_CONF_APP_ESP_TX_PIN, MBED_CONF_APP_ESP_RX_PIN);


                    
//PmodALS
SPI spi(D11, D12, D13); // mosi, miso, sck
DigitalOut alsCS(A6);        // chip select for sensor SPI communication


int getALS()
{
    char alsByte0 = 0; //8bit data from sensor board, char is the unsigned 8bit
    char alsByte1 = 0; // 8bit data from sensor board
    char alsByteSh0 = 0;
    char alsByteSh1 = 0;
    char als8bit = 0;
    unsigned short alsRaw = 0;   // unsigned 16 bit
    float alsScaledF = 0;       // 32 bit floating point
    
    // Begin the conversion process and serial data output
    alsCS.write(0); 
    // Reading two 8bit bytes by writing two dymmy 8bit bytes
    ThisThread::sleep_for(1ms);
    alsByte0 = spi.write(0x00);
    alsByte1 = spi.write(0x00);
    // End of serial data output and back to tracking mode
    alsCS.write(1);
    ThisThread::sleep_for(1ms);
    // Check the http://www.ti.com/lit/ds/symlink/adc081s021.pdf
    // shifting bits to get the number out
    alsByteSh0 = alsByte0 << 4;
    alsByteSh1 = alsByte1 >> 4;
    
    als8bit =( alsByteSh0 | alsByteSh1 );
        
    alsScaledF = (float(als8bit))*(float(6.68)); 
    // The value 6.68 is 64 bit double precision floating point of type double.
    // Conversions to 32 bit floating point of type float.
        
    return (int)alsScaledF; 
}

// WLAN security
const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

void scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\r\n");

    int count = wifi->scan(NULL, 0);

    /* Limit number of network arbitrary to 15 */
    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];

    count = wifi->scan(ap, count);
    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\r\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\r\n", count);

    delete[] ap;
}

     
int alsScaledI = 0;    

void AmbientLightSensor(){

    spi.format(8,0);           
    spi.frequency(2000000);
        // ready to wait the conversion start
    alsCS.write(1);
    ThisThread::sleep_for(1ms);

    if (alsScaledI == getALS()){
        ThisThread::sleep_for(10ms);
    }
    else {
        alsScaledI = getALS(); 
        printf("Ambient light scaled to LUX =  '%0d' \r\n",alsScaledI);
            
        if (alsScaledI > 300){ 
            ledRed1.write(1);
            ledYellow.write(0);
            ledGreen.write(0);
            printf("Liian kirkasta, silmiin sattuu!\n\n");
            }
        else if (alsScaledI > 20){
            ledGreen.write(1);
            ledRed1.write(0);
            ledYellow.write(0);
            printf("Sopiva valo työskentelyyn\n\n");
        }
        else{
            ledYellow.write(1);
            ledGreen.write(0);
            ledRed1.write(0);
            printf("Liian hämärää työskentelyyn\n\n");
            }
    }
}

#define ntpAddress "time.mikes.fi"  // The VTT Mikes in Helsinki
#define ntpPort 123     // Typically 123 for every NTP server

 
int main()
{ 

//Store device and broker IP 
    SocketAddress deviceIP;
    SocketAddress MQTTBroker;
    
    TCPSocket socket;
    MQTTClient client(&socket);
    char buffer[128];
    //kovakoodattu value 
    int payloadValue = 2;

    ThisThread::sleep_for(500ms); // waiting for the ESP8266 to wake up.
    
    printf("\r\nConnecting...\r\n");
    int ret = esp.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\r\nConnection error\r\n");
        //return -1;
    }

    printf("Success\n\n");
    esp.get_ip_address(&deviceIP);
    printf("IP: %s\n", deviceIP.get_ip_address());


    esp.gethostbyname(MBED_CONF_APP_MQTT_BROKER_HOSTNAME, &MQTTBroker, NSAPI_IPv4, "esp");
    MQTTBroker.set_port(MBED_CONF_APP_MQTT_BROKER_PORT);
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.MQTTVersion = 3;
    data.clientID.cstring = MBED_CONF_APP_MQTT_CLIENT_ID;
    data.username.cstring = MBED_CONF_APP_MQTT_AUTH_METHOD;
    data.password.cstring = MBED_CONF_APP_MQTT_AUTH_TOKEN;
 
    
    sprintf(buffer, "%d", payloadValue);
                    
    MQTT::Message msg;
    msg.qos = MQTT::QOS0;
    msg.retained = false;
    msg.dup = false;
    msg.payload = (void*)buffer;
    msg.payloadlen = strlen(buffer);
    
    ThisThread::sleep_for(2s);  
                                
    // Connecting mqtt broker
    if (client.isConnected() == false){
        printf("Connecting %s ...\n", MBED_CONF_APP_MQTT_BROKER_HOSTNAME);
        socket.open(&esp);
        socket.connect(MQTTBroker);
        client.connect(data);
    }
        
    //Publish                    
    printf("Publishing with payload: %d\n", payloadValue);
    client.publish(MBED_CONF_APP_MQTT_TOPIC, msg);


    NTPClient ntp(&esp);
    ntp.set_server(ntpAddress, ntpPort);



    printf("Opening socket...");
    socket.open(&esp);
    printf("Opened!");
    socket.connect(MQTTBroker);
    printf("Opening to MQTT...");
    client.connect(data); 
    printf("Opened!");

    printf("\nReporting the following warnings:\n");
    MBED_WARNING1(MBED_ERROR_TIME_OUT, "Timeout error", 1);
    printf("   MBED_ERROR_TIME_OUT       Status: 0x%x Value: 0x%x\n", MBED_ERROR_TIME_OUT, 1);

    //Publish                    
    printf("Publishing with payload: %d\n", payloadValue);
    client.publish(MBED_CONF_APP_MQTT_TOPIC, msg);
    
 
   //Tässtä led näyttö kun ohjelma käynnistyy
    OLED.begin();
    OLED.clearScreen();
    OLED.fillScreen(Black);
    OLED.setTextColor(White);
    OLED.setCursor(30,25);
    OLED.setTextSize(1);
    OLED.printf("Booting up");
 
    while (true) {
        time_t timestamp = ntp.get_timestamp();
        timestamp += (60*60*3);  //  GMT +3  for Finland for the summer time.
        ThisThread::sleep_for(20000ms);
       
        
        if (timestamp < 0) {
            printf("An error occurred when getting the time. Code: %u\r\n", timestamp);
        } else {
            
            printf("Current time is %s\r\n", ctime(&timestamp));
            char *ptr = ctime(&timestamp);
            char *aika = (char*)malloc(sizeof(char)*6);
            memset(aika, '\0', 6*sizeof(char));
            memcpy(aika,ptr+11, 5*sizeof(char));
            
            OLED.clearScreen();  
            OLED.fillScreen(Black); 
            OLED.setTextColor(White); 
            OLED.setTextSize(3); 
            OLED.setCursor(4,20); 
            OLED.printf("%s", aika);
            free(aika);
        }

        AmbientLightSensor();


        
        
    }
}