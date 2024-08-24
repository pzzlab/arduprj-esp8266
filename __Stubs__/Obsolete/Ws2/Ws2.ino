

#include "AsWebSrv.h"
#include <ESPAsyncUDP.h>
#include "SRam.h"
#include <Ticker.h>

#define NZONE   6                           // Heating Zones
#define NPRG    8                           // Heating programs
#define NPWR    8                           // Power Measure Zones
#define DLYHEAT 15                          // off time when no zones on [S]
#define DLYPUMP 30                          // off time when no zones on [S] (MUST BE GREATER THAN DLYHEAT)


Ticker    Tick;                             // Ticker handler
TWebSrv   Web;                              // The WrbServer with access to vars and html pages
AsyncUDP  Udp;                              // Udp Services

TSRam SRam(8000000,1024,16);                // Static Serial Ram, 8MHz , 128KB , CsPin on GPIO16

typedef struct 
              {
                byte Day,Hour,Min,Sec;
              } TClock;                       // >>>> Date/time

typedef struct 
              {
                byte Time,Temp;
              } TMan;                       // >>>> Manual zone record 
typedef struct 
              {
                byte Ip,Attr;
              } TPwrMod;                    // >>>> Power Module features
typedef struct 
              {
                TClock  Clk;                // date an time
                byte    Temp  [NZONE + 1];  // temperatures of zones + external
                TMan    Man   [NZONE];      // manual time/temp for zones
                byte    InOut [3];          // physical in/out image
                // Power data
                byte    C     [NPWR];       // CosFi (0.01)
                short   W     [NPWR];       // Actual Power (0.1W)
                short   Wpk   [NPWR];       // Peak Power (0.1W)
                short   A     [NPWR];       // Actual Current(0.001A)
                short   V     [NPWR];       // Voltage (0.1V)
              } TAct;                       // >>>> Actal Data (read as binary) 
typedef struct 
              {
                byte    Pgm   [NPRG][48];   // The programs
                byte    ZNum  [7][NZONE];   // Program Associate per zona and day
                char    ZNames[NZONE][16];  // Zones Names
                char    TAdj  [NZONE];      // Temperature offset for every zone
                TClock  Clk;                // date an time (backup ciclic avery min.)
                byte    Kw[2];              // generated keyword(0=rnd,1=level)
                // Power data
                TPwrMod Mod   [NPWR];       // Power Module features (IP and attribute)
                char    PNames[NPWR][12];   // Power Names
                short   PMax  [NPWR];       // limit of power (show red bar) [W]
              } TSet;                       // >>>> Setup Data (read as binary and written as array)

TSet   Set;                                 // Set    data (one shot @page load)
TAct   Act;                                 // Actual data (polled cyclic)

byte   Pwd[13],*Kw=Set.Kw,BckClk;           // Global flag to backup current time to nvram
bool   Log = false;                          // Send on Serial @ every /Cmd and /Write with values
// ========================================================================================
// table containing the public vars accessible to web
// Attribute:     Bits 0..2 = char,short,int,float,..,string;
// Len = size of data in bytes
// Note: for Array ,the field .Fmt is a decimal value of elements and .Len the sie of one element
PROGMEM const TVar Vars[] = 
{
  {"Set",       "",        &Set,          Struct, 0x80,   sizeof(Set)},
  {"Act",       "",        &Act,          Struct, 0x80,   sizeof(Act)},
  {"Clock",     "%hhd",    &Act.Clk,      Array,  0x80,   sizeof(Act.Clk)},
  {"Man",       "%hhd",    &Act.Man,      Array,  0x80,   sizeof(Act.Man)},
  {"ZNum",      "%hhd",    &Set.ZNum,     Array,  0x80,   sizeof(Set.ZNum)},
  {"TAdj",      "%hhd",    &Set.TAdj,     Array,  0x80,   sizeof(Set.TAdj)},
  {"PMax",      "%hd",     &Set.PMax,     Array,  0x81,   sizeof(Set.PMax)},
  {"ZNames",    "6",       &Set.ZNames,   Array,  0x87,   sizeof(Set.ZNames[0])},
  {"PNames",    "6",       &Set.PNames,   Array,  0x87,   sizeof(Set.PNames[0])},
  {"Pgm0",      "%hhd",    &Set.Pgm[0],   Array,  0x80,   sizeof(Set.Pgm[0])},
  {"Pgm1",      "%hhd",    &Set.Pgm[1],   Array,  0x80,   sizeof(Set.Pgm[0])},
  {"Pgm2",      "%hhd",    &Set.Pgm[2],   Array,  0x80,   sizeof(Set.Pgm[0])},
  {"Pgm3",      "%hhd",    &Set.Pgm[3],   Array,  0x80,   sizeof(Set.Pgm[0])},
  {"Pgm4",      "%hhd",    &Set.Pgm[4],   Array,  0x80,   sizeof(Set.Pgm[0])},
  {"Pgm5",      "%hhd",    &Set.Pgm[5],   Array,  0x80,   sizeof(Set.Pgm[0])},
  {"Pgm6",      "%hhd",    &Set.Pgm[6],   Array,  0x80,   sizeof(Set.Pgm[0])},
  {"Pgm7",      "%hhd",    &Set.Pgm[7],   Array,  0x80,   sizeof(Set.Pgm[0])},
};
const byte  VarsCnt = sizeof(Vars) / sizeof(TVar);        // define the number of elements

// ========================================================================================

// --------------------------------------


