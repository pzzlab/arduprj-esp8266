// ********************************* Enviroment Settings **************************************
// Upload Speed:  512KBaud
// Flash Size:    (FS:512KB OTA 246KB)
// Flash Mode:    QIO
// MMU:           16KB Flash + 48KB IRAM + 2nd HEAP Shared
// 
// ------ NOTE: DO NOT USE delay() function on Webserver and Ticker because cause exception ----
//        Use: the millis(); 
//        E.g.  (unsigned t = millis(); while((millis() - t) < delay);)



// ------------------------------- Data Structure on Eeprom -------------------------------------
// structure of a tabel:
// +0x0000..0x0007    TClock    Date;         // 8 bytes
// +0x0008..0x000b    word      Temp,Hum;     // 4 bytes
// +0x000c..0x000f    word      Flags,_;      // 4 bytes
// +0x0010..0x0027    TMcp39F511Data  Mcp;    // 
// +0x0028..0x002f    word      Time[2];      // 4 bytes
// +0x0030..0x007f    word      _[16];        // align to 128 bytes
// +0x0080..0x01ff    short     Wh[96][2];    // buffer (384 bytes)
//
// ----------------- EEPROM MAPPING (128 bytes/page(4 pages for table)) -------------------------
//
//  0x0000..0x017f    Unused
//  0x0180..0x01ff    Misc data 
//  0x0200..0x3fff    Daily  Area:   TEeLog [31]     15872 bytes     interval 15 min
//  0x4000..0x49ff    Weekly Area:   TEeLog [5]      2560  bytes     interval 2 hours
//  0x4a00..0x61ff    Montly Area:   TEeLog [12]     6144  bytes     interval 8 hour 
//  0x6200..0x89ff    Yearly Area:   TEeLog [20]     10240 bytes     interval 4 days
//  0x8a00..0xffff    Unused
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

#define   SERIALCMD                           // handle serial commands

#define   NPWR          2                     // Power Measure Zones
#define   TIMEZONE      3600 * 1              // gmt + 1
#define   NMIN          15

// EEprom Mapping defs
#define   DAYADDR       0x0200                
#define   WKADDR        0x4000          
#define   MONADDR       0x4a00
#define   YRADDR        0x6200



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
                short           Log[96][2];   // buffer (384 bytes)
               } TRtData;                     // 128 bytes



TRtData         Data;                         // complete image (data + log)
byte            Pwd[13],Keyw[2],*Kw=Keyw;     // Password handling
byte            Verbose,Boot = 1;             // verbose flag and boot statemachine
byte            IdxD,IdxW,IdxM;               // acual index of ative tables (calc. at boot and updated at change)
word            TableIdx;                     // bit 0..7 [-1,0,+1] = prev,nxt; bit 8..10 [0..3] = day,week,month,year
WiFiUDP         NtpUDP;                       // For NTP time client
NTPClient       NTPClnt(NtpUDP,"europe.pool.ntp.org",TIMEZONE);// Intenet time 
TMcp39F511      Mcp(&Data.Mcp,240);           // Poll energy every min @4 tick/sec
TAht21          Aht21;                        // Temp and humidity
T24LCxxx        Eep;                          // Eeprom I2C where save data (64KB)
Ticker          Tick;                         // Ticker handler
TWebSrv         Web;                          // The WebServer with access to vars and html pages

// ========================================================================================
// table containing the public vars accessible to web
// Attribute:     Bits 0..2: 1=sint, 2=float,..,7=string;
// Len = size of data in bytes
// Note: for Array ,the field .Fmt is a decimal value of elements and .Len the sie of one element
PROGMEM const TVar Vars[] = 
{
// Name         Fmt       Obj           Type    Attr      Len    
  {"Data",      "",       &Data,        Struct, 0x0000,   sizeof(Data)},
  {"TableIdx",  "",       &TableIdx,    Var   , 0x0000,   1           }
};
const byte  VarsCnt = sizeof(Vars) / sizeof(TVar);        // define the number of elements

