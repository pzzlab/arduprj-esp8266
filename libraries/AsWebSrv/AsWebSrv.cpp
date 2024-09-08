#include "AsWebSrv.h"
#include <ESP8266LLMNR.h>

AsyncWebServer  Srv(80);
TCmd            Cmd; 
bool           (*CmdCb)(byte,char*);
byte            Auth;
TAuth           Access[3];


byte GetConfigValue(const char *key, byte maxnum, char *values[])
// ---------------------------------------------
// open "setup.cfg" and search the relative
// "values[]"  of "key" (comma separated)
// "maxnum" set the max pointers allowed
// return: the number of args found for "key"
// E.G. Ap=param1 param2 param3 ....
// ---------------------------------------------
{
 String   s;
 char    *p;
 short    l = strlen(key), sp, ln = 0, n = 0;
 File     f = Fs.open("/setup.cfg", "r");
 if (Verbose & 0x1)Serial.printf(">>>> Setup.cfg: try to get %s (max %hhd arg)..",key,maxnum);
 if (maxnum) maxnum--;
 while (f && f.available())
    {
      // read every line until found 
      if ((s = f.readStringUntil('\n')) != NULL) 
         {
          ln++;
          // ignore the starting "//"
         if (s.startsWith("//")) continue;
          // key match and found '='
          if (s.startsWith(key) && (s[l] == '='))
             {
              // remove the "key="     
              s = s.substring(l+1);
              if (Verbose & 0x1)Serial.print("Found -> ");    
              // have one or more separators
              while (((sp = s.indexOf(' ')) != -1) && (n < maxnum))
                    {
                     if (sp > 15) sp = 15;  // limt to max 15 chars
                     strncpy(values[n], s.c_str(),sp); values[n][sp] = 0;
                     if (Verbose & 0x1)Serial.printf("arg[%hhd] %s , ",n,values[n]);
                     s = s.substring(sp+1); n++;            
                    }
              // remaining value,remove CR and stop
              if (p = strchr(s.c_str(), '\r')) *p = 0;
              if (Verbose & 0x1)Serial.printf("arg[%hhd] %s",n,s.c_str());
              strcpy(values[n++], s.c_str());  
              break;
             }
         }       
    }
 if (Verbose & 0x1) Serial.println();   
 f.close();
 return(n);
}


TWebSrv::TWebSrv(void)
// -------------------------------------
// 							Constructor
// -------------------------------------

{
 memset(Access,0,sizeof(Access));
}	
 

// ==========================================================================

void TWebSrv::Login(AsyncWebServerRequest *request)
// -------------------------------------
// handler to exec a login and redirect
// to a web page
// Ex: login?level&newpage
// call a login on browser (if not yet)
// and redirect to newpage (if not empty)
// also save the access on Auth global var
// -------------------------------------
 {
  const char *p1 = request->getParam(0)->name().c_str(),*p2;
  word lev;
  if (sscanf(p1,"%hhd",&lev))  lev = (lev-1) & 0x03; // max 4 levels
  if (request->params() > 1)p2 = request->getParam(1)->name().c_str();
//  Serial.printf("[%hhd] %s -> %s\n",lev,Access[lev].Usr,Access[lev].Pwd);
  if (!request->authenticate(Access[lev].Usr,Access[lev].Pwd))  return(request->requestAuthentication());
  Auth = lev + 1; // Auth is a Webserver variable
  if (*p2) request->redirect(p2);
 }	
 
void TWebSrv::Logout(AsyncWebServerRequest *request)
// -------------------------------------
// handler to exec a logout and redirect
// to a web page
// Ex: logout?&newpage
// and redirect to newpage (if not empty)
// also clear Auth global var
// -------------------------------------
 {
  const char *p1 = request->getParam(0)->name().c_str();
//  Serial.printf("%s\n",p1);
  Auth = 0;
  if (*p1) request->redirect(p1);
 }	

// ==========================================================================

