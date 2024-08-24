

#ifndef _SRAM
	#define _SRAM

// *******************************************
//     GPIO12   MISO  
//     GPIO13   MOSI  
//     GPIO14   SCK   
//     GPIO16   CS_RAM
//         
//    SPI.beginTransaction(SPISettings(8000000,MSBFIRST,SPI_MODE0));  1KB -> 3.5 ms
//		
// *******************************************
	
#include <SPI.h>
#include <arduino.h>
	
class TSRam
{
  private:
  byte      Size, Freq, CsPin;
  
  public:
          TSRam (unsigned clockfreq, word kb, byte cspin);
    void  Init  (bool magic);
    void  RdBuf (int addr, word len, byte *data);
    void  WrBuf (int addr, word len, byte *data);
    void  Fill  (int addr, int  len, byte withbyte);
    bool  Chk   (void);
};

	
#endif	