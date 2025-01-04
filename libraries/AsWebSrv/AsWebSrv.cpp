#include "AsWebSrv.h"


AsyncWebServer  Srv(80);
TCmd            Cmd; 
bool           (*CmdCb)(byte,char*);
byte            Verbose,Auth;
TAuth           Access[3];




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
  const char *p1 = request->getParam(0)->name().c_str(),*p2 = NULL;
  word lev = 0;
  if (sscanf(p1,"%hd",&lev))  lev = (lev-1) & 0x03; // max 4 levels
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
  byte  led = 0, retry = 250, ch = 0;
  char  apssid[33]= {""}, appwd[33]= {""}, stassid[33]= {""}, stapwd[33]= {""};
  
  // Assign as default the Led pin and input0 as input (if uses i2c, will be reconfigured later)
  pinMode(LED_BUILTIN, OUTPUT);   pinMode(0, INPUT_PULLUP);
  // Check for FS available
  if (!Fs.begin())  {Serial.println("\nInvalid Fs"); return(false);} 
  // assign static files handling
  Srv.serveStatic("/",	    Fs,"/").setDefaultFile("index.html");
  
   // read from ws.ini Verbose and host name
   IniFile * ini = new IniFile("ws.ini");
   Verbose = ini->ReadInteger("misc","verbose",127);       // Verbose level (default = 127) and display
  if (Verbose) Serial.printf("\nVerbose: 0x%02hhx\n",Verbose);
  // Get HostName
  strlcpy(HostName,ini->ReadString("misc","hostname","pwrmon"),sizeof(HostName)); 
  if (Verbose & 2) Serial.printf("HostName: %s\n",HostName);
  // Get Authentication Auth=usr pwd (max 3 levels)
  strlcpy(Access[0].Usr, ini->ReadString("auth","level1","user"), sizeof(Access[0].Usr)); strlcpy(Access[0].Pwd, ini->ReadString("auth","pwd1","12345")  ,sizeof(Access[0].Pwd));
  strlcpy(Access[1].Usr, ini->ReadString("auth","level2","user2"),sizeof(Access[1].Usr)); strlcpy(Access[1].Pwd, ini->ReadString("auth","pwd2","123456") ,sizeof(Access[1].Pwd));
  strlcpy(Access[2].Usr, ini->ReadString("auth","level3","user3"),sizeof(Access[2].Usr)); strlcpy(Access[2].Pwd, ini->ReadString("auth","pwd3","1234567"),sizeof(Access[2].Pwd));
  if ((Verbose & 2) && *Access[0].Usr && *Access[0].Pwd)  Serial.printf("AutH: %s %s\n",Access[0].Usr,Access[0].Pwd);
  if ((Verbose & 2) && *Access[1].Usr && *Access[1].Pwd)  Serial.printf("AutH: %s %s\n",Access[1].Usr,Access[1].Pwd);
  if ((Verbose & 2) && *Access[2].Usr && *Access[2].Pwd)  Serial.printf("AutH: %s %s\n",Access[2].Usr,Access[2].Pwd);
  // ---- Start with WIFI connection ----
  if (Verbose & 2) Serial.println("WebServer Init"); 

 // [wifi] sta + ap mode 
 //  stassid=wps    -> connect via WPS button
 //  stassid=SSID   -> station SSID
 //  stapwd=PWD     -> station PWD
 //  apssid         -> AcessPoint SSID
 //  appwd          -> AcessPoint PWD

 strlcpy(stassid,ini->ReadString("wifi","stassid",""),sizeof(stassid)); 
 strlcpy(stapwd ,ini->ReadString("wifi","stapwd", ""),sizeof(stapwd)); 
 
 if (strlen(stassid))             // read Sta=ssid pwd (or Sta=wps)
   { 
    WiFi.mode(WIFI_STA);
    if (Verbose & 2) 
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
        if (Verbose & 1) Serial.println("Failed, Push WPS button on AP");              
        while (!WiFi.beginWPSConfig() || !WiFi.SSID().length()) {LED(led); led = !led; Serial.print(".");}
        if (Verbose & 2) Serial.println("** PAIRED **");              
       }
       else 
        if (Verbose & 1) Serial.printf("Failed\n");              
     }
     else        
      if (Verbose & 0x2) Serial.printf("Connected With %s (%s)\n",WiFi.SSID().c_str(), WiFi.psk().c_str());
    StaIP = WiFi.localIP();
    if (Verbose & 0x2)   Serial.printf("HostName:%s\n",HostName);   
   }
   ch = WiFi.channel() + 2;
   if (ch > 13) ch = 13;
 // AP Mode, register the SSID     
 strlcpy(apssid,ini->ReadString("wifi","apssid",""),sizeof(apssid)); 
 strlcpy(appwd ,ini->ReadString("wifi","appwd", ""),sizeof(appwd)); 
 // defined AP mode, set (may be open(no pwd) *** PWD MUST be >= 8 chars**)
 if (strlen(apssid))  
  {
   LED(true);              if (Verbose & 2) Serial.printf("Starting AP:%s (%s)\n",apssid,appwd);
   if (!WiFi.softAP(apssid,appwd,ch) && Verbose & 1) Serial.println("Failed (pwd >= 8 chars?)");
   ApIP = WiFi.softAPIP();
   LED(false);
  }
 // Here come if the connection has successful
 if (strlen(HostName)) WiFi.setHostname(HostName);              // assume host name saved on file
 LLMNR.begin(HostName);
 LED(false);
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
    if (!strcmp(Vars[i].Name,name)) return(i);                                      // return index
 return(-1);                                                                        // not found
}

