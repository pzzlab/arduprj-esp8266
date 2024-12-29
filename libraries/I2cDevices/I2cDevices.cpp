#include <I2cDevices.h>

unsigned us;
// Ticker compatible delay() BLOCKING FUNCTION to use instead delay into ticker (else panic exc.)
 void Delay(word ms) {long t1 = millis(); while((millis() - t1) <= ms) yield();}



// ============================== EEPROM from 32 to 512kbit ===================================
// Example of use of a 24LC512 (64KB) to random access values
// >>>>> Locking functions (Erase is >= 6s; PgErase = 12ms)
/*  
T24LCxxx  Eeprom;

void setup(void) 
{Eeprom.Begin(512,true);....}

void loop(void) 
{
 // Read EEprom byte(word, dword, buffer) from address 
 Eeprom.Rd(addr,len,buf);
 // Write to EEprom byte(word, dword, buffer)  EEprom.Wr have an optional par. verify = false
 if (!Eeprom.Wr(addr,[word,dword[])) <error>    if (!Eeprom.Wr(addr,len,true)) <error>
}
*/

// ---------------------------------------------------------------------------------------

 void T24LCxxx::WtRdy(void)
// ------------------------------------------------
// check for eeprom RDY (no writing in progress)
// (testing for ACK from i2c device)
// ------------------------------------------------
{
 while ((micros() - lwr) <= 6000)                   // Timeout is 6ms
       {
        Wire.beginTransmission(devaddr);     
        if (!Wire.endTransmission()) return;        // ACK received, exit before timeout
        yield();                                    //  For OS scheduling
       }
}
// ---------------------------------------------------------------------------------------

 bool T24LCxxx::Begin(unsigned short kbit, bool fast, byte addr, byte sda_pin, byte scl_pin)
// -----------------------------------------------------
// default pin uses ESP01 pins @100(400)KHz,0x20,inputs.
// if needs different, use PinCfg(mask,initialstate)
// -----------------------------------------------------
{
  switch  (kbit)
    {
     case 32:  pgsz =  64; pgs =  64;  break;
     case 64:  pgsz =  32; pgs = 256;  break;
     case 128: pgsz =  64; pgs = 128;  break;
     case 256: pgsz =  64; pgs = 512;  break;
     case 512: pgsz = 128; pgs = 512;  break;
     default:  return(false);
    }
  devaddr = addr; 
  lwr     = micros();
  exist   = false;
  digitalWrite(LED_BUILTIN,false);        // force out level (else if true i2c do not function)
  Wire.begin(sda_pin,scl_pin);            // initialize pins
  if (fast) Wire.setClock(400000); 
   else     Wire.setClock(100000);
  // check for device on bus (trying  read from device)
  Wire.requestFrom(devaddr,(size_t)1,true);
  if (Wire.available()) {Wire.read(); exist = true;}
  return(exist);
}
// ---------------------------------------------------------------------------------------

 word  T24LCxxx::Rd (word addr, byte len, void* buf)
// ------------------------------------------------
// Read from addr for len bytes and put into buf[]
// return the number of bytes read
// ------------------------------------------------
{
 ;
 byte n = 0;
 WtRdy();
 Wire.beginTransmission(devaddr);
 Wire.write(addr >> 8);           
 Wire.write(addr & 0xff);           
 Wire.endTransmission();
 Wire.requestFrom(devaddr, len);
 if (Wire.available()) n = Wire.readBytes((char*)buf,len);
  else								 return(0);
 return (n);
}
// ---------------------------------------------------------------------------------------
 
 bool  T24LCxxx::Wr (word addr, byte len, void* buf, bool verify)
// ------------------------------------------------
// Write to addr for len bytes getting from buf[]
// return: true = OK false = FAIL
// ------------------------------------------------
{
 byte n;
 WtRdy();
 Wire.beginTransmission(devaddr);
 Wire.write(addr >> 8);           
 Wire.write(addr & 0xff);           
 Wire.write((char*)buf,len);
 Wire.endTransmission();
 lwr = micros();
 // verify if data read is equal  written
 if (verify)
    {
     WtRdy();
     Wire.beginTransmission(devaddr);
     Wire.write(addr >> 8);           
     Wire.write(addr & 0xff);           
     Wire.endTransmission();
     Wire.requestFrom(devaddr, len);
     byte *p = (byte*)buf;
     for (n = 0; n < len; n++)
       if (Wire.available()) if (Wire.read() != p[n]) return(false);
    }   
 return (true);
}
// ---------------------------------------------------------------------------------------

 bool  T24LCxxx::Wr (word addr, byte val, bool verify)
// ------------------------------------------------
// Overloaded of wr: Wite a 16 bit data
// return: true = OK false = FAIL
// ------------------------------------------------

{
  byte buf[8]; buf[0] = val;
  return(Wr(addr,1,(void*)buf,verify));
}
// ---------------------------------------------------------------------------------------

 bool  T24LCxxx::Wr (word addr, word val, bool verify)
