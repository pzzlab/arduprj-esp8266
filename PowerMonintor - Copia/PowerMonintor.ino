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

// Lookup table for the 1st day of the year from 2024 to 2044
// to calc. the day of the week = (_1_1_2024[Clk.Year-124] - 1 + Clk.Yday) % 7 where 0 is Sunday
const byte _1_1_2024 [] = { 1,  // 2024
                            3,  // 2025
                            4,  // 2026
                            5,  // 2027
                            6,  // 2028
                            1,  // 2029
                            2,  // 2030
                            3,  // 2031
                            4,  // 2032
                            6,  // 2033
                            0,  // 2034
                            1,  // 2035
                            2,  // 2036
                            4,  // 2037
                            5,  // 2038
                            6,  // 2039
                            0,  // 2040
                            2,  // 2041
                            3,  // 2042
                            4,  // 2043
                            5   // 2044
                          };

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
                word            RSSI;         // wifi signal
                byte            __[14];       // filler to reach 128 bytes
                short           Log[96][2];   // buffer (384 bytes)
               } TRtData;                     // 128 bytes



TRtData         Data;                         // complete image (data + log)
byte            Pwd[13],Keyw[2],*Kw=Keyw;     // Password handling
byte            Boot = 1;                     // boot statemachine
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
 for (n = 0; n < 95; n++) {Data.Log[n][0] = base + n +1;  Data.Log[n][1] = base - n -2;}
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
  static  byte  lidx[3] = {255,255,255};       // -1 force an update at 1st call
  static  byte  lday,ldw;
          byte  idx[3]  = {0,0,0};  
          word  addr;
          int   n[2],wh[2];

  // Get MCP data
  // Mcp.Poll();       // comment to have the terminal available

    if (Data.Clk.Min  > 59)   {Data.Clk.Min  = 0; Data.Clk.Hour++;}
    if (Data.Clk.Hour > 23)   {Data.Clk.Hour = 0; Data.Clk.Day++; Data.Clk.Yday++;}
    if (Data.Clk.Day  > 31)   {Data.Clk.Day = 1;}
    if (Data.Clk.Yday > 365)  {Data.Clk.Yday = 0; Data.Clk.Year++;}


  // index of table (96 elements,but point to -1)
  idx[0] = Data.Clk.Hour * 4 + Data.Clk.Min / 15 -1;
  if (idx[0] > 95) idx[0] = 95;
  // at boot save the actual date and index (do nothint @ 1st call)
  if (lidx[0] == 255)     {lidx[0] = idx[0]; lday = Data.Clk.Day; return;}
  
  // ---- at every change (15 min update the current log (on ram)) ----
  if (idx[0] != lidx[0])
    {
     // make a signed number for difference with energy in and out
     wh[0] = (int)(Data.Mcp.Wh.Pi[0] - Data.Mcp.Wh.Po[0])  ;
     wh[1] = (int)(Data.Mcp.Wh.Pi[1] - Data.Mcp.Wh.Po[1])  ;
     // save on array and normalize to +/- 32767 (-1 will be -2... because -1 is the cleared value)
//     n[0] = wh[0] - lwh[0]; lwh[0] = wh[0]; if ((short)n[0] < 0) n[0]--; Data.Log[idx[0]][0] = n[0];
//     n[1] = wh[1] - lwh[1]; lwh[1] = wh[1]; if ((short)n[1] < 0) n[1]--; Data.Log[idx[0]][1] = n[1];
     // -----------------------------------------------------------------------

     // Update the actual record on dayly log [0..30]
     // and at the first write also save the creating date
     // at first write,save the creation time
     if (!idx[0])                                                
        {
         // the date is located at beginning of file
         addr = DAYADDR + (lday -1) * BLKSIZE;
         //    Eep.Wr(addr, (byte)sizeof(TClock), &Data.Clk);
          Serial.printf("DyCreate: %hhd-%hhd-%hd (%4hx)\n",Data.Clk.Day,Data.Clk.Mon,(word)Data.Clk.Year + 1900,addr); 
        }
     // calculate the index with day of previous and actual hour+min
     addr = DAYADDR + (lday -1) * BLKSIZE + (word) idx[0] * 4 + 0x80;
     //  Eep.Wr(addr, 4, &n[0]);
     Serial.printf("Dy:%hhd-%hhd:%hhd [%2hhd] (%4hx) %5hd\n",Data.Clk.Day,Data.Clk.Hour,Data.Clk.Min,idx[0],addr,n[0]);
     lidx[0] = idx[0];
     lday    = Data.Clk.Day;




     
      // -----------------------------------------------------------------------
     // Update the actual record on weekly log [0..5]
     // and at the first write also save the creating date
     //  week and day of week  calculation from lookup table
     
     word tmp = Data.Clk.Yday + _1_1_2024[Data.Clk.Year - 124] - 1;
     word wh[2] = {0,0};
     byte wk    = tmp / 7, dw  = tmp % 7;
     // this is the table index 2 hours
     idx[1] = (dw * 12) + (Data.Clk.Hour >> 1) -1; 
     if (idx[1] > 95) idx[1] = 95;
     // at boot save the actual date and index (do nothint @ 1st call)
     if (lidx[1] == 255)     {lidx[1] = idx[1]; ldw = dw; return;}

     if (idx[1] != lidx[1])
        {
     // save on array and normalize to +/- 32767 (-1 will be -2... because -1 is the cleared value)
     Data.Log[idx[1]][0] = 2; Data.Log[idx[1]][1] = -2;


         if (!idx[1]) 
            {
             addr = WKADDR + (wk % 5) * BLKSIZE; 
    //       Eep.Wr(addr ,(byte)sizeof(TClock),&Data.Clk);
             Serial.printf("DyCreate: %hhd-%hhd-%hd (%4hx)\n",Data.Clk.Day,Data.Clk.Mon,(word)Data.Clk.Year + 1900,addr); 
            }
         // build the sum of the 8 records before (15 min * 8 = 2 Hour)
         for (byte n = 1; n <= 8; n++)  {wh[0] += Data.Log[idx[0]-n][0]; wh[1] += Data.Log[idx[0]-n][1];}
         addr = WKADDR + (wk % 5) * BLKSIZE + idx[1] * 4 + 0x80; 
         //    Eep.Wr(addr + (short) idx * 4 + 0x80, 4, &wh);
         Serial.printf("%hd,%hhd,%hhd [%hhd](0x%04hx) %5hd\n",tmp,wk,dw,idx[1], addr,wh[0]);
         lidx[1] = idx[1];
         ldw     = dw; 
        }                                             // idx[1] != lidx[1]



    }                                                 // idx[0] != lidx[0]

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
  WiFi.setOutputPower(20.5);
  
  // Start NTP client for timezone and update every 5 min timezone
  NTPClnt.begin(); NTPClnt.setTimeOffset(TIMEZONE);
  
  UpdateClock();