// ==========================================================================
// -----------------------------------------------
// Parse a webserver request
// If unexpected, return 404
// -> FRead     read a file
// -> Cmd       execute a specific command 
// -> Read      read  a variable defined into struct Vars[]
// -> Write     write a variable defined into struct Vars[]
// -----------------------------------------------

void TWebSrv::Parse(AsyncWebServerRequest *request)
{
  char *c = (char*)request->url().c_str();
  
  if (!request->args()) {request->send(404, "text/plain", request->url() + " Not Found!"); return;}
  const char *a = request->getParam(0)->name().c_str();
  // FRead: read a file
  if (!strcmp(c,"/FRead")) {request->send(Fs, a); return;}
  // Special case: Execute a command (struct TCmd))
  if (!strcmp(c,"/Cmd"))      {ExeCmd(request); return;}  // Write a variable
  // other cases here
  // ..
  // Is variable, search on list of availebles
  short  n = SearchVar(a);
  // Not found , report oin client the error
  if (n < 0)  
     {request->send(400, "text/plain"," Var: <" + request->getParam(0)->name() + "> Not Found"); return;}
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
 
 if (idx > (VarsCnt)) return;                           // can't happen, but check always
    
 char   out[32], *buf;
 byte   a = Vars[idx].Attr & 0xf; 
 word   l = Vars[idx].Len;                              
 void  *o = Vars[idx].Obj;
 
 // refuse if unhautorized
 if ((Vars[idx].Attr & 0x40) && !Auth)   {request->send(200, "text/plain","Unauthorized");return;}
 
 if (Verbose & 4) Serial.printf("ReadVar(%s)\n",Vars[idx].Name);
 // Allocate Response Buffer, invalid, error
 if (((buf = new char[1536]) == NULL) && (Verbose & 1))
    {Serial.println("ParseReq:/Read: Cannot Allocate Response Buffer"); return;}
 buf[0] = 0;
// Serial.printf("Var %s, Fmt %s, Typ %hhd, Attr%hhd, Size%hd\n", Vars[idx].Name,Vars[idx].Fmt,Vars[idx].Type,a,l);
 // switch from variable types
 switch (Vars[idx].Type)
        {
         case Var:    // is a single variable,build the out string by format and data 
                      if (a == 0)                                                         // integer  
                       {
                         if (l == 1) sprintf(out,Vars[idx].Fmt,*(byte*)o);                // signed byte
                          else
                           if (l == 2) sprintf(out,Vars[idx].Fmt,*(short*)o);             // short
                            else
                             if (l == 4) sprintf(out,Vars[idx].Fmt,*(int*)o);             // int
                        }
                        else                                                                         
                         if ((a == 1) && (l == 4)) sprintf(out,Vars[idx].Fmt,*(float*)o); // float32
                          else                     
                           if (a == 7) strcpy(out,(char*)o);                              // String or char[]
                       request->send(200, "text/plain", buf);     
//                       Serial.println(buf);
                       break;
                         
         case Array:  // is an array build the out as string by values comma separated
                      if (a < 4)
                        {
                         for (word i = 0; i < l; i++)                                     // Loop for any element
                           {
                            if (a == 0) sprintf(out,Vars[idx].Fmt,*((byte*)o + i));    	  // signed byte
                             else
                              if (a == 1) sprintf(out,Vars[idx].Fmt,*((short*)o + i));    // short
                               else
                                if (a == 2) sprintf(out,Vars[idx].Fmt,*((int*)o + i));    // int
                                 else                                                                         
                                  if (a == 3) sprintf(out,Vars[idx].Fmt,*((float*)o + i));// float32
                             // Concatenate and if not the last, add separator
                             strcat(buf,out); if (i < (l - 1)) strcat(buf, ";");
                           }      
                        }   
                       if (a == 7) 																											  // strings
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
 char  *p  = (char*)request->getParam(0)->value().c_str(), i;
 short  ls = strlen(p); 
 
 // refuse if unhautorized
 if ((Vars[idx].Attr & 0x80) && !Auth)  {request->send(200, "text/plain","Unauthorized");return;}
 
 if (Verbose & 4) Serial.printf("WriteVar: %s (%s)\n",Vars[idx].Name, p);
 
 // switch from variable types
 switch (Vars[idx].Type)
        {
         case Var:    // is a single variable,build the out string by format and data 
                      if (a == 0)                               // integer
                          {
                           sscanf(p,"%d",&v);
                           if (l == 1) *(byte*)o = v;
                            else
                             if (l == 2) *(short*)o = v;
                              else
                               if (l == 4) *(int*)o = v;
                          }
                       else                                     // floating point
                        if ((a == 1) && (l == 4)) {sscanf(p,"%f",&f); *(float*)o = f; }
                         else                                   // String (truncate to max value if over)
                          if (a == 7) {if (ls >= l) {ls = l-1; p[ls] = 0;} strlcpy((char*)o, p, ls);}
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
                                 if (Verbose & 0x4) Serial.printf("Wr%d\n",v);
                                 if (a == 0) *((byte*)o + i)    = v;                            // byte
                                  else
                                   if (a == 1) *((short*)o + i) = v;                            // word
                                    else
                                     if (a == 2) *((int*)o + i) = v;                            // dword
                                      else
                                       if (a == 3) {sscanf(p,"%f",&f); *((float*) o + i) = f;}  // float32
                                 }
                              else            
                                if (a == 7) strcpy((char*)o + i * l, p);                        // char[]
                              p = strtok(NULL, ",");                                      
                              i++;
                             // unexpected error
                             if ((i > l) && (Verbose & 1)) {Serial.printf("Expected %hd Found %hd",l,i); break;}
                            }
                      request->send(200, "text/plain", "Ok");          
                      
                      break;
         case Struct:   // is a memory block,build the out as bytes buffer
                      // unimplented now
                      i = 0;
                      request->send(200, "text/plain", "Ok");          
                      break;
        
        }
}


// ==========================================================================

 void TWebSrv::ExeCmd(AsyncWebServerRequest *request) 
// --------------------------------------
// Execute a Cmd instruction (.Cmd [byte] + .Txt[char*])
// and (if valid) call the external callback
// with the arguments (0 < code < 100).
// or execute specific here if more
// --------------------------------------
{
  char *p = (char*)request->getParam(0)->value().c_str();
  // get code 
  sscanf((char*)request->getParam(0)->name().c_str(),"%hhd",&Cmd.Code); 

  if (Verbose & 4) Serial.printf("Cmd: %hhd (%s)\n",Cmd.Code, p);

  strlcpy(Cmd.Txt,p,sizeof(Cmd.Txt)-1); Cmd.Txt[sizeof(Cmd.Txt)-1] = 0;
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
     default:   // any other case
                request->send(400, "text/plain", "Unknown Command");          
    }
}
  