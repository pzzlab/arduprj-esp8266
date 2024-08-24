// ********************************* Enviroment Settings **************************************
// Upload Speed:  512KBaud
// Flash Size:    (FS:512KB OTA 246KB)
// Flash Mode:    QIO
// MMU:           16KB Flash + 48KB IRAM + 2nd HEAP Shared
// ------ NOTE: DO NOT USE delay() function on Webserver and Ticker because cause exception ----
//        Use: the millis(); 
//        E.g.  (unsigned t = millis(); while((millis() - t) < delay);)



// ------------------------------- Data Structure on Eeprom -------------------------------------
// structure of a tabel:
// +0x0000..0x0007    TClock    Date;         // 8 bytes
// +0x0008..0x0027    TMinMax   Lim;          // 32 bytes
// +0x0028..0x002f    word      Time[2];      // 8 bytes
// +0x0030..0x007f    word      _[40];        // 80 bytes (align to 128 bytes)
// +0x0080..0x01ff    short     Wh[96][2];    // buffer (384 bytes)
//
// ----------------- EEPROM MAPPING (128 bytes/page(4 pages for table)) -------------------------
//
//  0x0000..0x007f     Unused
//  0x0080..0x008f    active index 
//  0x0000..0x3dff    Daily  Area:   TEeLog [31]     15872 bytes     interval 15 min
//  0x3e00..0x47ff    Weekly Area:   TEeLog [5]      2560  bytes     interval 2 hours
//  0x4800..0x5fff    Montly Area:   TEeLog [12]     6144  bytes     interval 8 hour 
//  0x6000..0x87ff    Yearly Area:   TEeLog [20]     10240 bytes     interval 4 days
//  0x8800..0xffff    Unused
//
//  EEprom Duration Estimate of 1.000.000 write cycles @ 15 min.rate (1 diff cell written at time)
//  A write of 1 or more cells will be divide in bytes/page due an internal buffering of data
//  Write @ Day   = 96(samples) * 2(channels) * 2(bytes) but if written in one time as DWORD
//                  consider as 1 write cycle per 2 bytes and 2 channels = 96
//  Effective page write @ day 96 * 4 / 128 = 3 (write endurance is refered to page write)
//  Worst case EEprom Endurance: 1000000 / 366 / 3    ---->>>> 910 Years! <<<<----
//  ................... O NO? .....................
//
//  Everry 15 min:
//    [Data.Clk.Day] [Data.Clk.Hour * 4 + Data.Clk.Min / 15] [0..1]
//  every change of day (Lday < day):
//        1) extract from active table (lday) the data, compact and save on weekly
//    [Data.Clk.Day] [Data.Clk.Hour / 2] [0..1]
//        2) extract from active table (lday) the data, compact and save on montly
//    [Data.Clk.Mon] [Data.Clk.Day * 3 + Data.Clk.Hour / 8] [0..1]
//        3) extract from active table (lday) the data, compact and save on yearly ( year will be save from starting 2024)
//    [Data.Clk.Year-124] [Data.Clk.Day / 4] [0..1]
//        4) save on Misc table the min/max in/out power and voltage from ram
//    [Data.Clk.Yday % 96] [0..3][0..1]
//
//    NOTE:
//    when selected actual day,the fields relative to limits are updated at runtime
//    and Time will be a global timer,on other logs has no reason to be
// ---------------------------------------------------------------------------------------------


#include <AsWebSrv.h>
#include <Mcp39F511.h>
#include <I2cDevices.h>
#include <Ticker.h>
#include <ESP8266LLMNR.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

//#define   SERIALCMD   1                       // handle serial commands
#define   NPWR          2                     // Power Measure Zones
#define   TIMEZONE      3600 * 1              // gmt + 1
#define   NMIN          15

// EEprom Mapping defs
#define   DAYADDR     0x0000                
#define   WKADDR      0x3e00          
#define   MONADDR     0x4800
#define   YRADDR      0x6000



typedef struct 
              {
                byte    Year,Mon,Day,Hour,Min,Sec;
                word    Yday;
              } TClock;                       // >>>> Date/time to here

typedef struct {
                byte            Last;         // last index of tables
                byte            Act;          // actual index in use
                byte            Size;         // number of tables
                byte           _;
               } TLogInfo;                  // 128bytes

typedef struct {
                TClock          Clk;          // 8 bytes
                word            Temp, Hum;    // Aht21 temperature and humudity
                word            Flags,_;      // general flags, filler
                TMcp39F511Data  Mcp;          // Mcp Data block
                word            Time[2];      // 4 bytes <inc every 15 min.(682 days)>
                byte            __[16];       // filler to reach 128 bytes
               } TEeData;                     // 128 bytes

