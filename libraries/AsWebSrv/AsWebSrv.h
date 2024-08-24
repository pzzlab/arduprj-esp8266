// ********************************* Enviroment Settings **************************************
// Flash Size:    (FS:512KB OTA 246KB)
// MMU:           16KB Flash + 48KB IRAM + 2nd HEAP Shared
// ------ NOTE: DO NOT USE delay() function on Webserver and Ticker because cause exception ----
//        Use: the millis(); 
//        E.g.  (unsigned t = millis(); while((millis() - t) < delay);)






#ifndef _WebSrv_H 
	#define _WebSrv_H
 
#define dword uint32_t
 
#ifdef ESP32  
   #define LED_BUILTIN  13
   #define  Fs  SPIFFS
   #include <SPIFFS.h>
   #include <AsyncTCP.h>
  #else 
    #define LED_BUILTIN  2
    #include <LittleFS.h>
    #define  Fs  LittleFS
    #include <ESPAsyncTCP.h>
   #endif
#include <ESPAsyncWebSrv.h>

#define LED(st)         digitalWrite(LED_BUILTIN,!st);
#define CHKACCESS       0x71
 

 

// Public table wich define the visibility of vars via ReadVar()/WriteVar..
enum eType
          {
            Var     = 0,
            Array   = 1,
            Struct  = 2
          } ;
     
typedef struct 
              {
                byte      Code;
                char      Txt[33];                        // text buffer
              } TCmd;                                     // Command from client
     
typedef struct 
              { 
                char      Name[16];												// The name used to identify the var side html
                char      Fmt[8];													// The output formatted to string
                void*     Obj;			 											// The pointer to ESP program ram (var,array,structure)
                eType     Type;												    // Enum of type
                word      Attr;														// Bits 0..2(int,?,?,float,String..) Bit 6,7 = Rd/Wr Author.
                word      Len;	  												// The lenght of var (1,2,4) for numeric,n for string
              } TVar;  
 
typedef struct { 
                char    Usr[16];
                char    Pwd[16];      
               }TAuth; 

extern const TVar  Vars[];									// Must be into Main program (define visibility of server)
extern const byte  VarsCnt;									// Same as before (global vars count)
extern       byte *Kw;                      // pointer of Set.Kv
extern       byte  Verbose;                 // helper
extern       byte  Auth;
extern "C"   byte GetConfigValue(const char *key, byte maxnum, char *values[]);
  
   
class TWebSrv 
{
	private:
	static short	SearchVar				(const char* name);
	static void 	ReadVar			    (AsyncWebServerRequest *request, short idx);
	static void 	WriteVar		    (AsyncWebServerRequest *request, short idx);
	static void 	ExeCmd				  (AsyncWebServerRequest *request);
  static void 	Parse				    (AsyncWebServerRequest *request);
  static void 	Login           (AsyncWebServerRequest *request);
  static void 	Logout          (AsyncWebServerRequest *request);
 
 public:
                TWebSrv				  (void);
				 bool   Begin           (bool(*cmdcallback)(byte code,char *txt));
  const  char   Build[16]       = "V1.01";
  const  char   HostName[16]    = "mywebsrv";
  IPAddress     ApIP,StaIP;
};
 
	
#endif
	 