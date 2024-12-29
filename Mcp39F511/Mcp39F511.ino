#include <AsWebSrv.h>
#include <Ticker.h>
#include <ESP8266LLMNR.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define   SERIALCMD   1                       // handle serial commands
#define   NPWR        2                       // Power Measure Zones
#define   TIMEZONE    3600 * 1                // gmt + 1
#define   NMIN        15
#define   SAMPD       (1440 / NMIN)           // Number of elements per log (day based)
#define   SAMPH       (60 / NMIN)             // samples per hour
#define   SAMPM       (24 * 31)               // Number of elements per log (month based)
#define   SAMPY       366                     // Number of elements per log (year based)

//#define   SIMTIME                             // uncomment to update the time manually

typedef struct 
              {
                int    Wh[NPWR][SAMPD];       // dayly Energy
                word   Day;                   // actual day (1..7)
                word   Yday;                  // Year Day (1..366)
               }TLog;                         //  772 bytes, (ram only)

typedef struct 
              {
                int    Wh[NPWR][SAMPM];       // Energy (sign define if import/export)
                word   Mon;                   // actual month (1..12)
                word   Res;                   // Reserve (align)
               }TLogM;                        // 5956 bytes (file + ram)
typedef struct 
              {
                int    Wh[NPWR][SAMPY];       // Energy (sign define if import/export)
                word   Year;                  // Current Year (2023..253)
                word   Res;                   // Reserve (align)
               }TLogY;                        // 2932 (file + ram)

typedef struct 
              {
                byte    Year,Mon,Day,Hour,Min,Sec;
                word    Yday;
              } TClock;                       // >>>> Date/time

typedef struct 
              {
                word    V;                    // Line Voltage
                word    F;                    // Line Frequency
                short   C[NPWR];              // CosFi (0.01)
                short   A[NPWR];              // Actual Current(0.001A)
                short   W[NPWR];              // Actual Power (0.1W)
                short   WmI[NPWR];            // Max Power Input(W)
                short   WmO[NPWR];            // Max Power Output(W)
              } TVaw;                         // >>>> VoltAmpereWatts Block

typedef struct 
              {
                word    Time[NPWR];           // time in Hours
                int     P[NPWR];              // active power
                int     Q[NPWR];              // reactive power 
              } TWh;                          // >>>> Energy block

typedef struct
              {
                TClock  Clk;                  // date and time
                TVaw     Vaw;
                TWh      Wh;   
                byte    Kw[2];                // generated keyword(0=random,1=level)
                byte    resb[2];              // Filler
              } TMeasData;


WiFiUDP ntpUDP;                               // For NTP time client
NTPClient NTPClnt(ntpUDP, "pool.ntp.org");    // Intenet time 

Ticker          Tick;                         // Ticker handler
TWebSrv         Web;                          // The WebServer with access to vars and html pages
TMeasData       Data;                         // V, A W, Wh...
TLog            Log;                          // daily ram log
byte            Pwd[13],*Kw=Data.Kw;          // Password handling
byte            Verbose;                      // verbose flag (will be overwritten into setup.cfg)
byte            LogsUpd,BootState = 1;        // Log (rename, backup..) and Boot statemachine,
char            LstY[10][6],Ny;               // name and number of yearly files (max 10)
byte            LMon,LYear;

// ========================================================================================
// table containing the public vars accessible to web
// Attribute:     Bits 0..2 = char,short,int,float,..,string;
// Len = size of data in bytes
// Note: for Array ,the field .Fmt is a decimal value of elements and .Len the sie of one element
PROGMEM const TVar Vars[] = 
{
// Name         Fmt       Obj           Type    Attr    Len    
  {"Data",      "",       &Data,        Struct, 0x80,   sizeof(Data)},
  {"Log",       "",       &Log,         Struct, 0x80,   sizeof(Log)},
  {"LstY",      "10",     &LstY[0],     Array,  0x87,   sizeof(LstY[0])}
};
const byte  VarsCnt = sizeof(Vars) / sizeof(TVar);        // define the number of elements

// ========================================================================================