typedef struct {
                TEeData         Data;         // 128 bytes  
                short           Log[96][2];   // buffer (384 bytes)
               } TEeTab;                      // 512 bytes



TEeData         Data;                         // Add runtime data
TEeTab          Tab;                          // complete image (data + log)
byte            Pwd[13],Keyw[2],*Kw=Keyw;     // Password handling
byte            Verbose;                      // verbose flag and debug buffer
byte            IdxD,IdxW,IdxM;               // acual index of ative tables (calc. at boot and updated at change)
WiFiUDP         NtpUDP;                       // For NTP time client
NTPClient       NTPClnt(NtpUDP,"europe.pool.ntp.org",TIMEZONE);// Intenet time 
TMcp39F511      Mcp(&Data.Mcp,240);           // Poll energy every min @4 tick/sec
TAht21          Aht21;                        // Temp and humidity
T24LCxxx        Eep;                          // Eeprom I2C where save data (64KB)
Ticker          Tick;                         // Ticker handler
TWebSrv         Web;                          // The WebServer with access to vars and html pages

// ========================================================================================
// table containing the public vars accessible to web
// Attribute:     Bits 0..2 = char,short,int,float,..,string;
// Len = size of data in bytes
// Note: for Array ,the field .Fmt is a decimal value of elements and .Len the sie of one element
PROGMEM const TVar Vars[] = 
{
// Name         Fmt       Obj           Type    Attr      Len    
  {"Data",      "",       &Data,        Struct, 0x0000,   sizeof(Data)},
  {"Tab",       "",       &Tab,         Struct, 0x0000,   sizeof(Tab)}
 };
const byte  VarsCnt = sizeof(Vars) / sizeof(TVar);        // define the number of elements

// ========================================================================================
//                      Eeprom support functions
// ========================================================================================

void PutEeLogDate(word tabaddr,byte idx)
// --------------------------------------
// update the datetime of table item[idx]

// --------------------------------------
{
 TEeTab *p = (TEeTab*)(dword)tabaddr;        // make local pointer
 p += idx;                            // make a pointer to eeprom location
 dword a = (dword)&p->Data.Clk;
 Eep.Wr(a,(byte)sizeof(TClock),&Data.Clk);
 //Serial.printf("%04X> %03hhd-%02hhd-%02hhd  %02hhd:%02hhd\n",a,Data.Clk.Year,Data.Clk.Mon,Data.Clk.Day,Data.Clk.Hour,Data.Clk.Min);
}

TClock GetEeLogDate(word tabaddr,byte idx)
// --------------------------------------
// return the datetime of table item[idx]
// --------------------------------------
{
 TEeTab *p = (TEeTab*)(dword)tabaddr;        // make local pointer
 TClock clk;
 p += idx;                            // make a pointer to eeprom location
 dword a = (dword)&p->Data.Clk;
 Eep.Rd(a,sizeof(TClock),(byte*)&clk);
 return(clk);
}
// ========================================================================================

void PutEeLog(word tabaddr,byte idx)
// --------------------------------------
// return the entire table item[idx] 
// on global struct Log
// --------------------------------------
{
 tabaddr += idx * sizeof(TEeTab);       // make a pointer to eeprom location
 byte *t = (byte*)&Tab, sz = Eep.GetPgSz();
 for (word n = 0; n < sizeof(TEeTab); n++, t += sz, tabaddr += sz)
      Eep.Wr(tabaddr,sz,t);
}
// ========================================================================================

void FillLog(word base)
{
 word lp,n;
 for (lp = 0; lp < 96; lp++) {Tab.Log[lp][0] = base + lp; Tab.Log[lp][1] = base - lp;}
}

void GetEeLog(word tabaddr,byte idx)
// --------------------------------------
// return the entire table item[idx] 
// on global struct Log
// --------------------------------------
{
 tabaddr += idx * sizeof(TEeTab);       // make a pointer to eeprom location
 byte *t = (byte*)&Tab;
 byte sz = Eep.GetPgSz();
 for (word n = 0; n < sizeof(TEeTab); n+= sz, t += sz, tabaddr += sz)
      Eep.Rd(tabaddr,sz,t);
}
// ========================================================================================