void Default(void)
// -----------------------------------------
// Set default values on nvram (and Set)
// called after a check and invalid value
// ----------------------------------------
{
  memset(&Set,  0, sizeof(Set));                      
  for (byte n = 0; n < NZONE; n++)  sprintf(Set.ZNames[n],"Zona Numero %hhd",n);  
  memset(Set.PNames,0,sizeof(Set.PNames)); memset(Set.Mod,0,sizeof(Set.Mod));   // reset Act
  for (byte n = 0; n < NPWR; n++)   Set.PMax[n]= 5000;                  
  strcpy((char*)Pwd,"1234");
  SRam.WrBuf(0          , sizeof(Set), (byte*)&Set);                   // Store default values
  SRam.WrBuf(sizeof(Set), sizeof(Pwd), (byte*)&Pwd);                   // Store default password
}

void CmdCallBack(byte code,byte*data)
{
 if (code == 1)  {Udp.broadcast("PwrMeas?");}
}

void Toggle(void)
// -----------------------------------------
// Cyclic call avery second 
// -----------------------------------------
{
 static byte min = 0, dlyheat = 0, dlypump = 0, v = 0, rkw = 2; 
//  LED(true);
  
  // Clock Handling
  if (++Act.Clk.Sec >= 60)  {Act.Clk.Sec  = 0;  Act.Clk.Min++; min = 1; rkw++;}
  if (Act.Clk.Min   >= 60)  {Act.Clk.Min  = 0;  Act.Clk.Hour++;}
  if (Act.Clk.Hour  >= 24)  {Act.Clk.Hour = 0;  Act.Clk.Day++;}
  if (Act.Clk.Day   >= 7 )  {Act.Clk.Day  = 0;} 
  // InOut[0]: bits 0..5 = zones valves; bit 6 = heat run; bit 7 = recirculing pump;
  for (short i = 0; i < NZONE; i++) 
      { 
       byte z = Set.ZNum[Act.Clk.Day][i];                                               // Program Number for the zone @ Day
       byte set = Set.Pgm[z][Act.Clk.Hour * 2 + Act.Clk.Min/30];                        // Set Temperature
       if (Act.Man[i].Time)  set = Act.Man[i].Temp;                                     // Manual Override
       if (Act.Temp[i] < set) Act.InOut[0] |= 1 << i; else Act.InOut[0] &= ~(1 << i);   // Activate Output Zone Valve
       if (min && Act.Man[i].Time) Act.Man[i].Time--;
    }
   // Delayed Off heat and recirculing pump handlind
  if (Act.InOut[0] & 0x3f)  {dlyheat = DLYHEAT; dlypump = DLYPUMP;}
  if (dlypump--) Act.InOut[0] |= 0x80; else Act.InOut[0] &=~0x80;
  if (dlyheat--) Act.InOut[0] |= 0x40; else Act.InOut[0] &=~0x40;

  // -------------- temporary values to test webserver ----------------------
  // Random values
  v+=5; if (v > 245) v = 5;  for (byte i = 0; i < NZONE; i++) Act.Temp[i] = v;
  Act.Temp[NZONE] = rand() % 250;

  // Handle Power measuring
  for (byte i = 0; i < NPWR; i++) 
      {
        Act.V[i] = 2200 + rand() % 100;
        Act.A[i] = 100 + rand() % 15000;  Act.C[i] = 80 + i*2; 
        Act.W[i] = (int) ((Act.A[i] * Act.C[i]) >> 3) * Act.V[i] / 12500;
        if (Act.Wpk[i] < Act.W[i]) Act.Wpk[i] = Act.W[i];
      }
  // ------------------------------------------------------------------------
  // Reset every minute flag and set a global for backup on nvram
  if (min) {min = 0; BckClk = true;}
  // Autogenerate a new keyword for the access and invalidate the access word
  if (rkw > 1)  {rkw = 0; Set.Kw[0] = rand(); Set.Kw[1] = 0;}

//LED(false);  
}




// ########################################################################
// ########################################################################
// ########################################################################

void setup(void) 
{
  pinMode(LED_BUILTIN, OUTPUT);                       // Initialize the LED_BUILTIN pin as an output
  pinMode(0, INPUT_PULLUP);                           // Prog. button
  Serial.begin(115200);   Serial.println();           // Init Serial Port
  // Restore SRam Data
  if (!SRam.Chk())   {SRam.Init(true); Default(); Serial.println("** Reset Ram **");}    
   else               SRam.RdBuf(0,sizeof(Set),(byte*) & Set);
  // Restore the last clock at boot (autosaved every minute) 
  memcpy(&Act.Clk,&Set.Clk,sizeof(Act.Clk));
  // Activate WiFi then webserver
  Web.Begin("pwrmon",CmdCallBack);
  Serial.printf("%s ->@ %s [%s]\n",WiFi.SSID().c_str(),WiFi.getHostname(),WiFi.localIP().toString().c_str());
  if (Udp.listen(1234)) Serial.printf("..UDP Listen on  %s\n",WiFi.localIP().toString().c_str());
  
    // Station Wifi
  Tick.attach_ms(1000,Toggle);
}



// ########################################################################

char c, buf[128];

void loop(void) 
{
 delay(10);
 // Backup every minute of the last clock (will be reload at next boot as default)
 if (BckClk) {BckClk = false; SRam.WrBuf((TClock*)&Set.Clk - (TClock*)&Set, sizeof(Set.Clk), (byte*)&Act.Clk);}
 // There is something on rx buffer, echo n UDP
 if ((c = Serial.available()) > 0)
   {
    // Get data
    Serial.readBytes(buf, c);
    if (buf[0] == 'h') Act.Clk.Hour++;
    if (buf[0] == 'm') Act.Clk.Sec = 60;
    if (buf[0] == 'w') for (short i = 0; i < NPWR; i++) Act.Wpk[i] = 0;
    if (buf[0] == 'M') SRam.Init(0);
   }     

}


