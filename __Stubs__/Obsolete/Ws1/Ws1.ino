
#include "WebSrv.h"
#include "SRam.h"
#include <Ticker.h>

#define NZONE   6                           // Heating Zones
#define NPRG    8                           // Heating programs
#define NPWR    6                           // Power Measure Zones
#define DLYHEAT 15                          // off time when no zones on [S]
#define DLYPUMP 30                          // off time when no zones on [S] (MUST BE GREATER THAN DLYHEAT)


Ticker  Tick;                               // Ticker handler
TWebSrv Web;                                // The WrbServer with access to vars and html pages
//TSRam SRam(8000000,1024,16);                // Static Serial Ram, 8MHz , 128KB , CsPin on GPIO16

typedef struct 
              {
                byte Time,Temp;
              } TMan;                       // >>>> Manual zone record 
typedef struct 
              {
                byte  Day,Hour,Min,Sec;     // actual time
                byte  Temp  [NZONE + 1];    // temperatures of zones + external
                TMan  Man   [NZONE];        // manual time/temp for zones
                byte  InOut [3];            // physical in/out image
                // Power data
                byte  C     [NPWR];         // CosFi (0.01)
                short W     [NPWR];         // Actual Power (0.1W)
                short Wpk   [NPWR];         // Peak Power (0.1W)
                short A     [NPWR];         // Actual Current(0.001A)
                short V     [NPWR];         // Voltage (0.1V)
              } TAct;                       // >>>> Actal Data (read as binary) 
typedef struct 
              {
                byte  Pgm   [NPRG][48];     // The programs
                byte  ZNum  [7][NZONE];     // Program Associate per zona and day
                char  ZNames[NZONE][16];    // Zones Names
                char  TAdj  [NZONE];        // Temperature offset for every zone
                char  Day,Fill;             // copy of Act.Day (for show programs/zones) and filler byte
                // Power data
                char  PNames[NPWR][12];     // Power Names
                short Wmax  [NPWR];         // limit of power (show red bar) [W]
              } TSet;                       // >>>> Setup Data (read as binary and written as array)

TSet   Set;                                 // Set    data (one shot @page load)
TAct   Act;                                 // Actual data (polled cyclic)
// ========================================================================================
// table containing the public blocks arranged as array accessible to web
// Attribute:     Bits 0..2 = int,float;
// Len = size of data in bytes
PROGMEM const TBlk  Blocks[] = 
{
 {&Set,      0x80,   sizeof(Set)},
 {&Act,      0x80,   sizeof(Act)}
};
const byte  BlksCnt = sizeof(Blocks) / sizeof(TBlk);           // define the number of elements

// ========================================================================================
// table containing the public Arrays accessible to web
// Attribute:     Bits 0..2 = int,float,..,string;
// Len = size of data in bytes
PROGMEM const TVar Arrays[] = 
{
  {"Man",       "%hhd",    &Act.Man,      0x80,   sizeof(Act.Man)},
  {"ZNum",      "%hhd",    &Set.ZNum,     0x80,   sizeof(Set.ZNum)},
  {"ZNames",    "%s",      &Set.ZNames,   0x87,   sizeof(Set.ZNames)},
  {"PNames",    "%s",      &Set.PNames,   0x87,   sizeof(Set.PNames)},
  {"Pgm0",      "%hhd",    &Set.Pgm[0],   0x80,   sizeof(Set.Pgm[0])},
  {"Pgm1",      "%hhd",    &Set.Pgm[1],   0x80,   sizeof(Set.Pgm[0])},
  {"Pgm2",      "%hhd",    &Set.Pgm[2],   0x80,   sizeof(Set.Pgm[0])},
  {"Pgm3",      "%hhd",    &Set.Pgm[3],   0x80,   sizeof(Set.Pgm[0])},
  {"Pgm4",      "%hhd",    &Set.Pgm[4],   0x80,   sizeof(Set.Pgm[0])},
  {"Pgm5",      "%hhd",    &Set.Pgm[5],   0x80,   sizeof(Set.Pgm[0])},
  {"Pgm6",      "%hhd",    &Set.Pgm[6],   0x80,   sizeof(Set.Pgm[0])},
  {"Pgm7",      "%hhd",    &Set.Pgm[7],   0x80,   sizeof(Set.Pgm[0])},
};
const byte  ArraysCnt = sizeof(Arrays) / sizeof(TVar);        // define the number of elements

// ========================================================================================
// table containing the public vars accessible to web
// Attribute:     Bits 0..2 = int,float,..,string;
// Len = size of data in bytes
PROGMEM const TVar Vars[] = 
{
  {"Vers",      "%s",      &Web.Vers,  0x7,    sizeof(Web.Vers)},
  {"Wifi",      "%s",      &Web.Ssid,  0x87,   sizeof(Web.Ssid)},
  {"IP",        "%s",      &Web.Ip,    0x87,   sizeof(Web.Ip)},
  {"Host",      "%s",      &Web.Host,  0x87,   sizeof(Web.Host)},
};
const byte  VarsCnt = sizeof(Vars) / sizeof(TVar);        // define the number of elements
word PageVars[MAXVARSPAGE],NVarsPage;                     // Table where saved the vars to refresh into a page

// ========================================================================================

PROGMEM const char DefZone[][16] =  {
                                    {"Zona1"},
                                    {"Zona2"},
                                    {"Zona3"},
                                    {"Zona4"},
                                    {"Zona5"},
                                    {"Zona6"}
                                   };