void ShowLog(void)
{
 word lp,sz= Eep.GetPgSz();
 byte *t = (byte*)&Tab;
 for (word n = 0; n < sizeof(TEeTab); n+= sz, t += sz)
      {
       Serial.printf("[%04hx] ",n);
       for(lp = 0; lp < sz; lp+=2)
          {
            if (lp &&!(lp &0x1f)) Serial.printf("\n[%04hx] ",n+lp);   
           Serial.printf("%04hx ",*(word*)&t[lp]);   
          } 
       if (!n && (lp == 0x80)) Serial.println("Buf:"); 
       Serial.println(""); 
      }

}




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
  
  if (Auth) Data.Flags |= Auth & 0x3;
   else     Data.Flags &= ~0x3;
  Mcp.Poll();       // comment to have the terminal available
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
  // Initialize the LED_BUILTIN pin as an output and the program button is done as default into Web.Begin()
  //  pinMode(LED_BUILTIN, OUTPUT);   pinMode(0, INPUT_PULLUP); 
  // Init the serial port and set the rx timeout 
  Serial.begin(115200);   Serial.setTimeout(5);
  // wait a little then flush remaining boot spurious response
  delay(500); Serial.flush();
  // Activate WiFi then webserver (Setup access will be available after this)
  if (!Web.Begin(CmdCallBack))  {LED(true); Serial.println("\nError on WebServer Init");}
  // Start NTP client for timezone and update every 5 min timezone
  NTPClnt.begin(); NTPClnt.setTimeOffset(TIMEZONE);
  
}   
// ########################################################################

byte bootstate = 1;

void loop(void) 
{
 time_t t; struct tm *tp;
 char buf[20],c;
 byte ebuf[128];
 word a,rd;
 dword n;
  // EatTime loop
 delay(100);
 memset(buf,0,sizeof(buf));
if ((Serial.available() >= 1) && Serial.readBytes(buf, sizeof(buf)))
   {
    a = n = 0;
    if (!strncmp(buf,"clr",3)) memset(&Data,0,sizeof(Data));
    else
    if (!strncmp(buf,"view",4)) ShowLog();
    else
    if (!strncmp(buf,"fill",4)) FillLog(0);
    else
    if (sscanf(buf,"wr %hx %d %c",&a,&n,&c) == 2)
       {
        PutEeLog(a,n);
        Serial.printf("Log[%04hx] %hhd Written\n",a,n);
       }
    else
    if (sscanf(buf,"rd %hx %d %c ",&a,&n,&c) == 2)
       {
        GetEeLog(a, n);
        Serial.printf("Log[%04hx] %hhd Read\n",a,n);
       }
    
       else Serial.println(buf); 
   }


  switch (bootstate)
        {
          case 1:   // booting, wait for a valid internet time (fast led blink 5/50ms)
                    if (!NTPClnt.forceUpdate()) {LED(true); delay(250);LED(false); break;}
                    // valid internet time, save for next compare                  
                    t = NTPClnt.getEpochTime(); tp = gmtime ((time_t *)&t);       
                    bootstate++; 
                    break;
          case 2:   // Init I2C devices then start cyclic timer (GPIO[0,2] used fr I2C an not available)
                    // Init EEprom 512Kbit @400Khz and def.pins
                    if (!Eep.Begin(512,true))  Serial.println("EEprom Init Failed");
                    // Start AHT21 temp. and Humidity sensor
                    if (!Aht21.Begin(true))    Serial.println("AHT21 Init Failed, Temp. and Hum. unavailable");
                    Tick.attach_ms(250,Cyclic);
                    bootstate++; 
                    break;
          case 11: ;break;
          default:  // any other case
                    // Update global time (or continue from itself)
                    NTPClnt.update(); t = NTPClnt.getEpochTime(); tp = gmtime ((time_t *)&t); 
                    Aht21.Poll();             // MUST ce call cyclically (> 4) to read
                    // Every second
                    TClock *c   = &Data.Clk;
                    if (c->Sec != tp->tm_sec)
                      {
                        // update entire clock struct
                        c->Mon  = tp->tm_mon + 1; c->Yday = tp->tm_yday;     
                        c->Day  = tp->tm_mday;    c->Hour = tp->tm_hour;
                        c->Min  = tp->tm_min;     c->Sec  = tp->tm_sec;
                        c->Year = tp->tm_year;                
                        // update temp. and Hum data (~270us @400KHz)
                        if (Aht21.Read() == 1) {Data.Temp = Aht21.Temp; Data.Hum = Aht21.Hum;}
/*                        Serial.printf("%hd -%hd  ",Data.Temp,Data.Hum);
                       Serial.printf("%hd-%hhd-%hhd %2hhd:%hhd.%hhd\n",Data.Clk.Year + 1900,
                                      Data.Clk.Mon,Data.Clk.Day,Data.Clk.Hour,Data.Clk.Min,Data.Clk.Sec);
  */                      
                      }

        }             // end switch BootSeq()

}