/*
  Data.Clk.Year = 2024 -1900;
  Data.Clk.Mon  = 12;
  Data.Clk.Day  = 22;
*/
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
      if (sscanf(buf,"%hd %hd",&a,&b))
       {
         if (a < 0) Data.Mcp.Wh.Po[0] += a; else  Data.Mcp.Wh.Pi[0] += a;
         if (b < 0) Data.Mcp.Wh.Po[1] += b; else  Data.Mcp.Wh.Pi[1] += b;
         Data.Clk.Min +=15;              
       }
       else
      if (sscanf(buf,"f %hd",&a))
       {
         FillLog(a);
         Data.Clk.Hour = 23; Data.Clk.Min = 30;              
       }
       else
        if (!strcmp(buf,"bld"))
            {
              Serial.println("---------------");
             for (byte d = 0; d < 2; d++)
                {
                  for (byte m = 0; m < 96; m++) 
                      {
                        Data.Clk.Min += 15;
                        Data.Mcp.Wh.Pi[0] = (word)d * 100 + m;
                        delay(300);
                      }
                }       
            }
       else
        if (buf[0] == 'm') Data.Clk.Min+=15;
       else
        if (buf[0] == 'h') Data.Clk.Hour++;
       else
        if (buf[0] == 'd') Data.Clk.Day++;
       else
        if (buf[0] == 'c') {memset(Data.Log,0,sizeof(Data.Log)); Data.Clk.Yday = Data.Clk.Day = 1; Data.Clk.Hour = Data.Clk.Min = 0;}
    


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
    // EatTime loop
  delay(10);
 
}                     // EndLoop
