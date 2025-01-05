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
// structure of a table:
// +0x0000..0x000     byte      Mon,Day;      // 2 bytes 
// +0x0002..0x0005    word      Vmin,Vmax;    // 4 bytes
// +0x0006..0x0009    word      WMax[2];      // 4 bytes
// +0x000a..0x0011    word      TimeMinMax[4];// 8 bytes
// +0x0012..0x007f    byte      _[110];       // filler to reach 128 bytes
// +0x0080..0x001ff   short     Log[96][2];   // 384 bytes
//
// ----------------- EEPROM MAPPING (128 bytes/page(4 pages for table)) -------------------------
//
//  0x0000..0x01ff    Unused
//  0x0200..0x3fff    Month[0]  TEeLog [31]    15872 bytes     interval 15 min    (96 recs)
//  0x4000..0x7dff    Month[1]  TEeLog [31]    15872 bytes     interval 15 min    (96 recs)
//  0x7e00..0xbbff    Month[2]  TEeLog [31]    15872 bytes     interval 15 min    (96 recs)
//  0xbc00..0xf9ff    Month[3]  TEeLog [31]    15872 bytes     interval 15 min    (96 recs)
//  0xfa00..0xffff    unused7
//
//  EEprom Duration Estimate of 1.000.000 write cycles @ 15 min.rate (1 diff cell written at time)
//  A write of 1 or more cells will be divide in bytes/page due an internal buffering of data
//  Write @ Day   = 96(samples) * 2(channels) * 2(bytes) but if written in one time as DWORD
//                  consider as 1 write cycle per 2 bytes and 2 channels = 96
//  Effective page write @ day 96 * 4 / 128 = 3 (write endurance is refered to page write)
//  Worst case EEprom Endurance: 1000000 / 366 / 3    ---->>>> 910 Years! <<<<----
//  ................... O NO? .....................
//    NOTE:
//    when selected actual day,the fields relative to limits are updated at runtime
//    and Time will be a global timer,on other logs has no reason to be
// ---------------------------------------------------------------------------------------------


#include <AsWebSrv.h>
#include <Mcp39F511.h>
#include <I2cDevices.h>
#include <Ticker.h>
#include <ESP8266LLMNR.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

//#define   SERIALCMD                           // handle serial commands
//#define   AUTODATE                            // autoenable date update manually (exclude clockupdate())

#define   NPWR          2                     // Power Measure Zones
#define   TIMEZONE      3600 * 1              // gmt + 1
#define   NMIN          15

// EEprom Mapping defs
#define   DAYSIZE       0x0200                // 512 bytes
#define   MONSIZE       DAYSIZE * 31          // 512 * 31
#define   DAYADDR       0x0200                // abs. starting address
#define   WHOFS         0x0080                // where start wh array

#define   EEWRCR        0x01                  // write creation date
#define   EEWRPW        0x02                  // write power (4 bytes)
#define   EEWRPK        0x04                  // write limits values (128 bytes)
#define   EEDYRD        0x10                  // Day Read (monthidx,dayidx) -> Table
#define   EEMCLR        0x80                  // Month clear (index)

typedef struct 
              {
                byte    Year,Mon,Day,Hour,Min,Sec;
                word    Yday;
              } TClock;                       // >>>> Date/time to here

typedef struct 
              {
                void   *Data;
                word    Addr;                 // address reference (or page if pgerase)
                byte    Cmd,Idx;              // command to execute (return 0 at done) , month
                byte    LDay,Monx,LMonx;
              } TeeCmd;                       // >>>> eeprom command struct to propate in 

typedef struct {
                TClock          Clk;          // 8 bytes
                word            Temp, Hum;    // Aht21 temperature and humudity
                word            Flags,__;      // general flags, filler
                TMcp39F511Data  Mcp;          // Mcp Data block
                word            RSSI;         // wifi signal
                word            Wmax[2];      // peack power
                word            Time[2];      // Time measure 
                word            Elaps[2];     // Time elapsed
                byte            _[2];         // filler to reach 128 bytes
               } TRtData;                     // 128 bytes