bool TWebSrv::Begin(bool(*callback)(byte code, char *text))
// -------------------------------------
// Init WebServer: 
// Check filesystem then
// Start Srv and assign handlers
// RETURN:
//  	 1  = OK
//		 0  = Invalid FileSystem
// -------------------------------------
{ 
  byte  led = 0, retry = 250;
  char  apssid[33],stassid[33],stapwd[33];
  char* stapar[] = {stassid,stapwd}, *pt[2]; 
  AsyncStaticWebHandler *hnd = NULL;
  // Assign as default the Led pin and input0 as input (if uses i2c, will be reconfigured later)
  pinMode(LED_BUILTIN, OUTPUT);   pinMode(0, INPUT_PULLUP);
  Serial.println("");
#ifdef ESP32
  Fs.begin();// )    {Serial.println("\nInvalid SPIFFs"); return(false);} 
	if (!Fs.exists("/setup.cfg")) {Serial.println("\nsetup.cfg not found"); return(-2);}
#elif defined(ESP8266)
  if (!Fs.begin())  {Serial.println("\nInvalid Fs"); return(false);} 
	if (!Fs.exists("/setup.cfg")) {Serial.println("\nsetup.cfg not found"); return(-2);}
#endif
// read from setup.cfg Verbose and host name  (else uses the default"hostname")
  char tmp[16]; pt[0] = tmp; GetConfigValue("Verbose",1,pt);
  sscanf(tmp,"%hhd",&Verbose);
  // Display Verbose (as default is disable so no any msg displayed before)
  if (Verbose) Serial.printf("Verbose Key 0x%02hhx\n",Verbose);
  // Get Authentication Auth=usr pwd (max 3 levels)
  pt[0] = Access[0].Usr; pt[1] = Access[0].Pwd; GetConfigValue("Auth1",2,pt);
  pt[0] = Access[1].Usr; pt[1] = Access[1].Pwd; GetConfigValue("Auth2",2,pt);
  pt[0] = Access[2].Usr; pt[1] = Access[2].Pwd; GetConfigValue("Auth3",2,pt);
  // assign static files handling
  Srv.serveStatic("/",	    Fs,"/").setDefaultFile("index.html");
  // assign default Hostname if undefined
  pt[0] = (char*)HostName; 
  if (!GetConfigValue("HostName",1,pt)) Serial.printf("'HostName' not found will use %s\n",HostName);
  // ---- Start with WIFI connection ----
  if (Verbose & 0x2) Serial.println("WebServer Init"); 

// STA/AP mode 
//  [Sta=wps]       -> connect via WPS button)
//  [Sta=SSID PWD]  -> connect with SSID and PWD (separate with a space)
//  [Ap=SSID PWD]   -> start as SoftAp with SSID and PWD

  if (GetConfigValue("Sta",2,stapar))             // read Sta=ssid pwd (or Sta=wps)
     { 
      WiFi.mode(WIFI_STA);
      if (Verbose & 0x2) 
          {
           if (!strcmp(stassid,"wps")) Serial.printf("Try to Wps  Link %s .. ",WiFi.SSID().c_str());    
            else                       Serial.printf("Try to Link with SSID %s .. ",stassid);    
          }  
      // wps mode selected,init with previous access saved
      if (!strcmp(stassid,"wps")) WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());  // last saved name 
       else                       WiFi.begin(stassid, stapwd);  // from cfg file
      // durimg connection, blink the led fast
      while (--retry && WiFi.status() != WL_CONNECTED) {led = !led; LED(led); delay(100);}
      // Connection failed, try to pair (need to push WPS button on router )
      // During pairing led blink slow (every 20 secs) into infinite loop until paired
      // if paired, continue
      if (!retry) 
       {
        if (!strcmp(stassid,"wps"))
           {
            if (Verbose & 0x2) Serial.println("Failed, Push WPS button on AP");              
            while (!WiFi.beginWPSConfig() || !WiFi.SSID().length()) {LED(led); led = !led; Serial.print(".");}
            if (Verbose & 0x2) Serial.println("** PAIRED **");              
           }
         else if (Verbose & 0x2) Serial.printf("Failed\n");              
       }
       else        
        if (Verbose & 0x2) Serial.printf("Connected With %s (%s)\n",WiFi.SSID().c_str(), WiFi.psk().c_str());
     StaIP = WiFi.localIP();
     Serial.printf("<%s>\n",HostName);   
     }
     // AP Mode, register the SSID     
     pt[0] = apssid; 
     if (GetConfigValue("Ap",1,pt))  
        {
         LED(true);              if (Verbose & 0x2) Serial.printf("Starting AP:%s\n",apssid);
         if (!WiFi.softAP(apssid) && Verbose & 0x2) Serial.println("Failed");
         ApIP = WiFi.softAPIP();
         LED(false);
        }
  // Here come if the connection has successful
  if (strlen(HostName)) WiFi.setHostname(HostName);              // assume host name saved on file
  LLMNR.begin(HostName);
  LED(false);
