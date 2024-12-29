#include <IniFile.h>  

 IniFile::IniFile(String filename)
// --------------------------------------
// Constructor.
// if file not exixst,create one empty
// --------------------------------------
 {
  fname = filename; 
  if (!LittleFS.exists(fname)) 
    {fin = LittleFS.open(fname,"w"); fin.close();}
 }   


 void IniFile::strclean(char* s)
// --------------------------------------
// clean from whithespaces and comment
// --------------------------------------
 {
  word n,l= strlen(s);
  for ( n = 0; n < l; n++)
      {
       if (s[n] == ';') {s[n] = 0; break;}
       if (strchr(" \n\r\t",s[n])) {memmove(&s[n],&s[n]+1,l-n-1);  l--; n--; s[l] = 0;}
      }
 }


// Loads the configuration from a file
char* IniFile::ReadString(const char* section, const char* key, const char* def) 
// --------------------------------------
// read a string from inifile, if not 
// found return the default value
// --------------------------------------
 {
  
  char     c, sectfl = 0;
  fin = LittleFS.open(fname, "r");
  
  if (!fin) return(NULL);

  s1 = "[" + String(section) + "]";                           // section to compare
  s = "";                                                     // clean string
  while (fin.available() && (c = fin.read()))
      {
       s += c;                                                // add a char
       if (c == '\n')                                         // newline,parse
          {
           strclean((char*)s.c_str()); s = String(s.c_str()); // remove all spaces,tab,\n,\r and comment from line( c_func) then reassign string class
           if (!sectfl && (s == s1)) sectfl = 1;              // search the matching section
           else
             {
              if (s.startsWith(String(key) + "="))            // section found, now search key (if found, return)
                 {s = s.substring(strlen(key) + 1); fin.close(); return((char*)s.c_str());}
             }        
           s = "";                                            // clean string for nxt line
          }
      }
  return((char*)def);                                                // not found, return the default value
 }

// Loads the configuration from a file
int IniFile::ReadInteger(const char* section, const char* key, int def) 
// --------------------------------------
// read an integer from inifile, if not 
// found return the default value
// try at first as hex then dec
// --------------------------------------
 {
  int      res;
  char tmp[32];

  sprintf(tmp,"%d",def);
  String str = ReadString(section,key,tmp);
  const char * p = str.c_str();

  if ((sscanf(p,"0x%x", &res) == 1) || (sscanf(p,"%d"  , &res) == 1))   {fin.close(); return(res);}
   else return(-999999999);
  return(def);                                                // not found, return the default value
 }


bool IniFile::WriteInteger(const char* section, const char* key, int value) 
// --------------------------------------
// write an integer to inifile, if not 
// found,create a key with value
// (if found,replace older)
// --------------------------------------
{
  char val[32];
  sprintf(val,"%d",value);
  return(WriteString(section,key,val));
 }

bool IniFile::WriteHex(const char* section, const char* key, unsigned value) 
// --------------------------------------
// write an integer to inifile, if not 
// found,create a key with value
// (if found,replace older)
// --------------------------------------
 {
  char val[16];
  sprintf(val,"%x",value);
  return(WriteString(section,key,val));
 }


bool IniFile::WriteString(const char* section, const char* key, const char* value) 
// --------------------------------------
// write astring to inifile, if not 
// found,create a key with value
// (if found,replace older)
// --------------------------------------
 {
  char done = 0, c, step = 0;
  
  s1 = fname + "t";                                           // at 't' at filename
  fin  = LittleFS.open(fname, "r");
  fout = LittleFS.open(s1 , "w");
  
  s1 = "[" + String(section) + "]";                           // section to compare
  strclean((char*)value);                                     // remove all spaces,tab and \n\r 
  if (!fin) return(false);
  s = "";                                                     // clean string
  
  while (fin.available() && (c = fin.read()))
      {
       s += c;
       if (c == '\n') 
          {
           // remove all spaces,tab,\n,\r and comment from line( c_func) then reassign string class
           strclean((char*)s.c_str()); s = String(s.c_str());            
           switch (step)
                  {
                    case  0:  // serch for valid section
                          done = false;
                          if (s != s1) break;
                          step++;      
                          break;
                    case  1:  // into section,search until found key or a new section
                          if (s.startsWith(String(key) + "="))// replace old value with new
                             {s = String(key) + " = " + String(value); done = true; }   
                           else
                            if (!done && (s[0] == '['))       // found a new section, and not yet replaced 
                               {                              // append the new key
                                s1 = String(key) + " = " + String(value) + "\n\r"; 
                                fout.write(s1.c_str(),s1.length());
                                done = true;
                                step++;
                                break;
                               }
                          break;
                    }
            s += "\n\r"; fout.write(s.c_str(),s.length());    // copy original, or replaced or new value
            s = "";                                           // clean string for nxt line
          }
      }
  // section.key not found, append a new section and write key=value
  if (!step)                                                          
    {s1 += "\n\r";                                  fout.write(s1.c_str(),s1.length());}
  if (!done)
    {s = String(key) + "=" + String(value) +"\n\r"; fout.write(s.c_str(),s.length());}
  fin.close(); fout.close();
  s1 = fname + "t";
  LittleFS.remove(fname);
  LittleFS.rename(s1,fname);                  
  return(true);
 }

bool IniFile::DeleteKey(const char* section, const char* key) 
// -------------------------------------------------------
// Remove a specific key and its value from file
// return:  true = ok
// -------------------------------------------------------
 {
  char done = 0, c, step = 0;
  
  s1 = fname + "t";                                           // at 't' at filename
  fin  = LittleFS.open(fname, "r");
  fout = LittleFS.open(s1 , "w");
  
  s1 = "[" + String(section) + "]";                           // section to compare
  if (!fin) return(false);
  s = "";                                                     // clean string
  
  while (fin.available() && (c = fin.read()))
      {
       s += c;
       if (c == '\n') 
          {
           // remove all spaces,tab,\n,\r and comment from line( c_func) then reassign string class
           strclean((char*)s.c_str()); s = String(s.c_str());            
           switch (step)
                  {
                    case  0:  // serch for valid section
                          done = false;
                          if (s != s1) break;
                          step++;      
                          break;
                    case  1:  // into section,search until found key or a new section
                          if (s.startsWith(String(key) + "="))// found, invalidate the string
                             {s = ""; done = true; }   
                          break;
                    }
            if (s != "") {s += "\n\r"; fout.write(s.c_str(),s.length());}    // copy original
            s = "";                                           // clean string for nxt line
          }
      }
  fin.close(); fout.close();
  s1 = fname + "t";
  LittleFS.remove(fname);
  LittleFS.rename(s1,fname);                  
  return(done);
 }