typedef struct {
                byte            Mon,Day;      // Creation date
                word            Vmin,Vmax;    // 
                word            Wmax[2];      // max power  
                word            TimeMinMax[4];// time when min/max occour
                byte            _[110];       // filler to reach 128 bytes
                short           Log[96][2];   // buffer (384 bytes)
               } TLog;                        // 128 bytes




TRtData         Data;                         // image for runtime data ( cyclic)
TLog            Log;                          // Log Image
byte            Pwd[13];                      // Password handling
byte            MIdx = 1,DIdx = 1,Boot = 1;   // boot statemachine
WiFiUDP         NtpUDP;                       // For NTP time client
NTPClient       NTPClnt(NtpUDP,"europe.pool.ntp.org",TIMEZONE);// Intenet time 
TMcp39F511      Mcp(&Data.Mcp,240);           // Poll energy every min @4 tick/sec
TAht21          Aht21;                        // Temp and humidity
T24LCxxx        Eep;                          // Eeprom I2C where save data (64KB)
TeeCmd          EeCmd;                        // loop handled eeprom handler
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
  {"Log",       "",       &Log,         Struct, 0x0000,   sizeof(Log)}
};

const byte  VarsCnt = sizeof(Vars) / sizeof(TVar);        // define the number of elements


// ========================================================================================



