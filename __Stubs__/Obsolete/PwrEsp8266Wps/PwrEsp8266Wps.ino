
/* ---------------------------------------------------- */
/*  Modifica esempio Udp per comunicare con Mcp39F511   */
/*  per raccogliere dati sul consumo di potenza         */
/* .................................................... */
/* Dopo programmazione,le credenziali sono vuote        */
/* quindi il led lampeggera' per 10 secondi poi rimarra'*/
/* acceso in beginWPSConfig() a tempo indefinito e      */
/* spegnendosi per 1s ogni try of pairing (20s)         */
/* una volta effettuato il pairing, entrera'in blink    */
/* di 1s per 2 minuti, dopdiche' ri autoresetta.        */
/* a questo punto ritenta la connessione con i dati     */
/* ottenuti (blink veloce) e, se a buon fine, il led si */
/* accendera' ogni minuto per 50ms  per segnalare OK    */
/* NOTA: alla fine del pairing,il nome dell'host sara'  */
/* pwrmonxx (minuscolo) ed andra' "battezzato" via UDP  */
/* one shot  (salva in eeprom)                          */
/* ---------------------------------------------------- */


#include <ESP8266WiFi.h>
#include <ESP8266LLMNR.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#define UDPBUFSIZE 1024                                 // Max is 8192
#define UARTBUFSIZE 128                                 // Default uart size

WiFiUDP     Udp;                                        // Udp instance
char        UdpBuf  [UDPBUFSIZE + 1];                   //buffer for Udp packet
char        UartBuf [UARTBUFSIZE + 1];                  //buffer for Serial packet
word        localPort = 65500;                          // local port to listen/response UDP
byte        AutoReset;

// =============================================================
//                  ******** BOOTING ********
// =============================================================
void setup() 
{
bool  led = 0;                                          // local flag for led blinking
char name[32];    // HostName

// Initialize the led_BUILTIN pin as an output and flash button
 pinMode(LED_BUILTIN, OUTPUT);  
 pinMode(0, INPUT_PULLUP);            

 // Initialization of serial and eeprom
 Serial.begin(115200);                                  // Init Uart @ 115200,n,8,1
 EEPROM.begin(512);                                     // Allocate 512 Bytes of flash for eeprom emulator
 
 // Read the saved HostName from EEPROM
 for (byte lp = 0; lp < sizeof(name); lp++) name[lp] = EEPROM.read(lp);
// Initialize with last credentials saved
 WiFi.mode(WIFI_STA);
 WiFi.setHostname(name);
 WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());
 byte retry = 100;
 // durimg connection, blink the led fast
 while (--retry && WiFi.status() != WL_CONNECTED) 
       {led = !led; digitalWrite(LED_BUILTIN, !led); delay(100);}
  // if connected, switch off led then start UDP and LLMNR services
  if (retry) 
     {
      digitalWrite(LED_BUILTIN, true);     
      Udp.begin(localPort);                
      LLMNR.begin(WiFi.hostname().c_str());
     } 
   // Connection failed, sw on led then try to pair (need to push WPS button on router)
   // Assign and save the default name "pwrmonxx" 
   // During pairing led will be ON/OFF for 20/1Sec  into infinite loop until paired
   else
      {
       digitalWrite(LED_BUILTIN, false);
       memset(name,0,sizeof(name)); strcpy(name,"pwrmonxx");  WiFi.setHostname(name);
       for (byte lp = 0; lp < sizeof(name); lp++) EEPROM.write(lp, name[lp]); EEPROM.commit();
       while (!WiFi.beginWPSConfig() || !WiFi.SSID().length())    
             {digitalWrite(LED_BUILTIN, true); delay(1000); digitalWrite(LED_BUILTIN, false);}
      AutoReset = 120;                                 // paired, autoreset time = 2Min
      }
}                         // End Setup()

// =============================================================
//                  ******** LOOPBACK ********
// =============================================================

void loop() 
{
 bool  led   = 0;                                       // local flag for led blinking
 short n;
 static short blink;
 // Come from pairing phase,stay here into loop until WPS timeout (2 min) blinking led then reset
 // Or if connection done, short the time
 if (AutoReset)
    {
     while (--AutoReset) 
           {
            digitalWrite(LED_BUILTIN, led); delay(1000); led = !led;
            if ((AutoReset > 10) && (WiFi.status() == WL_CONNECTED)) AutoReset = 10;
           }
     delay(1000);  ESP.restart();
    }     
 // --------------------- RUNTIME LOOP --------------------------
 // if there's data available (Cmd + optional data)
 if ((n = Udp.parsePacket()) >= 4)
    {
     // read the packet into packetBufffer 
     n = Udp.read(UdpBuf, UDPBUFSIZE);  
     digitalWrite(LED_BUILTIN,false); delay(5); digitalWrite(LED_BUILTIN,true);
     // there is a direct string for MCP, check then forward on serial
     if (UdpBuf[0] == 0xa5)
        {     
         // check the validity of packet rx (calculate checksum) UdpBuf[UdpBuf[2]] = chksm idx
         byte chksm,lp;     
         for (chksm = lp = 0; lp < UdpBuf[2]; lp++) chksm += UdpBuf[lp];
         // Valid Packet, FORWARD on serial port
         if (chksm == UdpBuf[lp]) 
            {
             memcpy(UartBuf,UdpBuf,n); Serial.write(UartBuf,lp);
             // Flash led and wait time for response 
             digitalWrite(LED_BUILTIN,false); delay(1); digitalWrite(LED_BUILTIN,true); delay(15);
            }   
        }            
     else 
         {
          // If Udp contain 0x00110011 command is set New Station Name then send back an echo
          if (*(int*)UdpBuf == 0x00110011)
             {
              for (byte lp = 0; lp < 32; lp++) EEPROM.write(lp, UdpBuf[lp + 4]); 
              EEPROM.commit();
              Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());                                  
              Udp.write(UdpBuf, 4); Udp.endPacket();                              
              AutoReset = 5;
             }
         }
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
 if (blink++ == 1199)   digitalWrite(LED_BUILTIN,false);
  else
   if (blink >= 1200)    {digitalWrite(LED_BUILTIN,true); blink = 0;}
 delay(50);
}
