#include "SRam.h"

TSRam::TSRam (unsigned clockfreq, word kb, byte cspin)
// ---------------------------------------------
// Constructor,initialize the SPI and CS pin
// If an invalid value , Freq will be zero
// ---------------------------------------------
{
  byte clk = clockfreq / 1000000;       // reduce to megahertz 
  Size = 0; Freq = 0;                   // default size is 512KB invalid value (also flag ok)
  if ((kb != 512) && (kb != 1024)) {Serial.println("Invalid Data Size (512/1024)"); return;}
  if ((clk != 1) && (clk != 2) && (clk != 4) && (clk != 8)) {Serial.println("Invalid clock value (1(2,3,8)000000)"); return;}
  if (kb == 1024) Size = 1;
  Freq  = clk; CsPin = cspin;
  pinMode(CsPin, OUTPUT); digitalWrite(CsPin, true); 
  SPI.begin(); SPI.beginTransaction(SPISettings(clk * 1000000,MSBFIRST,SPI_MODE0));
}

void TSRam::RdBuf(int addr, word len, byte *data)
// ---------------------------------------------
// Read From Addr for Len bytes and put on data
// NOTE: max 64KB and chk buffer
// ---------------------------------------------
{
  if (!Freq) return;                                                // Invalid Handler
  digitalWrite(CsPin,false);
  SPI.transfer(0x03);       if (Size) SPI.transfer(addr >> 16);     // Cmd + addr[23..16]
  SPI.transfer(addr >> 8);  SPI.transfer(addr);                     // addr[15..0]
  while (len--)             *data++ = SPI.transfer(0x00);           // read loop
  digitalWrite(CsPin,true);
}

void TSRam::WrBuf(int addr, word len, byte *data)
// ---------------------------------------------
// Write From Addr for Len bytes from data
// NOTE: max 64KB and chk buffer
// ---------------------------------------------
{
  if (!Freq) return;                                                // Invalid Handler
  digitalWrite(CsPin,false);
  SPI.transfer(0x02);       if (Size) SPI.transfer(addr >> 16);     // Cmd + addr[23..16]
  SPI.transfer(addr >> 8);  SPI.transfer(addr);                     // addr[15..0]
  while (len--)             SPI.transfer(*data++);                  // write loop
  digitalWrite(CsPin,true);
}

void TSRam::Fill(int addr, int len, byte data)
// ---------------------------------------------
// Fill From Addr for Len bytes of data
// ---------------------------------------------
{
  if (!Freq) return;                                                // Invalid Handler
  digitalWrite(CsPin,false);
  SPI.transfer(0x02);       if (Size) SPI.transfer(addr >> 16);     // Cmd + addr[23..16]
  SPI.transfer(addr >> 8);  SPI.transfer(addr);                     // addr[15..0]
  while (len--)             SPI.transfer(data);                     // write loop
  digitalWrite(CsPin,true);
}

void TSRam::Init(bool magic)
// ---------------------------------------------
// Initialize the entire SRam and if (chk)
// reserve the lat 32 bytes to magic buffer
// ---------------------------------------------
{ 
  unsigned len = 0x10000; if (Size) len = 0x20000;
  if (!Freq) return;                                                // Invalid Handler
  if (magic)                                                        // reserve last 32 butes to check buffer
    {
      byte buf[32]; for (byte i = 0; i < 32; i++)  buf[i] = i;      // fill check buffer at end of area
      WrBuf(len - 32, 32, buf);                                     // save on top area - 32
      len -= 32;                                                    // do not overwrite chk buffer
    }
  digitalWrite(CsPin,false);
  SPI.transfer(0x02);       if (Size) SPI.transfer(0);              // Cmd + addr[23..16]
  SPI.transfer(0);          SPI.transfer(0);                        // addr[15..0]
  while (len--)             SPI.transfer(0);                        // write loop
  digitalWrite(CsPin,true);
}

bool TSRam::Chk(void)
// ---------------------------------------------
// Check if the magic buffer is corrupted
// ---------------------------------------------
{
  if (!Freq) return(false);                                         // Invalid Handler
  byte buf[32];
  if (Size) RdBuf (0x20000 - 32,32,buf);                            // Get check buffer
    else    RdBuf (0x1000 - 32,32,buf);
  for (byte i = 0; i < 32; i++)  
    if (buf[i] != i)  return(false);
  return(true);
}
