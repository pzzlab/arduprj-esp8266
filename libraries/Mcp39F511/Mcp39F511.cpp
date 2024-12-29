#include "Mcp39F511.h"
extern byte Verbose;

void FlushRx(void)
// utility to clear any rx buffer
{
 while(Serial.available()) Serial.read();
}

TMcp39F511::TMcp39F511(TMcp39F511Data * data, word cyclesofwh)
 // -------------------------------------
// Consrtuctor
// -------------------------------------
{
 Data = data; 
 Nwh = cyclesofwh;
};

// ---------------------------------------------------------------------------------------

char TMcp39F511::RdReg(byte addr,byte bytes, void *rx)
// -------------------------------------
// read n registers to Mcp39F511
// Input:  addr  = starting address
//         size  = number of bytes to read
//         rx    = where locate the values
// Return:  1 = ok,
//          0 = no response
//         -1 = odd address
//         -2 = nack from device
//         -3 = chksm error
//         -4 = unknown error
// -------------------------------------
{
 byte i,chk,n;
 char tx[] = {"\xa5\x08\x41\x00\x02\x4e\x02\x00"};            // set address,cmd read n bytes
 char buf[36];

 if (addr & 0x1) {Data->Nack++;        return(-1);}          // odd address
 if (bytes > 32) bytes = 32;                                 // limit to 32 bytes
 n = bytes + 3;                                              // expected response
 memset(buf,0,sizeof(buf));                                  // clean buffer
 if (n > 35) n = 35;                                         // limit rxbuf overflow
 tx[4] = addr; tx[6] = bytes;                                // set starting and length
 for (chk = i = 0; i < 8; i++) chk += tx[i]; 
 tx[7] = chk;                                                // checksum
 Serial.write(tx,8);                                         // write on device
 n = Serial.readBytes(buf, n + 1);                           // query response
 // valid response 
 if ((n >= 3) && (buf[0] == '\x06'))                         // ack, bytes match
    {
      for (n--, chk = i = 0; i < n; i++)  chk += buf[i];
      // calc new checksum
      if (chk == buf[n]) 
         {memcpy(rx, &buf[2],n - 2);    return(1);}          // validate OK
      Data->ChkErr++;                   return(-3);          // Invalid checksum
    }
else  
  {
   if (!n) {Data->NoResp++;             return(0);}           // no response
   if (buf[0] == '\x15') 
      {FlushRx(); Data->Nack++; return(-2);}           // Nack from device
  }
 return(-4);                                                  // unknown error
}

// ---------------------------------------------------------------------------------------

char  TMcp39F511::WrReg(byte addr, byte bytes, dword value)
// -------------------------------------
// write a register to Mcp39F511
// Input:  addr = starting address
//         size = 2,4 (bytes)
//         value = value to write
// Return:  1 = ok,
//          0 = no response
//         -1 = odd address
//         -2 = nack from device
//         -3 = chksm error
//         -4 = unknown error
// -------------------------------------
{
 byte i,chk;
 byte n = 1;
 byte rx[8];
 byte tx[] = {"\xa5\x08\x41\x00\x00\x4d\x02\x00\x00\x00\x00\x00"};
 
 if (addr & 0x1) {Data->Nack++;         return(-1);}          // odd address
 tx[1] = bytes + 8; tx[4] = addr; tx[6] = bytes;              // set size and address 
 memcpy(&tx[7],&value,4);                                     // copy data
 for (chk = i = 0; i < bytes + 8; i++) chk += tx[i];          // calc chksm
 tx[7 + bytes] = chk;                                         // put into tx buffer
 Serial.write(tx,8 + bytes);                                  // write on device
 n = Serial.readBytes(rx,2);
 if (n && (rx[0] == 0x06))              return(1);            // response ACK
  else      
  {
   if (!n) {Data->NoResp++;             return(0);}           // no response
   if (rx[0] == '\x15') 
      {FlushRx(); Data->Nack++; return(-2);}           // Nack from device
  }
  return(-4);                                                 // unknown error
}

// ---------------------------------------------------------------------------------------