void EeHnd(TeeCmd *p)
// -----------------------------------------
// Handle the eeprom commands in background
// because can't done into Ticker why cause
// PANIC exception
// 
// EEMCLR (.Mon)          Erase entire month      ~1350ms
// EEWRCR (.Clk)          Write creation date     ~2ms
// EEWRPW (.Addr,.Data)   Write energy data       ~2ms
// EEWRPK (.Addr,.Data)   Write min-max values    ~4ms
// -----------------------------------------
{
 word addr;
 
if (p->Cmd & EEWRPK)
   // at last record, save the dayly min-max values
    {
     // Log.TimeMinMax is already saved on cyclic()
     Log.Vmin     = Data.Mcp.Vaw.Vmin;
     Log.Vmax     = Data.Mcp.Vaw.Vmax;
     Log.Wmax[0]  = Data.Mcp.Vaw.Wmax[0];
     Log.Wmax[1]  = Data.Mcp.Vaw.Wmax[1];
     addr = DAYADDR + MONSIZE * (word) p->LMonx + DAYSIZE * ((word)p->LDay -1);
     Eep.Wr(addr + 2 ,16 ,(byte*)&Log +2);                      // copy all 1st block
#ifdef TESTING     
     Serial.printf("LimitsWrite: (%4hx) %5hd %5hd %5hd %5hd\n", addr, Log.Vmin, Log.Vmax, Log.Wmax[0], Log.Wmax[1]); 
 #endif     
     p->Cmd &= ~EEWRPK;       
    }

 if (p->Cmd & EEMCLR)
    {
     // at 0.00 
     addr = DAYADDR + MONSIZE * (word) p->Monx;
#ifdef TESTING     
     Serial.printf("Erase [%hd] (%4hx ->",p->Monx, addr);
#endif
     // 0xff fill remaining pages
     for (byte lp =0; lp < 31; lp++)      // loop for 31 days
        {
         // Zero fill 1st page (+00.. +0x7f)
         Eep.PgFill(addr >> 7,0); addr += 128;
         Eep.PgErase(addr >> 7);  addr += 128;
         Eep.PgErase(addr >> 7);  addr += 128;
         Eep.PgErase(addr >> 7);  addr += 128;
        }
#ifdef TESTING     
      Serial.printf("%4hx\n",addr-1);
 #endif      
     p->Cmd &= ~EEMCLR;       
    }
     
if (p->Cmd & EEWRCR)
    {
      // write creation date (mon,day)
     addr = DAYADDR + MONSIZE * (word) p->Monx + DAYSIZE * ((word)p->LDay -1);
     Log.Day  = Data.Clk.Day;   Log.Mon = Data.Clk.Mon; 
     Log.Vmin = 500;            Log.Vmax = Log.Wmax[0] = Log.Wmax[1] = 0;   // reset limits (updated autmarically)
     Data.Mcp.Vaw.Vmin = 500;   Data.Mcp.Vaw.Vmax = Data.Mcp.Vaw.Wmax[0] = Data.Mcp.Vaw.Wmax[1] = 0;
     Data.Wmax[0] =             Data.Time[0] = Data.Elaps[0] = 0;
     Data.Wmax[1] =             Data.Time[1] = Data.Elaps[1] = 1;
     Eep.Wr(addr, 2 , (byte*)&Log);
#ifdef TESTING     
     Serial.printf("DyCreate: %hhd-%hhd (%4hx)\n",Log.Day,Log.Mon, addr); 
#endif     
     p->Cmd &= ~EEWRCR;       
    }

if (p->Cmd & EEWRPW)
    {
     // write ONE record of power (every 15 min)
     short *d = (short*)p->Data;
     Eep.Wr(p->Addr, 4, p->Data);
#ifdef TESTING     
     Serial.printf("[%2hhd.%2hhd][%2hhd] (%4hx) %5hd %5hd\n",p->Monx, p->LDay, p->Idx, p->Addr,d[0],d[1]);
 #endif
     p->Cmd &= ~EEWRPW;       
    }

 if (p->Cmd & EEDYRD)
    {
     // Read the entire log (overlat actual, will be restored at 1st 15 min)
     byte *d = (byte*)&Log;
     addr = p->Addr;
#ifdef TESTING     
     Serial.printf("Rd: [%4hx ->",addr);
#endif
     for (word lp = 0; lp < 4; lp++, addr += 128 , d += 128)      // loop for 3 pages
          Eep.Rd(addr, 128,d);
#ifdef TESTING     
     Serial.printf("%4hx]\n",addr-1);
 #endif     
     p->Cmd &= ~EEDYRD;       
    }
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
  static  int   lwh[2];
  static  short n[2];
  static  byte  lday,lmon,lmin,lidx = 255;       // -1 force an update at 1st call
          int   wh[2];
          byte  idx,mon;  
  

#ifdef AUTODATE
  if (Data.Clk.Min  > 59)   {Data.Clk.Min  = 0; Data.Clk.Hour++;}
  if (Data.Clk.Hour > 23)   {Data.Clk.Hour = 0; Data.Clk.Day++; Data.Clk.Yday++;}
  if (Data.Clk.Day  > 31)   {Data.Clk.Day = 1;}
  if (Data.Clk.Yday > 365)  {Data.Clk.Yday = 0; Data.Clk.Year++;}
 #else
    // Get MCP data
    Mcp.Poll();       // comment to have the terminal available
#endif

  // index of table (96 elements,zero based)          // bugfix (never happen)
  idx = Data.Clk.Hour * 4 + Data.Clk.Min / 15 -1;     if (idx > 95) idx = 95;
  mon = (Data.Clk.Mon -1) & 0x3;
  // at boot save the actual date and index (do nothint @ 1st call)
  if (lidx == 255)     {lidx = idx; lday = Data.Clk.Day; lmon = mon; return;}


  // ---- at every change (15 min update the current log (on ram)) ----
  if (idx != lidx)
    {
     // make a signed number for difference with energy in and out
     wh[0] = (int)(Data.Mcp.Wh.Pi[0] - Data.Mcp.Wh.Po[0]);
     wh[1] = (int)(Data.Mcp.Wh.Pi[1] - Data.Mcp.Wh.Po[1]);
     // save on array and normalize to +/- 32767 (-1 will be -2... because -1 is the cleared value)
     n[0] = wh[0] - lwh[0]; lwh[0] = wh[0]; if ((short)n[0] < 0) n[0]--; n[0] = Log.Log[idx][0] = n[0];
     n[1] = wh[1] - lwh[1]; lwh[1] = wh[1]; if ((short)n[1] < 0) n[1]--; n[1] = Log.Log[idx][1] = n[1];
     // Update the actual record on dayly log [0..30] and 4 months
     EeCmd.LDay = lday; EeCmd.Monx = mon; EeCmd.LMonx = lmon;  
     // starting address of table [month][day]
     EeCmd.Addr = DAYADDR + WHOFS + MONSIZE * (word) mon + DAYSIZE * ((word)lday -1) + (word) idx * 4;
     // save indexed record
     EeCmd.Idx = idx; EeCmd.Data = &n[0]; EeCmd.Cmd |= EEWRPW;
     // changed day , save lday limits 
     if (idx == 95)             EeCmd.Cmd |= EEWRPK;
     // changed month , clear buffer before write (~1.4 sec)
     if (mon != lmon)           EeCmd.Cmd |= EEMCLR;
     // at first record clear this mont then save the creation time
     if (!idx)                  EeCmd.Cmd |= EEWRCR;
     // UPdate last states
     lidx = idx;
     lday = Data.Clk.Day;
     lmon = mon;
    }                                                 

  // timer energy ( elapsed time)
  if (lmin != Data.Clk.Min)   {lmin = Data.Clk.Min; Data.Elaps[0]++; Data.Elaps[1]++;}

  // Update the min/max values and time event
  word tm = (word) Data.Clk.Hour * 256 + Data.Clk.Min;
  if (Data.Mcp.Vaw.Vmin    < Log.Vmin)    {Log.Vmin = Data.Mcp.Vaw.Vmin;        Log.TimeMinMax[0] = tm;}
  if (Data.Mcp.Vaw.Vmax    > Log.Vmax)    {Log.Vmax = Data.Mcp.Vaw.Vmax;        Log.TimeMinMax[1] = tm;}
  if (Data.Mcp.Vaw.Wmax[0] > Log.Wmax[0]) {Log.Wmax[0] = Data.Mcp.Vaw.Wmax[0];  Log.TimeMinMax[2] = tm;}
  if (Data.Mcp.Vaw.Wmax[1] > Log.Wmax[1]) {Log.Wmax[1] = Data.Mcp.Vaw.Wmax[1];  Log.TimeMinMax[3] = tm;}
  // also relative 
  if (Data.Wmax[0] < Data.Mcp.Vaw.W[0])   {Data.Wmax[0] = Data.Mcp.Vaw.W[0];    Data.Time[0] = tm;}
  if (Data.Wmax[1] < Data.Mcp.Vaw.W[1])   {Data.Wmax[1] = Data.Mcp.Vaw.W[1];    Data.Time[1] = tm;}


  // propagate auth. level
  if (Auth) Data.Flags |= Auth & 0x3; 
   else     Data.Flags &= ~0x3;
  //  
  // propagate the command read table on buffer
  //.....
  Data.RSSI = 234; WiFi.RSSI();
}