// ------------------------------------------------
// Overloaded of wr: Wite a 16 bit data
// return: true = OK false = FAIL
// ------------------------------------------------
{
  byte buf[8]; *(word*) buf = val;
  return(Wr(addr,2,(void*)buf,verify));
}
// ---------------------------------------------------------------------------------------

bool  T24LCxxx::Wr (word addr, dword val, bool verify)
// ------------------------------------------------
// Overloaded of wr: Wite a 32 bit data
// return: true = OK false = FAIL
// ------------------------------------------------
{
  byte buf[8]; *(dword*) buf = val;
  return(Wr(addr,4,(void*)buf,verify));
}

// ---------------------------------------------------------------------------------------

 bool  T24LCxxx::PgErase (word page)
// ------------------------------------------------
// erase a specific eeprom page and fill with 0xff
// return: true = ok; false = error (12ms)
// ------------------------------------------------
{
 byte  buf[128];
 if (page >= pgs) page = pgs - 1;
 memset(buf,0xff, sizeof(buf));
 return(Wr (page * pgsz, pgsz, buf, true));
}
// ---------------------------------------------------------------------------------------

 bool  T24LCxxx::Erase (void)
// ------------------------------------------------
// erase the entire  eeprom and fill with 0xff
// return: true = ok; false = error  (6s for 512)
// ------------------------------------------------
{
 word n;
 for (n = 0; n < pgs; n++) 
    if (!PgErase(n)) return(false);
 return(true);
}
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


// ---------------------------------------------------------------------------------------


bool TPcf8574::Begin(bool fast, byte addr, byte sda_pin, byte scl_pin)
// ------------------------------------------------
// default pin uses ESP01 pins @100KHz,0x20,inputs.
// if needs different, use PinCfg(mask,initialstate)
// ------------------------------------------------
{
  devaddr = addr; wraddr = addr << 1; rdaddr = wraddr + 1; inmsk = 0xff; exist = 0;
  digitalWrite(LED_BUILTIN,false);        // force out level (else if true i2c do not function)
  Wire.begin(sda_pin,scl_pin); 
  if (fast) Wire.setClock(400000); 
   else     Wire.setClock(100000);
  Wire.beginTransmission(devaddr); Wire.write(inmsk); Wire.endTransmission();  
  // check for response from device
  Wire.requestFrom(devaddr,(size_t)1,true);
  if (Wire.available()) {Wire.read(); exist = true;}
  return(exist);
}
// ---------------------------------------------------------------------------------------

void TPcf8574::PinsCfg(byte outputmask, byte initialstate)
// ------------------------------------------------
// Configure the pins as output with initial state
// and reconfigure other pinsa as inputs (high)
// ------------------------------------------------
{
  if (!exist) return;
  inmsk = ~outputmask; 
  Wire.beginTransmission(devaddr); Wire.write(~initialstate | inmsk); Wire.endTransmission();
}
// ---------------------------------------------------------------------------------------
  
byte TPcf8574::Rd(void)
// ---------------------------------------------
// Return the inputs ignoring the configured output
// ---------------------------------------------
{
 if (!exist) return(0); 
 Wire.requestFrom(devaddr,(size_t)1,true);
 if (Wire.available()) return(~Wire.read() & inmsk);
  else                 return(0); 
}
// ---------------------------------------------------------------------------------------

void TPcf8574::Wr(byte data)
// ---------------------------------------------
// Write a byte (only on out, input unchanged)
// ---------------------------------------------
{if (!exist) return; img = data; Wire.beginTransmission(devaddr); Wire.write(~img | inmsk); Wire.endTransmission();}
// ---------------------------------------------------------------------------------------

void TPcf8574::Pin(byte num, bool state)
// ----------------------------------
// Write a pin number  value
// ----------------------------------
{
  if (!exist) return;
  if (state) img |= 1 << num;
   else      img &= ~(1 << num);
  Wr(img);
}
// ---------------------------------------------------------------------------------------

bool TPcf8574::Pin(byte num)
// ----------------------------------
// Return the level of a selected pin
// ----------------------------------
{if (!exist) return(0); return((Rd() & (1 << num)) != 0);}
// ---------------------------------------------------------------------------------------


// ========================= AHT21 Temp. and Humidity Sensor ============================
// **** Locking function for 80ms, do not call at hight rate (more than 1") ****
// Example:
/*  
AHT21       Aht;
void setup(void) 
{Aht.Begin(true);..}

void loop(void) 
{..delay(1000); Aht.Read()..}
*/
// ---------------------------------------------------------------------------------------