void LstLog() 
// ----------------------------------------------
// List on "/log/" every file wih start with 'y'
// and copy on the global array List
// return the number of files found
// ---------------------------------------------
{
  Dir d = Fs.openDir("/log/");
  memset(LstY,0,sizeof(LstY)); Ny = 0;
  if (Verbose & 0x2) Serial.println("Log List:");
  while (d.next() && (Ny <= 10)) 
    {
      File f = d.openFile("r");
      if (f.name()[0] == 'y') strcpy(LstY[Ny++],(char*)f.name() + 1);
      if (Verbose & 0x2) Serial.printf("%s ",f.name());
      f.close();
    }
  if (Verbose & 0x2) Serial.println("");
}

// ----------------------------------------------------------------------------

bool CreateLog(char type, word num) 
// -----------------------------------
// Create an empty Log file 
// -----------------------------------
{
  int r = 0; File f; char fname[16];
  sprintf(fname,"/log/%c%hu",type,num);
  if (Verbose & 0x2) Serial.printf("Create %s\n",fname);
  // file do not exists, create a new empty (write 1 byte at time)
  if (!(f = Fs.open(fname, "w+")))   return(false);
  switch (type)
        {
          case 'm':   for (r = 0; r < sizeof(TLogM); r++ ) f.write(0); break;
          case 'y':   for (r = 0; r < sizeof(TLogY); r++ ) f.write(0); break;
        }
  f.close(); return(true);
}

// ----------------------------------------------------------------------------

bool UpdtLogs(void) 
// -------------------------------------
// Update the monthly and yearly log
// extracting the data from daily.
// 1) if exist a year log -10. remove
// rename m0 -> m1 and create empty m0
// 2) reduce day -> month and update
// 3) reduce day -> year and update
// 4) cleanup the daily an update LstY
// 5) if changed year, create a new
// 6) if changed month,delete m1,
// NOTE: To avoid to have many write 
// access to flash, allocate in ram
// the image of portion to save.
// worst case: (change year)
// Write access are:
// New day    ->  2
// new mon    ->  3
// new year   ->  1 + 1 + 3
// updt mon   ->  2
// updt year  ->  2  
// total year:    7
// total month:   5
// 20 years: 366 * 2 + 20 * (12 * 5 + 7) 
//           ->> 2072 flashwrite cycles
// -------------------------------------
{
  char  n, n1, fname[16];  File  f; 
  int   i, ofs, whd[2], whh[2], buf[2][24];
 
  // remove (if exist) the year - 10
  sprintf(fname,"/log/y%hd",Data.Clk.Year + 1890);  if (Fs.exists(fname)) Fs.remove(fname);
  // scan the entire day to calculate and write
  memset(&buf,0,sizeof(buf));
  // extract data from day to temp. data to write
  for (n = n1 = i = whh[0] = whh[1] = whd[0] = whd[1] = 0; i < SAMPD; i++)
    {
      // sum of records (hour and day)
      whh[0] += Log.Wh[0][i]; whh[1] += Log.Wh[1][i]; 
      whd[0] += Log.Wh[0][i]; whd[1] += Log.Wh[1][i];
      // save every hour on montly buffer
      if (++n1 >= SAMPH)  {buf[0][n] = whh[0]; buf[1][n++] = whh[1]; whh[0] = whh[1] = n1 = 0;}
    }
  // open the /log/m0 file to update 
  if (!(f = Fs.open("/log/m0", "r+")))   {Serial.println("R+ error");return(false);}
  // calc. offset where write the image
  ofs  = 24 * sizeof(int) * (Log.Day - 1);  f.seek(ofs, SeekSet);  f.write((char*)&buf[0], sizeof(buf[0]));
  ofs += sizeof(int) * SAMPM;               f.seek(ofs, SeekSet);  f.write((char*)&buf[1], sizeof(buf[1]));
  // close the /log/m0 and build last year name
  f.close();   sprintf(fname,"/log/y%hd", LYear + 1900);
  // open the /log/yxxxx file to update
  if (!(f = Fs.open(fname, "r+")))  {Serial.println("R+ error");return(false);}
  // calc. offset where write the image
  ofs  = sizeof(int) * (Log.Yday -1);  f.seek(ofs, SeekSet); f.write((char*)&whd[0], sizeof(whd[0]));
  ofs += sizeof(int) * SAMPY;          f.seek(ofs, SeekSet); f.write((char*)&whd[1], sizeof(whd[1]));
  // close year log and cleanup dayly
  f.close();    memset(&Log,0,sizeof(Log));
  // changed year, create a new empty year if not exist
  if (Data.Clk.Year != LYear)   CreateLog('y', Data.Clk.Year + 1900);
  // changed month, delete older, rename m0 -> m1 and create an empty m0
  if (Data.Clk.Mon != LMon)  {Fs.remove("/log/m1"); Fs.rename("/log/m0", "/log/m1"); CreateLog('m', 0);}
  // save for nxt compare (also at boot)
  LMon = Data.Clk.Mon; LYear = Data.Clk.Year;
  // update the list of availables
  LstLog();
  return(true);
}