// ========================================================================================
//                                  WebServer CallBack 
// ========================================================================================


bool CmdCallBack(byte code, char *txt)
// ------------------------------------
// execute a webserver command callback
// ------------------------------------
{
 switch (code)
    {
      case  1:  // Reset Power #1
                Data.Wmax[0] = 0;
                Data.Time[0] = 0;
                Data.Elaps[0] = 0;
                break;
      case  2:  // Reset Power #1
                Data.Wmax[1] = 0;
                Data.Time[1] = 0;
                Data.Elaps[1] = 0;
                break;
      case  10: // set log index
                word v;  
                if (sscanf(txt,"%hd",&v))
                  { 
                  MIdx = v >> 8; DIdx = v & 0x7f;
#ifdef TESTING     
                  Serial.printf("M%02hhd:D%02hhd\n",MIdx,DIdx);
 #endif
                  EeCmd.Addr = DAYADDR + MONSIZE * ((word) MIdx -1) + DAYSIZE * ((word)DIdx -1); 
                  EeCmd.Data = &Log; 
                  EeCmd.Cmd |= EEDYRD;
                  }
                break;
      case  90: // Mcp Services command
                return(Mcp.Parse(txt));
                break;
    }
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
       clk->Mon  = tp->tm_mon + 1; clk->Yday = tp->tm_yday; clk->Day  = tp->tm_mday;    
       clk->Hour = tp->tm_hour;    clk->Min  = tp->tm_min;  clk->Sec  = tp->tm_sec;
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
  // Max Tx Power 
  WiFi.setOutputPower(20.5);
    // Start NTP client for timezone and update every 5 min timezone
  NTPClnt.begin(); NTPClnt.setTimeOffset(TIMEZONE);

}   
// ########################################################################


