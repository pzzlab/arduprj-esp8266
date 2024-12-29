#ifndef _IniFile
  #define _IniFile
#include <LittleFS.h>
  

class IniFile 
{
 private:
 void strclean(char* s);
 File   fin,fout;
 String s, s1, fname;
 
 public:
 IniFile (String filename);
 char        *ReadString  (const char* section, const char* key, const char* def);
 int          ReadInteger (const char* section, const char* key, int def);
 bool         WriteString (const char* section, const char* key, const char* value);
 bool         WriteInteger(const char* section, const char* key, int value);
 bool         WriteHex    (const char* section, const char* key, unsigned value);
 bool         DeleteKey   (const char* section, const char* key);
 };
#endif


 