/*
  // Login Handler (/login?filename -> force auth. and set flag) 
  Srv.on("/login", HTTP_GET, [](AsyncWebServerRequest *request)
    {
     if(!request->authenticate(Au,Ap))  return(request->requestAuthentication());
     Auth = true;
     request->redirect(request->getParam(0)->name().c_str());
    });  
*/    
  // Login Handler (/login?level&filename -> set Auth 1..3) 
  Srv.on("/login", HTTP_GET, Login);
  // Logout Handler (/logout?filename -> reset Auth) 
  Srv.on("/logout", HTTP_GET, Logout);
  // Any other (not files into FS)
  Srv.onNotFound(Parse);                        
  Srv.begin();                                          // Start Server
  CmdCb = callback;
	return(true);																																					
}




// ==========================================================================

short TWebSrv::SearchVar(const char* name)
// -----------------------------------------------
// Search a variable from table by name
// RETURN:  0..n = index of table, -1 if not found
// -----------------------------------------------
{
// Serial.printf("Searching Var %s\n",name);
  for (short i = 0; i < VarsCnt; i++)                                               // loop for any defined var
    {
//     Serial.printf("[%hd] %s|%s|\n",i,Vars[i].Name,name);
     if (!strcmp(Vars[i].Name,name)) return(i);                                      // return index
    }
 return(-1);                                                                        // not found
}

// ==========================================================================

void TWebSrv::Parse(AsyncWebServerRequest *request)
{
  char *c = (char*)request->url().c_str();
  
  if (!request->args()) {request->send(404, "text/plain", request->url() + " Not Found!"); return;}
  const char *a = request->getParam(0)->name().c_str();
  // FRead: read a file
  if (!strcmp(c,"/FRead")) {request->send(Fs, a); return;}
  // Special case: Execute a command (struct TCmd))
  if (!strcmp(c,"/Cmd"))      {ExeCmd(request); return;}  // Write a variable
  // Is variable, s earch on list of availebles
  short  n = SearchVar(a);
  // Not found , error
  if (n < 0)  
     {
      request->send(400, "text/plain"," Var: <" + request->getParam(0)->name() + "> Not Found"); 
      if (Verbose & 0x2) Serial.printf("Var %s Not Found\n",a);
      return;
     }
  // exe the read/write  of mapped vars
  if (!strcmp(c,"/Read"))         ReadVar(request,n);     // Read a variable
   else																				
    if (!strcmp(c,"/Write"))      WriteVar(request,n);    // Write a variable
}

// ==========================================================================