void loop(void) 
{
 
 
 #ifdef SERIALCMD
  char buf[32],ebuf[128];
  word a,b,n;
  
  
  memset(buf,0,sizeof(buf));
  
  if ((Serial.available() >= 1) && Serial.readBytes(buf, sizeof(buf)))
    {
      a = b = 0;
        // read a page (128 bytes)
        if (sscanf(buf,"rd %hx",&a) == 1)
          {
           Eep.Rd(a,128, ebuf);
           Serial.printf("\n%04hX ",a );
           for (n = 0; n < 128; n++) 
              { 
                if (n && !(n & 0xf)) Serial.printf("\n%04hX ",a + n);    // every 16, new line
                Serial.printf("%02hhX ",ebuf[n]);         // show byte
              }  
          }
        else
        if (sscanf(buf,"fmt %hd",&n) == 1)      {EeCmd.Monx = n; EeCmd.Cmd = EEMCLR;}
        else
        if (buf[0] == 'i') Data.Clk.Min ++;
       else
        if (buf[0] == 'd') {Data.Clk.Hour = 23; Data.Clk.Min = 30;}
       else
        if (buf[0] == 'm') {Data.Clk.Day = 31; Data.Clk.Hour = 23; Data.Clk.Min = 30;}
       else
      if (sscanf(buf,"%hd %hd",&a,&b))
       {
         if (a < 0) 
            {
              Data.Mcp.Wh.Po[0] += a; 
            }  
           else  
              {
                Data.Mcp.Wh.Pi[0] += a;
                Data.Mcp.Vaw.W[0] = a;
              }  
         if (b < 0) 
            {
              Data.Mcp.Wh.Po[1] += b; 
            }  
           else  
              {
                Data.Mcp.Wh.Pi[1] += b;
                Data.Mcp.Vaw.W[1] = b;
              }  
         Data.Clk.Min +=15;              
       }
    }
#endif

  switch (Boot)
        {
          case 1:   // booting, wait for a valid internet time (fast led blink 5/50ms)
                    UpdateClock();
                    if (NTPClnt.isTimeSet()) Boot++; 
                    LED(true); delay(250);LED(false);
                    break;
          case 2:   // Init I2C devices then start cyclic timer (GPIO[0,2] used fr I2C an not available)
                    // Init EEprom 512Kbit @400Khz and def.pins
                    if (!Eep.Begin(512,true))  Serial.println("EEprom Init Failed");
                    // Start AHT21 temp. and Humidity sensor
                    if (!Aht21.Begin(true))    Serial.println("AHT21 Init Failed, Temp. and Hum. unavailable");
                    Tick.attach_ms(250,Cyclic);
                    Boot++; 
                    break;
          case  3:  // dummy 
#ifndef   AUTODATE
                    Boot++;
 #endif                    
                    break;
          default:  // any other case
                    // Update global time (or continue from itself)
                    Aht21.Poll();             // MUST ce call cyclically (> 4) to read
                    // Every second
                    if (UpdateClock())
                      {
                        // update temp. and Hum data (~270us @400KHz)
                        if (Aht21.Read() == 1) {Data.Temp = Aht21.Temp; Data.Hum = Aht21.Hum;}
//    Serial.printf("%hd/%hhd/%hd %hhd:%02hhd(%hd,%hd)\n",Data.Clk.Yday,Data.Clk.Mon,Data.Clk.Year + 1900,Data.Clk.Hour,Data.Clk.Min,Data.Mcp.Wh.Pi[0],Data.Mcp.Wh.Po[0]);
                      }
                    break;
        }             // end switch BootSeq()
   //  Eeprom handler, MUST be in loop because blocking functions generate exception
   EeHnd(&EeCmd);
  // EatTime loop
  delay(50);
}                     // EndLoop