bool    TMcp39F511::WrCmd(byte cmd)
// -------------------------------------
// write a command to Mcp39F511
// Return: true  = ok,
//         false = error or not response
// -------------------------------------
{
 byte n = 1;
 char rx[8],tx[8];

 switch (cmd)
        {
         case     CMD_BULK_ERASE:    // Bulk erase eeprom
                  strcpy(tx,"\xa5\x04\x4f\xf8"); n = 4;          break;
         case     CMD_SAVE_FLASH:    // Store on Flash
                  strcpy(tx,"\xa5\x04\x53\xfc"); n = 4;          break;
         case     CMD_CALIB_FREQ:    // Calibration frequency
                  strcpy(tx,"\xa5\x04\x76\x1f"); n = 4;          break;
        }

 Serial.write(tx,n); n = 1;                                   // write cmd
 unsigned t = millis();
 while((millis() - t) < 1000);                                // one seconf delay
 n = Serial.readBytes(rx,n+1);                                // query response
 if (n && (rx[0] == 0x06))              return(true);         // OK
  else
  {
   if (!n) Data->NoResp++;                                    // no response
   if (rx[0] == '\x15') 
      {FlushRx(); Data->Nack++; return(-2);}           // Nack from device
   return(false);                                             // no response or nack
  }
}
// ---------------------------------------------------------------------------------------

bool TMcp39F511::RstEnergy(byte ch,char* txt)
// ------------------------------------------------
// reset energy counter for channel ch)
// return: true = ok false = error (on txt reason)
// ------------------------------------------------
{
  unsigned v;
  char     r;
  // check for a valid channel
  if ((ch > 1) &&(Data->Device != 'N')) {strcpy(txt,"Invalid Channel");       return(false);}
  // read actual config value, reset counter git then writeback,delay, set newly then rewriteback
  if ((r = RdReg(CfgReg,4,&v)) != 1)    {sprintf(txt,"Rd CfgReg Err %hhd",r); return(false);}
  if (Data->Device == 'N')              {if (ch == 2) v &= ~0x200; else v &= ~0x100;}
   else                                 v &= ~0x1;
  if ((r = WrReg(CfgReg,4,v)) != 1)     {sprintf(txt,"Wr CfgReg Err %hhd",r); return(false);}
  unsigned t = millis();               
  while((millis() - t) < 100);                                  // 100 ms delay
  if (Data->Device == 'N')              {if (ch == 2) v |= 0x200; else v |= 0x100;}
   else                                 v |= 0x1;
  if ((r = WrReg(CfgReg,4,v)) != 1)     {sprintf(txt,"Wr CfgReg Err %hhd",r); return(false);}
  Data->Wh.Pi[ch] = 0; Data->Wh.Po[ch] = 0;
 return(true);  
}

// ---------------------------------------------------------------------------------------

bool TMcp39F511::SetOfs(char wich, byte ch, int val, char* txt)
// ------------------------------------------------
// Save a zero load offset
// return: true = ok false = error (on txt reason)
// ------------------------------------------------
{
  byte addr = 0, l, r;
  // check for a valid channel
  if ((ch > 1) &&(Data->Device != 'N')) {strcpy(txt,"Invalid Channel");   return(false);}
  switch (wich)
        {
          case 'a':  if (ch == 2) addr = CalOfs[CALOFSA2];  else addr = CalOfs[CALOFSA1];  l = 4; break;
          case 'p':  if (ch == 2) addr = CalOfs[CALOFSP2];  else addr = CalOfs[CALOFSP1];  l = 4; break;
          case 'q':  if (ch == 2) addr = CalOfs[CALOFSQ2];  else addr = CalOfs[CALOFSQ1];  l = 4; break;
        }
 if ((r = WrReg(addr,l,val)) != 1) {sprintf(txt,"Wr OfsReg Err %hhd",r);  return(false);}
 return(true);  
}

// ---------------------------------------------------------------------------------------