void TWebSrv::ReadVar(AsyncWebServerRequest *request, short idx)
// ----------------------------------------------- 
// Read the var value, build the value by attr,len 
// and format then put on out 
// -----------------------------------------------
{
 char out[32], *buf;
 if (idx > (VarsCnt)) 
    {sprintf(out,"Idx Overflow % hd\n",idx); request->send(200, "text/plain", out);return;} // here never happen
 byte   a = Vars[idx].Attr & 0xf; 
 word   l = Vars[idx].Len;                              
 void  *o = Vars[idx].Obj;
 
 // refuse if unhautorized
 if ((Vars[idx].Attr & 0x40) && !Auth)   {request->send(200, "text/plain","Unauthorized");return;} // reject access
 if (Verbose & 0x2) Serial.printf("ReadVar(%s)\n",Vars[idx].Name);
 
 if ((buf = new char[1024]) == NULL)                // Allocate Response Buffer, invalid, error
    {Serial.println("ParseReq:/Read: Cannot Allocate Response Buffer"); return;}
 buf[0] = 0;
// Serial.printf("Var %s, Fmt %s, Typ %hhd, Attr%hhd, Size%hd\n", Vars[idx].Name,Vars[idx].Fmt,Vars[idx].Type,a,l);
 switch (Vars[idx].Type)
        {
         case Var:    // is a single variable,build the out string by format and data 
                      if (a == 0)                                                                    // integer  
                       {
                         if (l == 1) sprintf(out,Vars[idx].Fmt,*(byte*)o);               // signed byte
                          else
                           if (l == 2) sprintf(out,Vars[idx].Fmt,*(short*)o);            // short
                            else
                             if (l == 4) sprintf(out,Vars[idx].Fmt,*(int*)o);            // int
                        }
                        else                                                                         
                         if ((a == 1) && (l == 4)) sprintf(out,Vars[idx].Fmt,*(float*)o);// float32
                          else                     
                           if (a == 7) strcpy(out,(char*)o);                             // String or char[]
                       request->send(200, "text/plain", buf);     
//                       Serial.println(buf);
                       break;
                         
         case Array:  // is an array build the out as string by values comma separated
                      if (a < 4)
                        {
                         for (word i = 0; i < l; i++)                                                  // Loop for any element
                           {
                            if (a == 0) sprintf(out,Vars[idx].Fmt,*((byte*)o + i));       	// signed byte
                             else
                              if (a == 1) sprintf(out,Vars[idx].Fmt,*((short*)o + i));     // short
                               else
                                if (a == 2) sprintf(out,Vars[idx].Fmt,*((int*)o + i));     // int
                                 else                                                                         
                                  if (a == 3) sprintf(out,Vars[idx].Fmt,*((float*)o + i)); // float32
                             // Concatenate and if not the last, add separator
                             strcat(buf,out); if (i < (l - 1)) strcat(buf, ";");
                           }      
                        }   
                       if (a == 7) 																																		// strings
                          {
                           buf[0] = 0; sscanf(Vars[idx].Fmt,"%hhu",&a);
                           for (word i = 0; i < a; i++)
                                {strcpy(out,(char*)o + i * l); strcat(buf, out); if (i < (a-1)) strcat(buf, ";");}
                          }
                       request->send(200, "text/plain", buf);          
                       break;

         case Struct:   // is a memory block,build the out as bytes buffer
//                        Serial.printf("Resp: %p  %hd\n",o,Vars[idx].Len);
                        AsyncWebServerResponse *response = request->beginResponse_P(200, "application/octet-stream", (const byte*)o,Vars[idx].Len);
                        request->send(response);
                        break;
        }               
    delete[] buf;  
}


// ============================= SERVER HANDLERS ================================

 void TWebSrv::WriteVar(AsyncWebServerRequest *request, short idx) 
