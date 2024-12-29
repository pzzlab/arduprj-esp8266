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
//  0x0000..0x01ff    Unused
//  0x0200..0x3fff    Month[0]  TEeLog [31]    15872 bytes     interval 15 min    (96 recs)
//  0x4000..0x7dff    Month[1]  TEeLog [31]    15872 bytes     interval 15 min    (96 recs)
//  0x7e00..0xbbff    Month[2]  TEeLog [31]    15872 bytes     interval 15 min    (96 recs)
//  0xbc00..0xf9ff    Month[3]  TEeLog [31]    15872 bytes     interval 15 min    (96 recs)
//  0xfa00..0xffff    unused
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
#include <NTPClient.h>
#include <WiFiUdp.h>

#define   SERIALCMD                           // handle serial commands

#define   NPWR          2                     // Power Measure Zones
#define   TIMEZONE      3600 * 1              // gmt + 1
#define   NMIN          15

// EEprom Mapping defs
#define   BLKSIZE       0x0200
#define   DAYADDR       0x0200                

#define   EEWRCR        0x01                  // write creation date
#define   EEWRPW        0x02                  // write power (4 bytes)

#define   EEDYRD        0x10                  // Day Read (monthidx,dayidx) -> Table

#define   EEMCLR        0x80                  // Month clear (index)


typedef struct 
              {
                void   *Data;
                word    Addr;                 // address reference (or page if pgerase)
                byte    Cmd,Mon,Idx;          // command to execute (return 0 at done) , month
              } TeeCmd;                       // >>>> eeprom command struct to propate in 


typedef struct 
              {
                byte    Year,Mon,Day,Hour,Min,Sec;
                word    Yday;
              } TClock;                       // >>>> Date/time to here


typedef struct {
                TClock          Clk;          // 8 bytes
                word            Temp, Hum;    // Aht21 temperature and humudity
                word            Flags,_;      // general flags, filler
                TMcp39F511Data  Mcp;          // Mcp Data block
                word            Time[2];      // 4 bytes <inc every 15 min.(682 days)>
                word            RSSI;         // wifi signal
                byte            __[14];       // filler to reach 128 bytes
                short           Log[96][2];   // buffer (384 bytes)
               } TRtData;                     // 128 bytes



TRtData         Data;                         // complete image (data + log)
byte            Pwd[13];     // Password handling
byte            Boot = 1;                     // boot statemachine
word            TableIdx;                     // 
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
  {"Data",      "",       &Data,        Struct, 0x0000,   sizeof(Data)}
};

const byte  VarsCnt = sizeof(Vars) / sizeof(TVar);        // define the number of elements


// ========================================================================================



