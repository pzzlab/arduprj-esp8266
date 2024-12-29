//#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <I2cDevices.h>

// Program to test I2C eeprom.
// Typing a series of strings execute soome commands to test:
// r aa nn                -> read from address aa(hex) for nn bytes
// w8[16,32] aa xxxx      -> write 8,16 32 bit value (hex) on address aa(hex)
// fmt                    -> erase the entire eeprom
// pf nn                  -> page erase nn (dec)



T24LCxxx        Eep;
Ticker          Tick;                         // Ticker handler

byte rq;


void Cyclic(void)
{
 unsigned t1 = millis();
 word n= 128,rd,a = 0;
 byte ebuf[128];

 switch (rq)
  {
    case 1:
           if (!Eep.PgErase(0)) Serial.println("Pgfmt #0 Error");
           Serial.printf("\nTime: %u ms\n >>\n",millis() - t1);
           break;

    case 10:
           rd = Eep.Rd(a,n, ebuf);
           Serial.printf("%04hX ",a); 
           for (n = 0; n < rd; n++) 
              { 
                if (n && !(n & 0xf)) Serial.printf("\n%04hX ",a + n);    // every 16, new line
                Serial.printf("%02hhX ",ebuf[n]);         // show byte
              }             
           Serial.printf("\nTime: %u ms\n >>\n",millis() - t1);
           break;

  }
 rq = 0;
}



void setup(void) 
{
  //  pinMode(LED_BUILTIN, OUTPUT);   pinMode(0, INPUT_PULLUP); 
  // Init the serial port and set the rx timeout 
  Serial.begin(115200);   Serial.setTimeout(5);
  // wait a little then flush remaining boot spurious response
  delay(500); Serial.flush();
    // Start AHT21 temp. and Humidity sensor
  if (!Eep.Begin(512,true)) Serial.println("\nEEprom Init Failed");

  Serial.println("\n---- EEprom I2C test program ----");
  Serial.println("r aa nn                -> read from address aa(hex) for nn bytes");
  Serial.println("w8[16,32] aa xxxx      -> write 8,16 32 bit value (hex) on address aa(hex)");
  Serial.println("fmt                    -> erase the entire eeprom");
  Serial.println("pf nn                  -> page erase nn (dec)");
  Tick.attach_ms(250,Cyclic);

}

// ########################################################################
void loop(void) 
{
 static char buf[24];
 word        a,rd,_w;
 dword       n,_d;
 unsigned    t1;
 byte        ebuf[128],_b;

 delay(100);

 if ((Serial.available() >= 1) && Serial.readBytes(buf, sizeof(buf)))
   {
    t1 = millis();
    n = 0;
    if (sscanf(buf,"r %hx %d",&a,&n) == 2)
        {
         if (n > sizeof(ebuf)) {n = sizeof(ebuf); Serial.printf("N Adjusted to %u\n", sizeof(ebuf));}
         rq = 10;
/*         rd = Eep.Rd(a,n, ebuf);
         Serial.printf("%04hX ",a); 
         for (n = 0; n < rd; n++) 
              { 
                if (n && !(n & 0xf)) Serial.printf("\n%04hX ",a + n);    // every 16, new line
                Serial.printf("%02hhX ",ebuf[n]);         // show byte
              }  
  */
        }

    if (sscanf(buf,"w8 %hx %x",&a,&n) == 2)   
        {_b = n; if (!Eep.Wr(a,_b,true)) Serial.println("Vfy Error");}
    if (sscanf(buf,"pf %d",&n) == 1) rq = 1;


/*    if (sscanf(buf,"w8 %hx %x",&a,&n) == 2)   
        {_b = n; if (!Eep.Wr(a,_b,true)) Serial.println("Vfy Error");}
    if (sscanf(buf,"w16 %hx %x",&a,&n) == 2)   
        {_w = n; if (!Eep.Wr(a,_w,true)) Serial.println("Vfy Error");}
    if (sscanf(buf,"w32 %hx %x",&a,&n) == 2)   
        {_d = n; if (!Eep.Wr(a,_d,true)) Serial.println("Vfy Error");}
    if (!strncmp(buf,"fmt",3))   
        if (!Eep.Erase()) Serial.println("Fmt Error");
    if (sscanf(buf,"pf %d",&n) == 1)   
        if (!Eep.PgErase(n)) Serial.println("Pgfmt Error");
    Serial.printf("\nTime: %u ms\n >>\n",millis() - t1);
    */
   }
  
  

}