// --------------------------------------
// Write a variable
// --------------------------------------
{
 float  f;
 int    v;
 short  a  = Vars[idx].Attr & 0xf, l = Vars[idx].Len;
 void  *o  = Vars[idx].Obj;
 char*  p  = (char*)request->getParam(0)->value().c_str(), i;
 short  ls = strlen(p); 
 
 // refuse if unhautorized
 if ((Vars[idx].Attr & 0x80) && !Auth)  {request->send(200, "text/plain","Unauthorized");return;} // reject access
 if (Verbose & 0x2) Serial.printf("WriteVar: %s (%s)\n",Vars[idx].Name, p);

 switch (Vars[idx].Type)
        {
         case Var:    // is a single variable,build the out string by format and data 
                      if (a == 0)        // integer
                          {
                           sscanf(p,"%d",&v);
                           if (l == 1) *(byte*)o = v;
                            else
                             if (l == 2) *(short*)o = v;
                              else
                               if (l == 4) *(int*)o = v;
                          }
                       else             // floating point
                        if ((a == 1) && (l == 4)) {sscanf(p,"%f",&f); *(float*)o = f; }
                         else           // String (truncate to max value if over)
                          if (a == 7) {if (ls >= l) {ls = l-1; p[ls] = 0;} strncpy_P((char*)o, p, ls);}
                      request->send(200, "text/plain", "Ok");          
                      break;
       
       
         case Array:  // is an array build the out as string by values comma separated
                      // decode the names into a sequence of substrings
                      p = strtok(p,","); i = 0;
                      while (p) 
                            {
                             if (a < 4)	      // is a numeric value
                                {
                                 sscanf(p,"%d",&v); 
                                 if (Verbose & 0x2) Serial.printf("Wr%d\n",v);
                                 if (a == 0) *((byte*)o + i)    = v;
                                  else
                                   if (a == 1) *((short*)o + i) = v;
                                    else
                                     if (a == 2) *((int*)o + i) = v;
                                      else
                                       if (a == 3) {sscanf(p,"%f",&f); *((float*) o + i) = f;}
                                 }
                              else            
                                if (a == 7) strcpy((char*)o + i * l, p);
                              p = strtok(NULL, ",");                          // next
                              i++;
                             if (i > l) {Serial.printf("Expected %hd Found %hd",l,i); break;}
                            }
                      request->send(200, "text/plain", "Ok");          
                      
                      break;
         case Struct:   // is a memory block,build the out as bytes buffer
                      i = 0;
                      request->send(200, "text/plain", "Ok");          
                      break;
        
        }
}


// ==========================================================================

 void TWebSrv::ExeCmd(AsyncWebServerRequest *request) 
// --------------------------------------
// Execute a Cmd instruction
// --------------------------------------
{
  extern char Pwd[]; 
  char *p = (char*)request->getParam(0)->value().c_str();
  short i;
  // get code 
  sscanf((char*)request->getParam(0)->name().c_str(),"%hhd",&Cmd.Code); 

  if (Verbose & 0x2) Serial.printf("Cmd: %hhd (%s)\n",Cmd.Code, p);

  strncpy(Cmd.Txt,p,sizeof(Cmd.Txt)-1); Cmd.Txt[sizeof(Cmd.Txt)-1] = 0;
  // Commands 0..99 if callback,tranfer out (Code == 1: terminal command, 2= generic cmd with arg)
  if (CmdCb && (Cmd.Code < 100)) 
      {
       if (CmdCb(Cmd.Code, Cmd.Txt))  request->send(200, "text/plain", "Ok");
        else                          request->send(200, "text/plain", Cmd.Txt);
       return;
      } 
  // internal handled
  switch (Cmd.Code)
    {
     
     case 0x71: // Check for authorization password  
                for (byte i = 0; i < sizeof(Cmd.Txt); i++) 
                    {if (!Cmd.Txt[i]) break; Cmd.Txt[i] = (Cmd.Txt[i] ^ Kw[0]) - i;}
                if (!strcmp(Pwd,(const char*)Cmd.Txt)) Kw[1] = 1; else Kw[1] = 0;
                request->send(200, "text/plain", "Ok");
                break;

     default:   // any other case
                request->send(400, "text/plain", "Unknown Command");          
    }
//  Serial.printf("\n%hhx %s  (%hhd)\n",Cmd.Code, Cmd.Data, Kw[1]);
}
  