bool TMcp39F511::SetRange(char wich, byte ch, char op, char* txt)
// ------------------------------------------------
// Save a new range 
// return: true = ok false = error (on txt reason)
// ------------------------------------------------
{
  byte addr = 0, v, r;
  // check for a valid channel
  if ((ch > 1) &&(Data->Device != 'N')) {strcpy(txt,"Invalid Channel");       return(false);}
  if ((op != '>') && (op !='<'))        {strcpy(txt,"only < or >");           return(false);}
  switch (wich)
        {
          case 'v':  addr = Range[RNGV]; break;
          case 'a':  if (ch == 2) addr = Range[RNGA2]; else addr = Range[RNGA1]; break;
          case 'p':  if (ch == 2) addr = Range[RNGP2]; else addr = Range[RNGP1]; break;
        }
  // read actual value, inc/dec then writeback
  if ((r = RdReg(addr,1,&v)) != 1)     {sprintf(txt,"Rd Rng Reg Err %hhd",r); return(false);}
  if ((op =='>') && (v < 31)) v++; else if ((op =='<') && (v > 1)) v--;
  if ((r = WrReg(addr,1,v) != 1))      {sprintf(txt,"Wr Rng Reg Err %hhd",r); return(false);}
 return(true);  
}

// ---------------------------------------------------------------------------------------

bool TMcp39F511::Calibrate(char wich, byte ch, float req,char* txt)
// ------------------------------------------------
// read, calc and store adc gain
// return: true = ok false = error (on txt reason)
// ------------------------------------------------
{
  byte addr; int act = 0,set,val,r;
  ch &=0x3; ch--;   // maybe 0 or 1
  // check for a valid channel
  if (ch &&(Data->Device != 'N')) {strcpy(txt,"Invalid Channel");                                             
                                                              return(false);}
  switch (wich)
        {
          case 'f': // set start autocalibration frequency 
                    if ((r = RdReg(CalGain[CALGAINHZ],2,&act)) != 1)  
                       {sprintf(txt,"GetF Err %hhd",r);       return(false);}
                    set = act * (req * 1000) / Data->Vaw.Hz;
                    if ((set < 25000) || (set > 65530))               
                       {sprintf(txt,"Invalid Gain (%d)",set); return(false);}
                    if ((r = WrReg(CalGain[CALGAINHZ],2,set)) != 1)   
                       {sprintf(txt,"SetF Err %hhd",r);       return(false);}
                    break;
          case 'v': // read,calculate and write new gain for frequency 
                    if ((r = RdReg(CalGain[CALGAINV],2,&act)) != 1)   
                       {sprintf(txt,"GetV Err %hhd",r);       return(false);}
                    set = act * (req * 10) / Data->Vaw.V;
                    if ((set < 25000) || (set > 65530))               
                       {sprintf(txt,"Invalid Gain (%d)",set); return(false);}
                    if ((r = WrReg(CalGain[CALGAINV],2,set)) != 1)    
                       {sprintf(txt,"SetV Err %hhd",r);       return(false);}
                    break;
          case 'a': // current calibration 
                    if (!ch)  addr = CalGain[CALGAINA1]; 
                     else     addr = CalGain[CALGAINA2]; 
                    val = Data->Vaw.A[ch];     
                    if ((r = RdReg(addr,2,&act)) != 1)
                       {sprintf(txt,"GetA Err %hhd",r);       return(false);}
                    set = act * (req * 1000) / val; 
                    if ((set < 25000) || (set > 65530))
                       {sprintf(txt,"Invalid Gain (%d)",set); return(false);}
                    if ((r = WrReg(addr,2,set)) != 1)
                       {sprintf(txt,"SetA Err %hhd",r);       return(false);}
                    break;
          case 'p': // power calibration 
                    if (!ch)  addr = CalGain[CALGAINP1]; 
                     else     addr = CalGain[CALGAINP2]; 
                    val = Data->Vaw.W[ch];
                    if ((r = RdReg(addr,2,&act)) != 1)
                       {sprintf(txt,"GetP Err %hhd",r);       return(false);}
                    set = act * (req * 100) / val;
                    if ((set < 25000) || (set > 65530))
                       {sprintf(txt,"Invalid Gain (%d)",set); return(false);}
                    if ((r = WrReg(addr,2,set)) != 1)
                       {sprintf(txt,"SetP Err %hhd",r);       return(false);}
                    break;
          case 'q': // reactive power calibration 
                    if (!ch)  addr = CalGain[CALGAINQ1]; 
                     else     addr = CalGain[CALGAINQ2]; 
                    val = Data->Vaw.Var[ch];
                    if ((r = RdReg(addr,2,&act)) != 1)
                       {sprintf(txt,"GetQ Err %hhd",r);       return(false);}
                    set = act * (req * 100) / val;
                    if ((set < 25000) || (set > 65530))
                       {sprintf(txt,"Invalid Gain (%d)",set); return(false);}
                    if ((r = WrReg(addr,2,set)) != 1)
                       {sprintf(txt,"SetQ Err %hhd",r);       return(false);}
                    break;
          default:  // any other case
                    strcpy(txt,"Unknown Cal. Arg");           return(false);
                    break;
        }
  return(true);
}  