// ========================================================================================
//                      Eeprom support functions
// ========================================================================================
/*
void PutEeLogDate(word tabaddr,byte idx)
// --------------------------------------
// update the datetime of table item[idx]

// --------------------------------------
{
 TRtData *p = (TRtData*)(dword)tabaddr;        // make local pointer
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
 TRtData *p = (TRtData*)(dword)tabaddr;        // make local pointer
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
 byte *t = (byte*)&Data, sz = Eep.GetPgSz();
 for (word n = 0; n < sizeof(Data.Log); n++, t += sz, tabaddr += sz)
      Eep.Wr(tabaddr,sz,t);
}

void GetEeLog(word tabaddr,byte idx)
// --------------------------------------
// return the entire table item[idx] 
// on global struct Log
// --------------------------------------
{
 TRtData *p = (TRtData*)(dword)tabaddr;        // make local pointer
 byte *t = (byte*)&Data;
 byte sz = Eep.GetPgSz();
 for (word n = 0; n < sizeof(TEeTab); n+= sz, t += sz, tabaddr += sz)
      Eep.Rd(tabaddr,sz,t);
}
// ========================================================================================


*/

// ========================================================================================

void FillLog(word base)
{
  word n;
 for (n = 0; n < 96; n++) {Data.Log[n][0] = base + n;  Data.Log[n][1] = base - n -1;}
}


// ========================================================================================
//                                  Cyclic Processing
// ========================================================================================


void Cyclic(void)
// -----------------------------------------
// Cyclic callback (every 250ms)
// get data from device,build
// -----------------------------------------
{
  static int   lwh[2];
  static byte  ly,lm,lw,ld,lidx = -1;       // -1 force a update of Wh
  int          n[2],wh[2];
  // Get MCP data
//   Mcp.Poll();       // comment to have the terminal available

  // index of table (96 elements)
  byte idx = Data.Clk.Hour * 4 + Data.Clk.Min / 15;
  // at change..
  if (idx != lidx)          
    {
      // make a signed number for difference with energy in and out
      wh[0] = (int)(Data.Mcp.Wh.Pi[0] - Data.Mcp.Wh.Po[0])  ;
      wh[1] = (int)(Data.Mcp.Wh.Pi[1] - Data.Mcp.Wh.Po[1])  ;
      // at boot (lidx == -1) set values to actual mcp count
      if (lidx < 0) {lwh[0] = wh[0]; lwh[1] = wh[1];}
      // save on array and normalize to +/- 32767 (-1 will be -2... because -1is the cleared value)
      n[0] = wh[0] - lwh[0]; lwh[0] = wh[0]; if ((short)n[0] < 0) n[0]--; Data.Log[idx][0] = n[0];
      n[1] = wh[1] - lwh[1]; lwh[1] = wh[1]; if ((short)n[1] < 0) n[1]--; Data.Log[idx][1] = n[1];
      // Update the Dayly Eeprom of current with new data
      // EeUpdCurrDay(idx,n[0],n[1]);
      Serial.printf("[%hd] %hd  %hd   %5hd  %5hd\n",idx,n[0],n[1],wh[0],wh[1]);
    }
  // at boot save the actual date
  if (lidx < 0) {ly = Data.Clk.Year; lm = Data.Clk.Mon; ld = Data.Clk.Day; lw = Data.Clk.Yday / 7;}
  // @ every change of day,manage the tables
  if (Data.Clk.Day != ld)
    {
      // reduce the records and save on weekly image (also save the acual index)

      // reduce the records and save on montly image (also save the acual index)

      // reduce the records and save on yearly image (also save the acual index)


      


    }
  // for nxt test
  lidx = idx;
  // propagate auth. level
  if (Auth) Data.Flags |= Auth & 0x3;
   else     Data.Flags &= ~0x3;
  //  
  // propagate the command read table on buffer
  //.....
}

// ========================================================================================
//                                  WebServer CallBack 
// ========================================================================================


