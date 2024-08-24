// ********************************* Enviroment Settings **************************************
// Upload Speed:  512KBaud
// Flash Size:    (FS:512KB OTA 246KB)
// Flash Mode:    QIO
// MMU:           16KB Flash + 48KB IRAM + 2nd HEAP Shared
// ------ NOTE: DO NOT USE delay() function on Webserver and Ticker because cause exception ----
//        Use: the millis(); 
//        E.g.  (unsigned t = millis(); while((millis() - t) < delay);)


#include <AsWebSrv.h>
#include <Mcp39F511.h>
#include <I2cDevices.h>
#include <AsWebSrv.h>
#include <Ticker.h>

//#define   SERIALCMD   1                       // handle serial commands


TMcp39F511      Mcp(40);
TAht21          Aht21;
Ticker          Tick;                         // Ticker handler
TWebSrv         Web;                          // The WebServer with access to vars and html pages
//TData           Data;                         // everythig of data to read
byte            Pwd[13],Keyw[2],*Kw=Keyw;     // Password handling
byte            Verbose,DbgBuf[32];           // verbose flag and debug buffer

// ========================================================================================
// table containing the public vars accessible to web
// Attribute:     Bits 0..2 = char,short,int,float,..,string;
// Len = size of data in bytes
// Note: for Array ,the field .Fmt is a decimal value of elements and .Len the sie of one element
PROGMEM const TVar Vars[] = 
{
// Name         Fmt       Obj           Type    Attr    Len    
  {"Data",      "",       &Mcp.Data,    Struct, 0x80,   sizeof(Mcp.Data)},
  {"DbgBuf",    "",       &DbgBuf,      Struct, 0x80,   sizeof(DbgBuf)},
};
const byte  VarsCnt = sizeof(Vars) / sizeof(TVar);        // define the number of elements

// ========================================================================================
//                      Mcp39F11A(N) function
// ========================================================================================

// ========================================================================================

// ----------------------------------------------------------------------------

void Cyclic(void)
// -----------------------------------------
// Cyclic call avery 250ms 
// -----------------------------------------
{
  Mcp.Poll();
}




bool CmdCallBack(byte code, char *txt)
// ------------------------------------
// execute a webserver command callback
// ------------------------------------
{
 if (code == 1) return(Mcp.Parse(txt));
  return(false);
}

// ----------------------------------------------------------------------------


// ########################################################################
// ########################################################################
// ########################################################################


void setup(void) 
{
  // Initialize the LED_BUILTIN pin as an output and the program button
//  pinMode(LED_BUILTIN, OUTPUT);   pinMode(0, INPUT_PULLUP);
  // Init the serial port and set the rx timeout 
  Serial.begin(115200);   Serial.setTimeout(5);
  // wait a little then flush remaining boot spurious response
  delay(500); Serial.flush();
  // Activate WiFi then webserver (Setup access will be available after this)
  if (!Web.Begin(CmdCallBack))  {LED(true); Serial.println("\nError on WebServer Init");}
//  LED(true);
  // Start AHT21 temp. and Humidity sensor
  if (!Aht21.Begin(true)) Serial.println("\nAHT21 Init Failed");
  // start inerrupt timer every 250ms
  Tick.attach_ms(250,Cyclic);
   
  }

// ########################################################################
byte n;

void loop(void) 
{
  delay(100);
  Aht21.Poll();
  if (n++ > 10)
  {
  if (Aht21.Read() == 1) {Mcp.Data.Temp = Aht21.Temp; Mcp.Data.Hum = Aht21.Hum;}
  n =0;
  }
 
}