// ---------------------------------------------------------------------------------------

bool  TMcp39F511::Detect(void)
// -----------------------------
// return the type of device
// 0 = no response or invalid
// 
// 
// -----------------------------
{
 byte buf[8];
 int n = sizeof(buf);
 char tx[] = {"\x5a\x00"};                                    // set address,cmd read n bytes
 memset(buf,0,sizeof(buf));
 Serial.write(tx,1);                                          // write on device
 n = Serial.readBytes(buf,sizeof(buf));                       // query response
 
 if ((n == 2) && (buf[0] == '\x15'))                          // correct response
    {
     if (buf[1] == '\x3') buf[1] = 'N';
      else
       if (buf[1] == '\x4') buf[1] = 'A';
    }
  else 
      {
       if (!n) Data->NoResp++;                                // no response
   if (buf[0] == '\x15') 
      {FlushRx(); Data->Nack++; return(-2);}           // Nack from device
       return(false);                                         // no response or nack
      }
  
  
 if (buf[1] == 'A') 
    { 
     CalOfs[CALOFSA1]   = 0x5a;   CalOfs[CALOFSP1]   = 0x5c;    CalOfs[CALOFSQ1]   = 0x5e;    CalOfs[CALOFSPH1]  = 0x62;
     CalGain[CALGAINV]  = 0x52;   CalGain[CALGAINHZ] = 0x60;    CalGain[CALGAINA1] = 0x50;    CalGain[CALGAINP1] = 0x54;
     CalGain[CALGAINQ1] = 0x56;   CfgReg             = 0x94;    Range[RNGV]        = 0x9c;    Range[RNGA1]       = 0x9d;
     Range[RNGP1]       = 0x9e;   Data->Device       = 'A';
    } 
  else                
     {
      CalOfs[CALOFSA1]   = 0x80;  CalOfs[CALOFSA2]   = 0x84;    CalOfs[CALOFSP1]   = 0x88;    CalOfs[CALOFSP2]   = 0x8c; 
      CalOfs[CALOFSQ1]   = 0x90;  CalOfs[CALOFSQ2]   = 0x94;    CalOfs[CALOFSPH1]  = 0x98;    CalOfs[CALOFSPH2]  = 0x9a;
      CalGain[CALGAINV]  = 0x74;  CalGain[CALGAINHZ] = 0x7e;    CalGain[CALGAINA1] = 0x70;    CalGain[CALGAINA2] = 0x72; 
      CalGain[CALGAINP1] = 0x76;  CalGain[CALGAINP2] = 0x78;    CalGain[CALGAINQ1] = 0x7a;    CalGain[CALGAINQ2] = 0x7c;
      CfgReg             = 0xa0;  Range[RNGV]        = 0xae;    Range[RNGA1]       = 0xaf;    Range[RNGA2]       = 0xbf;
      Range[RNGP1]       = 0xb0;  Range[RNGP2]       = 0xc0;    Data->Device        = 'N';
     } 
 return(true);
}

// ---------------------------------------------------------------------------------------