bool TAht21::Begin(bool fast, byte addr, byte sda_pin, byte scl_pin)
// ------------------------------------------------
// default pin uses ESP01 pins @100KHz,0x38
// ------------------------------------------------
{
  byte stat = 0;
  exist = false; state = 0; devaddr = addr;
  digitalWrite(LED_BUILTIN,false);        // force out level (else if true i2c do not function)
  Wire.begin(sda_pin,scl_pin); 
  if (fast) Wire.setClock(400000); 
   else     Wire.setClock(100000);
  Wire.beginTransmission(devaddr); Wire.write(0xbe); Wire.endTransmission();
  Delay(500);
  Wire.requestFrom(devaddr,(uint8_t) 1);
  if (!Wire.available()) return(false);
  stat = Wire.read();
  if ((stat & 0x18) == 0x18) {exist = true; return(true);}
  return(false);
}
// ---------------------------------------------------------------------------------------
byte TAht21::Read(void)
// -------------------------------------
// Start a new read data and return the 
// state ol last call (1 = ok, 2 = busy)
// Must call ::Poll at 100ms rate cyclically
// -------------------------------------
{
 if (state == 2)      return(2);                // conversion not yet done     
 if (state == 3)     {state = 1; return(1);}    // conversion finish
 state = 1;           return(2);                // start in any other case
}

void TAht21::Poll(void)
// ------------------------------------------------
// Read the sensor data (temp + umidity) and
// put on public variables Temp(0.1^C) + Hum(%)
// return: true if done, false if not
// Due  the slow device,(>80ms) the read will
// be segmented in 3 phases
//  make blocking, call until return != 2
// ------------------------------------------------
{
 if (!exist) return;
 const byte startcmd[] = {0xac,0x33,0x00};
 static byte tmt;
 // check for data (bit 7 of buf[0] mean conversion running)
 switch (state)
  {
    case  1:  // start acquisition
              Wire.beginTransmission(devaddr); 
              Wire.write(startcmd,3); 
              Wire.endTransmission(); 
              tmt = 0; state = 2;
              break;
    case  2:  // read buffer and check for conversion done              
              Wire.requestFrom(devaddr,(uint8_t) 6);
              byte buf[6] = {0,}, i = 0;
              while (Wire.available()) buf[i++] = Wire.read();
              if (buf[0] & 0x80) break;                         // Wait for convertion done
              if (!i || (tmt++ > 20)) state = 0;                // timeout,reset
              unsigned h,t;
              // build the read data 
              h = buf[1]; h <<= 8; h += buf[2]; h <<= 4; h += buf[3] >> 4;
              //Hum = h / 1048576.0;
              Hum = (h * 100) >> 20;
              t = buf[3] & 0x0f; t <<= 8; t += buf[4]; t <<= 8; t += buf[5];
              //Temp = t / 1048576.0 * 200.0 - 50.0;
              Temp = ((t  * 2000) >> 20) - 500;
              state = 3;                                        // Data Available
  }
}
// ---------------------------------------------------------------------------------------


// ========================= ADS1115 4x16bit ADC ============================
// Example:
/*  
TAht21       Aht;
void setup(void) 
{Aht.Begin(true);..}

void loop(void) 
{..delay(500); Aht.Read()..}
*/
// ---------------------------------------------------------------------------------------

bool TAds1115::Begin(bool fast, byte addr, byte sda_pin, byte scl_pin)
// ------------------------------------------------
// default pin uses ESP01 pins @100KHz,0x48
// ------------------------------------------------
{
  exist = false;
  devaddr = addr;
  digitalWrite(LED_BUILTIN,false);        // force out level (else if true i2c do not function)
  Wire.begin(sda_pin,scl_pin); 
  if (fast) Wire.setClock(400000); 
   else     Wire.setClock(100000);
  if (!RdReg(1)) return(false);        // read cfg register (0 = invalid or no device)
  // initialize one shot, 4.096V, 860smp, channel #0
  cfg = 0x03e0; WrReg(1, cfg);
  exist = true;
  return(exist);
}

// ---------------------------------------------------------------------------------------
void TAds1115::WrReg(byte num, word value)
// ------------------------------
// private: write on a register
// ------------------------------
{
 Wire.beginTransmission(devaddr);
 Wire.write(num & 0x3);           // address register
 Wire.write(value >> 8);          // HI 
 Wire.write(value & 0xff);        // LO
 Wire.endTransmission();
}
// ---------------------------------------------------------------------------------------

short TAds1115::RdReg(byte num)
// ------------------------------
// private: read from a register
// ------------------------------
{
 byte buf[2] = {0,0};
 Wire.beginTransmission(devaddr);
 Wire.write(num & 0x3);           // address register
 Wire.endTransmission();
 Wire.requestFrom(devaddr,(uint8_t)2);
 if (Wire.available()) Wire.readBytes(buf,2);
 return((buf[0] << 8) | buf[1]);
}
// ---------------------------------------------------------------------------------------

bool TAds1115::Read(byte chan)
// ------------------------------------------------
// Read the adc chan
// put on public variables MilliV[chan]
// return: true if done, false if not
// RESOURCES:   ~ 2ms
// ------------------------------------------------
{
 if (!exist) return(false);
 WrReg(1,cfg | 0xc000 | ((word)(chan & 0x3) << 12));  // select channel and start conversion
 Delay(2);
 // while(!(RdReg(1) & 0x8000)) delayMicroseconds(200);    // poll  conversion complete
 MilliV[chan & 0x3] = RdReg(0) >> 3;                       // get conversion result in mV
 return(true);
} 
// ---------------------------------------------------------------------------------------




