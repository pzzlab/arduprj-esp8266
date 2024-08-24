#include <Wire.h>
#include <ESP8266WiFi.h>

#ifndef dword
  #define dword unsigned int
#endif


// ============================== 24LCxxx EEPROM ===================================
// Example of use: 
/*    
T24LCxxx     Eeprom;
void setup(void) 
{Eeprom.Begin(true,true);}

void loop(void) 
{
 byte b; word a,w; dword dw;
 if (!Eeprom.Write(a,b)).. // error
 
}
*/

class T24LCxxx
{ 
 
private:
    void  WtRdy(void);
          byte exist,devaddr,pgsz;
          word pgs;
          dword lwr;                    // last write time (us)


 public:
    bool  Begin   (unsigned short kbit, bool fast = false, byte devaddr = 0x50, byte sda_pin = 0, byte scl_pin = 2);
    byte  GetPgSz (void ){return(pgsz);}
    bool  PgErase (word page);
    bool  Erase   (void);
    word  Rd      (word addr, byte  len, void* buf); 
    bool  Wr      (word addr, byte  len, void* buf, bool verify = false); 
    bool  Wr      (word addr, byte  val, bool verify = false); 
    bool  Wr      (word addr, word  val, bool verify = false); 
    bool  Wr      (word addr, dword val, bool verify = false); 
};

// ---------------------------------------------------------------------------------------


// ============================== PCF8574 IO EXPANDER ===================================
// Example of two devices chained, one define all input and other all output
/*  
PCF8574     Pcf1;
PCF8574     Pcf2;
void setup(void) 
{Pcf1.Begin(true); Pcf2.Begin(true,0x21);.. Pcf1.PinsCfg(0x00, 0x00);  Pcf2.PinsCfg(0xff, 0x00); ..}

byte n;
bool r[4];
void loop(void) 
{..delay(50); for (byte a = 0; a < 4; a++) r[a] = Pcf1.Pin(a); Pcf2.Wr((n++) << 4); for (byte a = 0; a < 4; a++) Pcf2.Pin(a,r[a]);..}
*/



class TPcf8574
{
 private:
          byte exist,devaddr,rdaddr,wraddr;
          byte inmsk,img;

 public:
    bool  Begin(bool fast = false, byte devaddr = 0x20, byte sda_pin = 0, byte scl_pin = 2);
    void  PinsCfg(byte outputmask, byte initialstate);
    byte  Rd(void);                         // 150.. 600us
    void  Wr(byte value);                   // 150.. 600us
    bool  Pin(byte num);                    // 150.. 600us
    void  Pin(byte num, bool state);        // 150.. 600us
};

// ---------------------------------------------------------------------------------------



// ========================= AHT21 Temp. and Humidity Sensor ============================
// **** Locking function for 80ms, do not call at hight rate (more than 1") ****
// Example:
/*  
AHT21       Aht;
void setup(void) 
{Aht.Begin(true);..}

void loop(void) 
{..delay(500); Aht.Read()..}
*/
class TAht21
{
 private:
  byte  exist,devaddr,state;

 public:
  bool  Begin(bool fast = false, byte devaddr = 0x38, byte sda_pin = 0, byte scl_pin = 2);
  void  Poll(void);             // must be called cyclically ( > 1 ms)
  byte  Read(void);             // call to start, return last state

  short Temp,Hum;               // Public data
};
// ---------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------


// ========================= ADS1115 4x16bit ADC ============================
// Example:
/*  
AHT21       Aht;
void setup(void) 
{Aht.Begin(true);..}

void loop(void) 
{..delay(500); Aht.Read()..}
*/
class TAds1115
{
 private:
  void  WrReg(byte num, word value);
  short RdReg(byte num);

  byte  exist,devaddr;
  word  cfg;

 public:
  bool  Begin(bool fast = false, byte devaddr = 0x48, byte sda_pin = 0, byte scl_pin = 2);
  bool  Read(byte chan);          // ~2ms

  short MilliV[4];                // reading data
};
// ---------------------------------------------------------------------------------------

