#include <arduino.h>                          // default definitions
 
#define   NPWR        2                       // Power Measure Zones   
#define   dword       uint32_t                  
#define   qword       uint64_t                

// commands defines  
#define CMD_SAVE_FLASH  0x53   
#define CMD_CALIB_FREQ  0x76 
#define CMD_CALIB_GAIN  0x5a
#define CMD_CALIB_RGAIN 0x7a
#define CMD_BULK_ERASE  0x4f
 

 typedef struct {
                 word  Stat,Ver;
                 word  V,Vmin,Vmax;
                 word  Hz;
                 short Pf[2];
                 word  A[2];
                 word  W[2],Wmax[2];
                 word  Var[2];
                } TVaw;

typedef struct {
                dword Pi[2];
                dword Po[2];
               } TWh;

typedef struct { 
                TVaw  Vaw;                                  // Volt-ampere-watts
                TWh   Wh;                                   // Energy 
                word  Comms,NoResp,ChkErr,Nack;             // Statistics  
                byte  Device,Valid;                         // Flags 
                byte  Dbg[34];                              // Debug buffer (+2 for alignment)
               } TMcp39F511Data;

class TMcp39F511   
{
 private:
    #define CALOFSA1        0       
    #define CALOFSA2        1 
    #define CALOFSP1        2
    #define CALOFSP2        3
    #define CALOFSQ1        4
    #define CALOFSQ2        5
    #define CALOFSPH1       6
    #define CALOFSPH2       7

    #define CALGAINV        0
    #define CALGAINHZ       1
    #define CALGAINA1       2
    #define CALGAINA2       3
    #define CALGAINP1       4
    #define CALGAINP2       5
    #define CALGAINQ1       6  
    #define CALGAINQ2       7
    
    #define RNGV            0
    #define RNGA1           1
    #define RNGA2           2
    #define RNGP1           3
    #define RNGP2           4
    
        

    bool    Detect(void);
    char    RdReg     (byte addr, byte bytes, void *rx);
    char    WrReg     (byte addr, byte bytes, dword value);
    bool    SetOfs    (char wich, byte ch,    int   val,  char* txt);
    bool    SetRange  (char wich, byte ch,    char  op,   char* txt);
    bool    RstEnergy (byte ch,   char* txt);
    
    bool    Calibrate (char wich, byte ch,    float req,  char *txt);
    bool    WrCmd     (byte cmd);
    bool    RdVaw     (void);
    bool    RdWh      (void);

    byte    CalOfs[8],CalGain[8],Range[5];                     // address where located (device dependent)
    byte    CfgReg;                                            // config register
    word    Nwh;                                               // number of ticks to read energy instead power

 public:
            TMcp39F511(TMcp39F511Data * data, word nwh);      // address of data buffer,number of poll to read energy
    bool    Poll      (void);                                 // call cyclically (false = error)
    bool    Parse     (char *line);                           // simple command line parser 

    TMcp39F511Data   *Data;                                   // reference to external data block


};