// ----------------------------------------------------------------------------

void Add2Log(int *wh)
// -----------------------------------------
// Add to daily log a new record.
// Called in to Cyclic() every NMIN
// when the time ocverflow (over 0.00)
// start a flag to update the log files
// -----------------------------------------
{
  TClock *clk = & Data.Clk;
  word i = clk->Hour * (60 / NMIN) + clk->Min / NMIN -1;        // calc. index from time
  // handle overflow (00.00) 
  if (i > SAMPD) {i = 0; LogsUpd = 1;}                          // start update logs (in loopback)
   else  
    {
     // for every device, save the result on dayly log
#ifndef SIMTIME     
     Log.Wh[0][i] = wh[0];    Log.Wh[1][i] = wh[0];             // save readings on log
#endif     
     Log.Day = Data.Clk.Day;  Log.Yday = Data.Clk.Yday;         // update logs date
    }    
}

// ----------------------------------------------------------------------------

void Cyclic(void)
// -----------------------------------------
// Cyclic call avery 500ms 
// -----------------------------------------
{
  static byte lmin, rkw = 2;
  static int  wh[2];
  


  // Update the read values
  wh[0] = Data.Vaw.W[0]; wh[1] = Data.Vaw.W[1];
  
  // ---------------- TIME SYNCHRONIZED  ----------------
  // increment time every minute (every NMIN will update logs)
  if (Data.Clk.Min != lmin)     
     {
       lmin = Data.Clk.Min; Data.Wh.Time[0]++; Data.Wh.Time[1]++;
       // every NMIN insert a new data to log
       if (!(Data.Clk.Min % NMIN))   Add2Log(&wh[0]);
     }

  // Autogenerate a new keyword for the access and invalidate the access word
  if (rkw > 1)  {rkw = 0; Data.Kw[0] = random(128); Data.Kw[1] = 0;}
}



bool CmdCallBack(byte code,char *data)
// ----------------------------------
// execute a webserver command 
// ----------------------------------
{
  TVaw * v = &Data.Vaw; TWh *w = &Data.Wh;

  switch (code)
          {
            case 1:   // reset #1
                      v->WmI[0] = v->WmO[0] = w->Time[0] = 0; break;
            case 2:   // reset #2
                      v->WmI[1] = v->WmO[1] = w->Time[1] = 0; break;
          }
  return(false);
}

// ----------------------------------------------------------------------------


// ########################################################################
// ########################################################################
// ########################################################################


void setup(void) 
{
  pinMode(LED_BUILTIN, OUTPUT);                       // Initialize the LED_BUILTIN pin as an output
  pinMode(0, INPUT_PULLUP);                           // Prog. button
  Serial.begin(115200);   Serial.setTimeout(50); Serial.println(); // Init Serial Port, and short timeout
  // Activate WiFi then webserver (Setup access will be available after this)
  if (!Web.Begin(CmdCallBack))  {LED(true); Serial.println("\nError on WebServer Init");}
   else                         
    if (Verbose) Serial.printf("%s ->@ %s [%s]\n",WiFi.SSID().c_str(),WiFi.getHostname(),WiFi.localIP().toString().c_str());
    Verbose = 2;
  // Start NTP client 
  NTPClnt.begin();  NTPClnt.setTimeOffset(TIMEZONE);  
}

// ########################################################################


