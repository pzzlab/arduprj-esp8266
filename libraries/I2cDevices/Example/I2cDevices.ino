#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <I2cDevices.h>

TAht21          Aht21;
T24LCxxx        Eep;
dword           EE[32];

void setup(void) 
{
  //  pinMode(LED_BUILTIN, OUTPUT);   pinMode(0, INPUT_PULLUP); 
  // Init the serial port and set the rx timeout 
  Serial.begin(115200);   Serial.setTimeout(5);
  // wait a little then flush remaining boot spurious response
  delay(500); Serial.flush();
    // Start AHT21 temp. and Humidity sensor
  if (!Aht21.Begin(true)) Serial.println("\nAHT21 Init Failed, Temp. and Hum. unavailable");
  if (!Eep.Begin(512,true)) Serial.println("\nEEprom Init Failed");
  // TODO add the ioexpander example
  
  // eeprom commands
  Serial.println("\n---- EEprom I2C test program ----");
  Serial.println("r aa nn                -> read from address aa(hex) for nn bytes");
  Serial.println("w8[16,32] aa xxxx      -> write 8,16 32 bit value (hex) on address aa(hex)");
  Serial.println("fmt                    -> erase the entire eeprom");
  Serial.println("pf nn                  -> page erase nn (dec)");
  
}

// ########################################################################
char n;
void loop(void) 
{
 static char buf[16],i;
 word        a,rd,_w;
 dword       n,_d;
 unsigned    t1;
 byte        n1,ebuf[128],_b;

 delay(100);

 if ((Serial.available() >= 1) && Serial.readBytes(buf, sizeof(buf)))
   {
    t1 = millis();
    if (sscanf(buf,"r %hx %hd",&a,&n) == 2)
        {
         if (n > sizeof(ebuf)) {n = sizeof(ebuf); Serial.printf("N Adjusted to %u\n", sizeof(ebuf));}
         rd = Eep.Rd(a,n, ebuf);
         Serial.printf("%04hX ",a); 
         for (n = 0; n < rd; n++) 
              { 
                if (n && !(n & 0xf)) Serial.printf("\n%04hX ",a + n);    // every 16, new line
                Serial.printf("%02hhX ",ebuf[n]);         // show byte
              }  
        }
    if (sscanf(buf,"w8 %hx %hhx",&a,&n) == 2)   
        {_b = n; if (!Eep.Wr(a,_b,true)) Serial.println("Vfy Error");}
    if (sscanf(buf,"w16 %hx %hx",&a,&n) == 2)   
        {_w = n; if (!Eep.Wr(a,_w,true)) Serial.println("Vfy Error");}
    if (sscanf(buf,"w32 %hx %x",&a,&n) == 2)   
        {_d = n; if (!Eep.Wr(a,_d,true)) Serial.println("Vfy Error");}
    if (!strncmp(buf,"fmt",3))   
        if (!Eep.Erase()) Serial.println("Fmt Error");
    if (sscanf(buf,"pf %hd",&n) == 1)   
        if (!Eep.PgErase(n)) Serial.println("Pgfmt Error");
    Serial.printf("\nTime: %u ms\n >>\n",millis() - t1);
   }
  Aht21.Poll();
  if (n1++ > 10)
  {
  if (Aht21.Read() == 1) Serial.printf("%hd^C  %hd%%\n",Aht21.Temp,Aht21.Hum);
  n1 =0;
  }
 }