bool TMcp39F511::RdVaw(void)
{
 byte buf[35];
  if ((Data->Device == 'A') && (RdReg(0x02, 20, buf)) == 1)
     {
      Data->Vaw.Stat   =  *(word*)  &buf[0];          Data->Vaw.V      =  *(word*)  &buf[4];
      Data->Vaw.Hz     =  *(word*)  &buf[6];          Data->Vaw.Pf[0]  =  *( word*) &buf[10];
      Data->Vaw.A[0]   = (*(dword*) &buf[12]) / 10;   Data->Vaw.W[0]   =  *(dword*) &buf[16];
        // clip cosfi under a certain value because unstable
      if (Data->Vaw.A[0] < 5)                         Data->Vaw.W[0] = Data->Vaw.Pf[0] = 0;
        // set limits of Power and Voltage
      if (Data->Vaw.V < Data->Vaw.Vmin)               Data->Vaw.Vmin   = Data->Vaw.V;
       else
        if (Data->Vaw.V > Data->Vaw.Vmax)             Data->Vaw.Vmax   = Data->Vaw.V;
      if (Data->Vaw.W[0] > Data->Vaw.Wmax[0])         Data->Vaw.Wmax[0] = Data->Vaw.W[0];
      return(true);
     } 
   else
    if ((Data->Device == 'N') && (RdReg(0x02, 28, buf)) == 1)
       {
        Data->Vaw.Stat   =  *(word*)  &buf[0];        Data->Vaw.V       =  *(word*)  &buf[4];
        Data->Vaw.Hz     =  *(word*)  &buf[6];        Data->Vaw.Pf[0]   =  *( word*) &buf[8];
        Data->Vaw.Pf[1]  =  *( word*) &buf[10];       Data->Vaw.A[0]    = (*(dword*) &buf[12]) / 10;
        Data->Vaw.A[1]   = (*(dword*) &buf[16]) / 10; Data->Vaw.W[0]    =  *(dword*) &buf[20];
        Data->Vaw.W[1]   =  *(dword*) &buf[24];       
        // clip cosfi under a certain value because unstable
        if (Data->Vaw.A[0] < 5)                       Data->Vaw.W[0]    = Data->Vaw.Pf[0] = 0;
        if (Data->Vaw.A[1] < 5)                       Data->Vaw.W[1]    = Data->Vaw.Pf[1] = 0;
        // set limits of Power and Voltage
        if (Data->Vaw.V < Data->Vaw.Vmin)             Data->Vaw.Vmin    = Data->Vaw.V;
         else
          if (Data->Vaw.V > Data->Vaw.Vmax)           Data->Vaw.Vmax    = Data->Vaw.V;
        if (Data->Vaw.W[0] > Data->Vaw.Wmax[0])       Data->Vaw.Wmax[0] = Data->Vaw.W[0];
        if (Data->Vaw.W[1] > Data->Vaw.Wmax[1])       Data->Vaw.Wmax[1] = Data->Vaw.W[1];
        return(true);
       } 
 return(false);
}

// ---------------------------------------------------------------------------------------

bool TMcp39F511::RdWh(void)
{
 byte buf[36];
  if ((Data->Device == 'A') && (RdReg(0x1e, 16, buf)) == 1)
     {
      Data->Wh.Pi[0]  = *(dword*) &buf[0];
      Data->Wh.Po[0]  = *(dword*) &buf[8];
        return(true);
     } 
   else
    if ((Data->Device == 'N') && (RdReg(0x2e, 32, buf)) == 1)
       {
        Data->Wh.Pi[0]  = *(dword*) &buf[0];      Data->Wh.Pi[1]  = *(dword*) &buf[8];
        Data->Wh.Po[0]  = *(dword*) &buf[16];     Data->Wh.Po[1]  = *(dword*) &buf[24];
        return(true);
       } 
 return(false);
}

// ---------------------------------------------------------------------------------------

bool TMcp39F511::Poll(void)
{
 static word npoll;
 Data->Comms++;    
 Data->Valid = false;

 // if undefined, get the device type
 if ((Data->Device != 'A') && (Data->Device != 'N')) 
    {return(Detect());}                         
 // read actual Volt,ampere and watts (1 energy and N power)
 if (npoll++ < Nwh) 
    {
     if (!RdVaw())      return(false);
    }
  else
   if (npoll >= Nwh)  
      {
       if (!RdWh())     return(false);
       npoll = 0;
      }
 Data->Valid = true;
 return(true);
}

// ---------------------------------------------------------------------------------------

