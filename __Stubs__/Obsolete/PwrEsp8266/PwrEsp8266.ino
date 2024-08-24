/* ---------------------------------------------------- */
/*  Modifica esempio Udp per comunicare con Mcp39F511   */
/*  per raccogliere dati sul consumo di potenza         */  
/* ---------------------------------------------------- */


#include <ESP8266WiFi.h>
#include <ESP8266LLMNR.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#define UDPBUFSIZE  1024                                // Max is 8192
#define UARTBUFSIZE 128                                 // default

unsigned  localPort = 65500;                            // local port to listen/response UDP
char      UdpBuf    [UDPBUFSIZE + 1];                   //buffer for Udp packet
char      UartBuf   [UARTBUFSIZE + 1];                  //buffer for Serial packet
WiFiUDP   Udp;                                          // Udp instance

// ================================================================================ 
//                          ******** BOOTING ********
// ================================================================================ 
void setup() 
{
 bool led = true;                                       // local flag for led blinking
 char name[32],ssid[32],pwd[32];                        // SSID,password and host name
 
 pinMode(LED_BUILTIN, OUTPUT);                          // Initialize the LED_BUILTIN pin as an output
 pinMode(0, INPUT);                                     // The flash button
 
 // Initialization of serial and eeprom 
 Serial.begin       (115200);                           // Init Uart @ 115200,n,8,1
 EEPROM.begin       (512);                              // Allocate 512 Bytes of flash for eeprom emulator
 // Read the entire allocated buffer of credentials (zero filled at write)
 for (byte lp = 0; lp < sizeof(name); lp++) name[lp] = EEPROM.read(lp);
 for (byte lp = 0; lp < sizeof(ssid); lp++) ssid[lp] = EEPROM.read(lp + sizeof(name));
 for (byte lp = 0; lp < sizeof(pwd);  lp++) pwd [lp] = EEPROM.read(lp + sizeof(name) + sizeof(ssid));
 /*     
 Serial.println("");                                    // $$$$$$$$$$$$$$$$$$$
 Serial.printf("name [%s]\n",name);                     // $$$$$$$$$$$$$$$$$$$
 Serial.printf("ssid [%s]\n",ssid);                     // $$$$$$$$$$$$$$$$$$$
 Serial.printf("pwd  [%s]\n",pwd);                      // $$$$$$$$$$$$$$$$$$$
 */
 // boot delay
 delay(500);
 byte nretry = 0;                               
 // Something of plausible on parameters try to connect
 if (name[0] && ssid[0] && pwd[0])
    {
     WiFi.mode          (WIFI_STA);                     // Mode Station
     WiFi.setHostname   (name);                         // Assign module name
     WiFi.setSleepMode  (WIFI_LIGHT_SLEEP,1);           // Modem sleep
     WiFi.begin         (ssid, pwd);                    // Credentials
     nretry = 40;                                       // 20 secs to try connection
     // Try for N times else exit to config 
     while (--nretry && (WiFi.status() != WL_CONNECTED))
           {
            digitalWrite(LED_BUILTIN,led); led = !led; delay(500);
            // Press Button 
            if (!digitalRead(0)) {nretry = 0; break;}
           }
    }
 // If timeout occour, retry value will be zero.
 // Enter into config loop to wait a serial string for save HostName and credentials to save on EEPROM        
 
 while (!nretry)       
       {
        byte n,lp,lp1;
        if(( n = Serial.available()) > 16)              // at least 16 bytes 
          {
           // Zero fill credentials
           memset(name,0,sizeof(name)); memset(ssid,0,sizeof(ssid)); memset(pwd ,0,sizeof(pwd));
           Serial.readBytes(UartBuf, n); 
           // Remove All spaces or tabs from string
           for (lp = 0; lp < n; lp++)                   
               if ((UartBuf[lp] == ' ') || (UartBuf[lp] == '\t'))
                  {
                   for ( lp1 = lp; lp1 < n; lp1++) UartBuf[lp1] = UartBuf[lp1+1];
                   lp--;  n--;
                  }
           // scan for 1st comma, save in lowercase the hostname
           for (lp = 0; lp < n; lp++)
               {
                if (UartBuf[lp] == ',')  {lp++; break;}
                 else                     name[lp] = tolower(UartBuf[lp]);
               }
           // scan for 2nd comma, save ssid
           for ( lp1 = 0; lp < n; lp++, lp1++)
               {
                if (UartBuf[lp] == ',')  {lp++; break;}
                 else                     ssid[lp1] = UartBuf[lp];
               }
           // remaining is pwd
           for ( lp1 = 0; lp < n; lp++, lp1++)
               {
                if (UartBuf[lp] == 0)     break;
                 else                     pwd[lp1] = UartBuf[lp];
               }
           // save on EEPROM the string WITHOUT ANY CHECK
           for (lp = 0; lp < sizeof(name); lp++) EEPROM.write(lp, name[lp]);
           for (lp = 0; lp < sizeof(ssid); lp++) EEPROM.write(lp + sizeof(name), ssid[lp]);
           for (lp = 0; lp < sizeof(pwd);  lp++) EEPROM.write(lp + sizeof(name) + sizeof(name), pwd[lp]);
           digitalWrite(LED_BUILTIN,false);
           // save on flash from location zero, if ok, reset else stop with led on
           if (EEPROM.commit()) {delay(500); ESP.reset();}
            else                 while(true); 
          }
        // fast blink of led to show configuration mode
        led = !led; digitalWrite(LED_BUILTIN,!led);
        delay(50);
       }
 
 led = false; digitalWrite(LED_BUILTIN,!led);
 // Initiate Udp Object (assume LocalIp same of Wifi.LocalIP)
 Udp.begin(localPort);                                            // Start UDP service
 LLMNR.begin(name);                                               // Start LLMNR service (recognize ONLY lower chars!!)
} 

// ================================================================================ 
//                          ******** LOOPBACK ********
// ================================================================================ 

void loop() 
{
 short n;
 // if there's data available, read a packet
 if ((n = Udp.parsePacket()) > 0)
    {
     // read the packet into packetBufffer (put a terminator in any case)
     n = Udp.read(UdpBuf, UDPBUFSIZE);  UdpBuf[n] = 0;
     digitalWrite(LED_BUILTIN,false); delay(5); digitalWrite(LED_BUILTIN,true);
     // Parse the Udp Command received and write on uart the relative message to mcp32f511x

     memcpy(UartBuf,UdpBuf,n);  // >>>>>>>>>>>  will be parser result <<<<<<<<<<<<<     
     // send message via Uart
     Serial.write(UartBuf,n);
     // Flash led and wait time for response 
     digitalWrite(LED_BUILTIN,false); delay(1); digitalWrite(LED_BUILTIN,true); delay(15);
    }
 // There is something on rx buffer, echo n UDP
 if ((n = Serial.available()) > 0)
   {
    // Get data
    Serial.readBytes(UartBuf, n);
    // send back via UDP
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort()); Udp.write(UartBuf, n); Udp.endPacket();
    // Flash blink led    
    digitalWrite(LED_BUILTIN,false); delay(1); digitalWrite(LED_BUILTIN,true);
   }     
 delay(50);
}