void EeHnd(TeeCmd *p)
{
 word addr;
 unsigned t1;

 if (p->Cmd & EEMCLR)
    {
     t1 = millis();
     addr = DAYADDR + BLKSIZE * 31 * (word) p->Mon;
     for (word lp =0; lp < 4 * 31; lp++, addr += 128)      // loop for 4 pages * 31 days
         {
          Eep.PgErase(addr); 
          Serial.printf("Erase [%hd] (%4hx -> %4hx)\n",p->Mon, addr, addr + 127);
         } 
     p->Cmd &= ~EEMCLR;       
     Serial.printf("Time: %u ms\n",millis() - t1); 
    }
     
if (p->Cmd & EEWRCR)
    {
     t1 = millis();
     addr = DAYADDR + BLKSIZE * 31 * (word) p->Mon;
//     Eep.Wr(addr, (byte)sizeof(TClock), &Data.Clk);
     Serial.printf("DyCreate: %hhd-%hhd (%4hx)\n",Data.Clk.Day,Data.Clk.Mon,addr); 
     p->Cmd &= ~EEWRCR;       
     Serial.printf("Time: %u ms\n",millis() - t1); 
    }
     
if (p->Cmd & EEWRPW)
    {
     t1 = millis();
     Eep.Wr(p->Addr, 4, p->Data);
     Serial.printf("[%2hhd] (%4hx) %5hd %5hd\n",p->Idx,p->Addr,0,0); // *(word*)p->Data,*(word*)(p->Data+1)
     p->Cmd &= ~EEWRPW;       
     Serial.printf("Time: %u ms\n",millis() - t1); 
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
  static  byte  lidx = 255;       // -1 force an update at 1st call
  static  byte  lday,lmon;
          byte  idx,mon;  
          word  addr;
          int   n[2],wh[2];

  // Get MCP data
  // Mcp.Poll();       // comment to have the terminal available

    if (Data.Clk.Min  > 59)   {Data.Clk.Min  = 0; Data.Clk.Hour++;}
    if (Data.Clk.Hour > 23)   {Data.Clk.Hour = 0; Data.Clk.Day++; Data.Clk.Yday++;}
    if (Data.Clk.Day  > 31)   {Data.Clk.Day = 1;}
    if (Data.Clk.Yday > 365)  {Data.Clk.Yday = 0; Data.Clk.Year++;}


  // index of table (96 elements,zero based)          // bugfix (never happen)
  idx = Data.Clk.Hour * 4 + Data.Clk.Min / 15 -1;     if (idx > 95) idx = 95;
  mon = (Data.Clk.Mon -1) & 0x3;
  // at boot save the actual date and index (do nothint @ 1st call)
  if (lidx == 255)     {lidx = idx; lday = Data.Clk.Day; lmon = mon; return;}
  
  // ---- at every change (15 min update the current log (on ram)) ----
  if (idx != lidx)
    {
     // make a signed number for difference with energy in and out
     wh[0] = (int)(Data.Mcp.Wh.Pi[0] - Data.Mcp.Wh.Po[0])  ;
     wh[1] = (int)(Data.Mcp.Wh.Pi[1] - Data.Mcp.Wh.Po[1])  ;
     // save on array and normalize to +/- 32767 (-1 will be -2... because -1 is the cleared value)
     n[0] = wh[0] - lwh[0]; lwh[0] = wh[0]; if ((short)n[0] < 0) n[0]--; Data.Log[idx][0] = n[0];
     n[1] = wh[1] - lwh[1]; lwh[1] = wh[1]; if ((short)n[1] < 0) n[1]--; Data.Log[idx][1] = n[1];
     // Update the actual record on dayly log [0..30] and 4 months
     // changed month , clear buffer before write (~1.4 sec)
     if (mon != lmon) {EeCmd.Mon = mon; EeCmd.Cmd |= EEMCLR;}
     // starting address of table [month][day]
     addr = DAYADDR + BLKSIZE * 31 * (word) mon + BLKSIZE * ((word)lday -1) ;
     // at first record save also the creation time
     if (!idx)    {EeCmd.Mon = mon; EeCmd.Cmd |= EEWRCR;}
     // calculate the index with day of previous and actual hour+min
     addr +=  0x80 + (word) idx * 4;
     EeCmd.Data = &n[0]; EeCmd.Addr = addr; EeCmd.Cmd |= EEWRPW;
     // UPdate last states
     lidx = idx;
     lday = Data.Clk.Day;
     lmon = mon;
    }                                                 

  // propagate auth. level
  if (Auth) Data.Flags |= Auth & 0x3; 
   else     Data.Flags &= ~0x3;
  //  
  // propagate the command read table on buffer
  //.....
  Data.RSSI = WiFi.RSSI();
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
  // Get starting time  
  UpdateClock();

}   
// ########################################################################


void loop(void) 
{
 
 
 #ifdef SERIALCMD
  char buf[32];
  word a,b;
  
  
  memset(buf,0,sizeof(buf));
  
  if ((Serial.available() >= 1) && Serial.readBytes(buf, sizeof(buf)))
    {
      a = b = 0;
        if (buf[0] == 'm') Data.Clk.Mon++;
       else
        if (buf[0] == 'd') Data.Clk.Day++;
       else
        if (buf[0] == 'c') {memset(Data.Log,0,sizeof(Data.Log)); Data.Clk.Mon = Data.Clk.Day = 1; Data.Clk.Hour = Data.Clk.Min = 0;}
       else
      if (sscanf(buf,"%hd %hd",&a,&b))
       {
         if (a < 0) Data.Mcp.Wh.Po[0] += a; else  Data.Mcp.Wh.Pi[0] += a;
         if (b < 0) Data.Mcp.Wh.Po[1] += b; else  Data.Mcp.Wh.Pi[1] += b;
         Data.Clk.Min +=15;              
       }
//    Serial.printf("%hd/%hhd/%hd %hhd:%02hhd(%hd,%hd)\n",Data.Clk.Yday,Data.Clk.Mon,Data.Clk.Year + 1900,Data.Clk.Hour,Data.Clk.Min,Data.Mcp.Wh.Pi[0],Data.Mcp.Wh.Po[0]);
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
                    Boot++; 
                    break;
          case  3:  // dummy 
                    break;
          default:  // any other case
                    // Update global time (or continue from itself)
                    Aht21.Poll();             // MUST ce call cyclically (> 4) to read
                    // Every second
/*                    if (UpdateClock())
                      {
                        // update temp. and Hum data (~270us @400KHz)
                        if (Aht21.Read() == 1) {Data.Temp = Aht21.Temp; Data.Hum = Aht21.Hum;}
                      }
  */                  break;
        }             // end switch BootSeq()
   //  Eeprom handler, MUST be in loop because blocking functions generate exception
   EeHnd(&EeCmd);
  // EatTime loop

  delay(10);
 
}                     // EndLoop