PROGMEM const char DefPwr[][12] =  {
                                    {"Pwr1"},
                                    {"Pwr2"},
                                    {"Pwr3"},
                                    {"Pwr4"},
                                    {"Pwr5"},
                                    {"Pwr6"}
                                   };
// --------------------------------------


void Def(void)
// -----------------------------------------
// Set default values on eeprom (and Set)
// called after a check and invalid value
// -----------------------------------------
{
  memset(Set.Pgm,  0, sizeof(Set.Pgm));                      
  memset(Set.ZNum, 0, sizeof(Set.ZNum));                          
  memset(Set.TAdj, 0, sizeof(Set.TAdj));                    
  for (byte n = 0; n < NZONE; n++)  strcpy(Set.ZNames[n], DefZone[n]);  
  for (byte n = 0; n < NPWR; n++)   strcpy(Set.PNames[n], DefPwr[n]);    
  for (byte n = 0; n < NPWR; n++)   Set.Wmax[n]= 5000;                  
  //SRam.WrBuf(0, sizeof(Set), (byte*)&Set);                   // Store default values
}

void Toggle(void)
// -----------------------------------------
// Cyclic call avery second 
// -----------------------------------------
{
 static byte chgmin = 0, dlyheat = 0, dlypump = 0, v = 0; 
//  LED(led); led = !led;
  // Random values
  v+=5; if (v > 245) v = 5;  for (byte i = 0; i < NZONE; i++) Act.Temp[i] = v;
  Act.Temp[NZONE] = rand() % 250;
  
  // Clock Handling
  if (++Act.Sec >= 60)  {Act.Sec  = 0;  Act.Min++; chgmin = 1;}
  if (Act.Min   >= 60)  {Act.Min  = 0;  Act.Hour++;}
  if (Act.Hour  >= 24)  {Act.Hour = 0;  Act.Day++;}
  if (Act.Day   >= 7 )  {Act.Day  = 0;} Set.Day = Act.Day;
  // InOut[0]: bits 0..5 = zones valves; bit 6 = heat run; bit 7 = recirculing pump;
  for (short i = 0; i < NZONE; i++) 
      { 
       byte z = Set.ZNum[Act.Day][i];                                                   // Program Number for the zone @ Day
       byte set = Set.Pgm[z][Act.Hour * 2 + Act.Min/30];                                // Set Temperature
       if (Act.Man[i].Time)  set = Act.Man[i].Temp;                                     // Manual Override
       if (Act.Temp[i] < set) Act.InOut[0] |= 1 << i; else Act.InOut[0] &= ~(1 << i);   // Activate Output Zone Valve
       if (chgmin && Act.Man[i].Time) Act.Man[i].Time--;
    }
   // Delayed Off heat and recirculing pump handlind
  if (Act.InOut[0] & 0x3f)  {dlyheat = DLYHEAT; dlypump = DLYPUMP;}
  if (dlypump--) Act.InOut[0] |= 0x80; else Act.InOut[0] &=~0x80;
  if (dlyheat--) Act.InOut[0] |= 0x40; else Act.InOut[0] &=~0x40;
  // Handle Power measuring
  for (byte i = 0; i < NPWR; i++) 
      {
        Act.V[i] = 2200 + rand() % 100;
        Act.A[i] = 100 + rand() % 15000;  Act.C[i] = 80 + i*2; 
        Act.W[i] = (int) ((Act.A[i] * Act.C[i]) >> 3) * Act.V[i] / 12500;
        if (Act.Wpk[i] < Act.W[i]) Act.Wpk[i] = Act.W[i];
      }
  chgmin = 0;
}



// ########################################################################
// ########################################################################
// ########################################################################

void setup(void) 
{
  char n;
  pinMode(LED_BUILTIN, OUTPUT);                       // Initialize the LED_BUILTIN pin as an output
  Serial.begin(115200);   Serial.println();           // Init Serial Port
  // Restore SRam Data
  /*
  if (!SRam.Chk())   {SRam.Init(true); Def(); Serial.println("Reset Ram");}    
   else               SRam.RdBuf(0,sizeof(Set),(byte*) & Set);
    */
  // WiFi
  if ((n = Web.Init(80)) < 0) Serial.printf("Error: %hhd\n",n);
  Serial.printf("%s (%s) ->@ %s [%s]\n",Web.Ssid,Web.Pwd,Web.Host,Web.Ip);
    // Station Wifi
//  Web.Init("Ssid_Casa", "12345678", "pwrmon", IPAddress(192,168,10,1), 80);
  Tick.attach_ms(1000,Toggle);

}



// ########################################################################

word n,n1,c;
char buf[128];


void loop(void) 
{
 Web.Cyclic(); 
 delay(5);

 // There is something on rx buffer, echo n UDP
 if ((c = Serial.available()) > 0)
   {
    // Get data
    Serial.readBytes(buf, c);
    if (buf[0] == 'd') Act.Day++;
    if (buf[0] == 'h') Act.Hour++;
    if (buf[0] == 'm') Act.Sec = 60;
    if (buf[0] == '+') {Act.Temp[0]+=5;Act.Temp[1]+=5;Act.Temp[2]+=5;Act.Temp[3]+=5;Act.Temp[4]+=5;Act.Temp[5]+=5;}
    if (buf[0] == '-') {Act.Temp[0]-=5;Act.Temp[1]-=5;Act.Temp[2]-=5;Act.Temp[3]-=5;Act.Temp[4]-=5;Act.Temp[5]-=5;}
    if (buf[0] == 'w') for (short i = 0; i < NPWR; i++) Act.Wpk[i] = 0;
    //if (buf[0] == 'M') SRam.Init(0);
   }     

}