void loop(void) 
{
 static char buf[16],i;
 char fname[16],bld;  
 word n;
 delay(25);

 // Manually increment of time
 #ifdef SIMTIME
 if ((Serial.available() >= 1) && Serial.readBytes(buf, sizeof(buf)))
   {
     bld = 0;
    if (buf[0] == 'd')  bld++;
    if (buf[0] == 'm')  {Data.Clk.Mon  ++;   Data.Clk.Yday += 30; bld++;}
    if (buf[0] == 'y')  {Data.Clk.Year ++;   Data.Clk.Mon  ++; bld++;}
    if (buf[0] == '+')  Data.Clk.Min+=15; 
    if (bld || (buf[0] == 'c'))  
        {
        for (i = 0; i < SAMPD; i++) {Log.Wh[0][i] = (Data.Clk.Yday & 0x3f) * 100 + i/4; Log.Wh[1][i] = -Log.Wh[0][i];}
        Data.Clk.Hour = 23; Data.Clk.Min = 45;
        }
   }
   // overflow handling of time 
   if (Data.Clk.Min  > 59) {Data.Clk.Min  = 0;  Data.Clk.Hour++;}  
   if (Data.Clk.Hour > 23) {Data.Clk.Hour = 0;  Data.Clk.Day++; Data.Clk.Yday++;}  
   if (Data.Clk.Day  > 30) {Data.Clk.Day  = 0;  Data.Clk.Mon++;}  
   if (Data.Clk.Mon  > 12) {Data.Clk.Mon  = 0;  Data.Clk.Year++;}
#endif
  // ========================= BOOTING SEQUENCE =======================================
  // 1) wait until a valid internet time (fast blink led 5/50ms)
  // 2) load the last "/log/d0" and save on Active Log(0) or create an empty if not
  // 3) create a list of logs into dedicated arrays then start timer service
  // 4) 
  //  ) runtime loop(update clock,..)
  
  time_t t; struct tm *tp;

  switch (BootState)
        {
          case 1:   // booting, wait for a valid internet time (fast led blink 5/50ms)
                    if (!NTPClnt.update()) {LED(true); delay(5);LED(false); break;}
#ifdef  SIMTIME
                    Data.Vaw.V    = 2234;
                    Data.Vaw.F    = 4999;
                    Data.Vaw.C[0] = Data.Vaw.C[1] = 126;
                    Data.Vaw.A[0] = Data.Vaw.A[1] = 12345;
                    Data.Clk.Mon  = 1;
                    Data.Clk.Day  = 3;
                    Data.Clk.Yday = 3;
                    Data.Clk.Year = 2023-1900;
#endif              
                    // valid internet time, save for next compare                  
                    t = NTPClnt.getEpochTime(); tp = gmtime ((time_t *)&t);       
                    LYear    = tp->tm_year;
                    LMon     = tp->tm_mon + 1;
                    BootState++; 
                    break;
          case 2:   // create an empty m0 file if not exist
                    if (!Fs.exists("/log/m0")) CreateLog('m',0);
                    BootState++; 
                    break;
          case 3:   // create an empty m1 file if not exist
                    if (!Fs.exists("/log/m1")) CreateLog('m',1);
                    BootState++; 
                    break;
          case 4:   // create an empty yxxxx file if not exist
                    sprintf(fname,"/log/y%hu", LYear + 1900);
                    if (!Fs.exists(fname))     CreateLog('y',LYear + 1900);
                    BootState++; 
                    break;
          case 5:   // start cyclic timer
                    LstLog();
                    Tick.attach_ms(500,Cyclic);
                    BootState++; 
                    break;
          default:  // any other case
#ifndef SIMTIME                
                    // Update global time every sec (and update current week on active log))
                    NTPClnt.update(); t = NTPClnt.getEpochTime(); tp = gmtime ((time_t *)&t); 
                    TClock * c = &Data.Clk;
                    if (c->Sec != tp->tm_sec)
                      {
                        c->Mon  = tp->tm_mon + 1; c->Yday = tp->tm_yday;     
                        c->Day  = tp->tm_mday;    c->Hour = tp->tm_hour;
                        c->Min  = tp->tm_min;     c->Sec  = tp->tm_sec;
                        c->Year = tp->tm_year;                
                      }
#endif                      
                    // update the chain of logs (@ every day change)
                    if (LogsUpd) {LogsUpd = 0;    UpdtLogs();}

        }             // end switch BootSeq()

}