bool TMcp39F511::Parse(char *line)
// ------------------------------------------------
// Parse a command line
// return:  true  = ok 
//          false = error (on line the reason)
/*
0xaa(b)=nn   -> assign at address [aa] the value nn (b=1:16bit b=2:32bit)
0xaa(b)?     -> read content (bit size) at address [aa] 
ofs[apq][ch]=nn  -> set the adc offsets at NO LOAD a,p,q for channelch
cal[fvapq][ch]=nn  -> start calibration value for f,v,a,p,q
store        -> store the data on flash
** note: [ch] means 1 or 2 (no square braket: e.g. ofsa1=1234)
*/
// ------------------------------------------------
{
  int val; float fval;
  byte a,ch,l,r;
  
 if (!strcmp((const char*)line,"store"))        // store on flash
    {
     if (!WrCmd(CMD_SAVE_FLASH))    {strcpy(line,"Store Failed");         return(false);}
      else                           strcpy(line,"Stored ");
    } 
  else
   if (!strcmp((const char*)line,"erase"))     // bulk erase eeprom
      {
       if (!WrCmd(CMD_BULK_ERASE))    {strcpy(line,"Eeprom Erase Failed"); return(false);}
        else                           strcpy(line,"Eeprom Erased ");
      }  
    else                                        // reset energy counters
     if ((sscanf((const char*)line,"zerop%hhu",&ch) == 1) && ch && (ch <= 2))
      {
      if (RstEnergy(ch,line))         {strcpy(line,"Energy Zeroed");      return(true);}
      }   
      else                                      // write register 16
       if ((sscanf((const char*)line,"%hhx=%d",&a,&val) == 2))
          {
           if ((r = WrReg(a,2,val)) == 1)                                 return(true); 
            else                                sprintf(line,"Wr Reg16 Err %hhd",r);
          } 
        else                                    // read register 32
         if ((sscanf((const char*)line,"%hhx.2=%d",&a,&val) == 2))
            {
             if ((r = WrReg(a,4,val)) == 1)                               return(true); 
              else                              sprintf(line,"Wr Reg32 Err %hhd",r);
            } 
          else                                    // read register 16
           if ((sscanf((const char*)line,"%hhx%c",&a,&ch) == 2) && (ch == '?'))
              {
              if ((r = RdReg(a,2,&val)) == 1)   sprintf((char*)line,"R[0x%hhx](16): 0x%04hx ..... %hd",a,val,val);
                else                            sprintf(line,"Rd Reg16 Err %hhd",r);
              } 
            else                                  // assign offset
             if ((sscanf((const char*)line,"%hhx.2%c",&a,&ch) == 2) && (ch == '?'))
                {
                if ((r = RdReg(a,4,&val)) == 1) sprintf((char*)line,"R[0x%hhx](32): 0x%08x . %d",a,val,val);
                  else                          sprintf(line,"Wr Reg32 Err %hhd",r);
                } 
              else                                  // assign offset
               if ((sscanf((const char*)line,"ofs%c%hhu=%d",&a,&ch,&val) == 3) && strchr("apq@",a) && ch && (ch <=2))
                  return(SetOfs(a,ch,val,line));
                else                                // calibrate F or V
                 if ((sscanf((const char*)line,"cal%c=%f",&a,&fval) == 2) && strchr("vf",a))
                    return(Calibrate(a,1,fval,line));
                  else                              // calibrate A,P,Q
                   if ((sscanf((const char*)line,"cal%c%hhu=%f",&a,&ch,&fval) == 3) && strchr("apq",a) && ch && (ch <=2))
                      return(Calibrate(a,ch,fval,line));
                    else                            // range V
                     if (sscanf((const char*)line,"rngv%c",&l) == 1)
                        return(SetRange('v',1,l,line));
                      else                          // range A,P
                       if ((sscanf((const char*)line,"rng%c%hhu%c",&a,&ch,&l) == 3) && strchr("ap",a) && ch && (ch <=2))
                          return(SetRange(a,ch,l,line));
                        else     
                         strcpy(line,"Unknown Syntax") ;
 return(false);
}
// ---------------------------------------------------------------------------------------