bool CmdCallBack(byte code, char *txt)
// ------------------------------------
// execute a webserver command callback
// ------------------------------------
{
 if (code == 90) return(Mcp.Parse(txt));
return(false);
}

// ----------------------------------------------------------------------------

// ========================================================================================
//                                    Misc support
// ========================================================================================


 bool UpdateClock(void)
 // -----------------------------
 // Update the clock time
 // return true at every second
 //  else return false
 // -----------------------------
 {
   time_t t; struct tm *tp;
   TClock *clk = &Data.Clk;  
   NTPClnt.update(); t = NTPClnt.getEpochTime(); tp = gmtime ((time_t *)&t); 
   if (clk->Sec != tp->tm_sec)
      {
       // update entire clock struct
       clk->Mon  = tp->tm_mon + 1; clk->Yday = tp->tm_yday;     
       clk->Day  = tp->tm_mday;    clk->Hour = tp->tm_hour;
       clk->Min  = tp->tm_min;     clk->Sec  = tp->tm_sec;
       clk->Year = tp->tm_year;                
       return(true);
      }
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


void loop(void) 
{
 
 byte ebuf[128];

 #ifdef SERIALCMD
  char buf[32],c;
  word a,b;
  
  memset(buf,0,sizeof(buf));
  
  if ((Serial.available() >= 1) && Serial.readBytes(buf, sizeof(buf)))
    {
      a = b = 0;
      if (sscanf(buf,"p %hd %hd",&a,&b))
       {
         if (a < 0) Data.Mcp.Wh.Po[0] += a; else  Data.Mcp.Wh.Pi[0] += a;
         if (b < 0) Data.Mcp.Wh.Po[1] += b; else  Data.Mcp.Wh.Pi[1] += b;
         Data.Clk.Min += 15;              
       }

    if (Data.Clk.Min  > 59) {Data.Clk.Min  = 0; Data.Clk.Hour++;}
    if (Data.Clk.Hour > 23) {Data.Clk.Min  = Data.Clk.Hour = 0; Data.Clk.Yday++;}
    if (Data.Clk.Yday > 365) {Data.Clk.Min  = Data.Clk.Hour = Data.Clk.Yday = 0; Data.Clk.Year++;}

    Serial.printf("%hd/%hhd/%hd %hhd:%02hhd(%hd,%hd)\n",Data.Clk.Yday,Data.Clk.Mon,Data.Clk.Year + 1900,Data.Clk.Hour,Data.Clk.Min,Data.Mcp.Wh.Pi[0],Data.Mcp.Wh.Po[0]);
    }
#endif

  switch (Boot)
        {
          case 1:   // booting, wait for a valid internet time (fast led blink 5/50ms)
                    if (!NTPClnt.forceUpdate()) {LED(true); delay(250);LED(false); break;}
                    // valid internet time, save for next compare                  
                    Boot++; 
                    break;
          case 2:   // Init I2C devices then start cyclic timer (GPIO[0,2] used fr I2C an not available)
                    // Init EEprom 512Kbit @400Khz and def.pins
                    if (!Eep.Begin(512,true))  Serial.println("EEprom Init Failed");
                    // Start AHT21 temp. and Humidity sensor
                    if (!Aht21.Begin(true))    Serial.println("AHT21 Init Failed, Temp. and Hum. unavailable");
                    Tick.attach_ms(250,Cyclic);
                    UpdateClock();
                    Boot++; 
                    break;
          case  3:  // dummy 
                    break;
          default:  // any other case
                    // Update global time (or continue from itself)
                    Aht21.Poll();             // MUST ce call cyclically (> 4) to read
                    // Every second
                    if (UpdateClock())
                      {
                        // update temp. and Hum data (~270us @400KHz)
                        if (Aht21.Read() == 1) {Data.Temp = Aht21.Temp; Data.Hum = Aht21.Hum;}
                      }
                    break;
        }             // end switch BootSeq()
    // EatTime loop
  delay(100);
}                     // EndLoop
