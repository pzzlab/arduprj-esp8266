
#include <IniFile.h>

IniFile   *ini;

void setup() 
{
  String s;
   // Initialize serial port
  Serial.begin(115200);
  LittleFS.begin();
  Serial.println("\nWrite a char then return");

}

void loop() 
{

 if (Serial.available())
  {
    Serial.read();
    String s;
    ini = new IniFile("config.ini");
    Serial.println("-------- Lettura 6 valori ----------------");

    int m = millis();

    Serial.printf("%s \n", ini->ReadString("sezione1","chiave2.1","niente1"));
    Serial.printf("%s \n", ini->ReadString("sezione2","chiave2.2","niente2"));
    Serial.printf("%s \n", ini->ReadString("sezione3","chiave1.3","niente3"));
    Serial.printf("%s \n", ini->ReadString("sezione3","chiave2.3","niente4"));
    Serial.printf("%d \n", ini->ReadInteger("sezione4","chiavedec",-12345678));
    Serial.printf("%d \n", ini->ReadInteger("sezione4","chiavehex",-9999));
    
    Serial.printf(">> Read Time: %ul\n",millis()-m); m = millis();

    Serial.println("-------- Scrittura 6 valori ----------------");
    if (ini->WriteString  ("sezione6","chiave6","il_nuovo_6"))      Serial.println("WrString ok");
    if (ini->WriteString  ("sezione3","chiave1.3","il_nuovo_1.3"))  Serial.println("WrString ok");
    if (ini->WriteInteger ("sezione2","chiave5.2",12345678))        Serial.println("WrInteger ok");
    if (ini->WriteInteger ("sezione5","chiave5.1",555))             Serial.println("WrInteger ok");
    if (ini->WriteHex     ("sezione5","chiave5.2",0x12345678))      Serial.println("WrHex ok");
    if (ini->WriteInteger ("sezione8","chiave8.1",81))              Serial.println("WrInteger ok");
    if (ini->DeleteKey    ("sezione2","chiave3.2"))                 Serial.println("DeleteKey ok");
    if (ini->DeleteKey    ("sezione5","chiave5.3"))                 Serial.println("DeleteKey ok");
    
    Serial.printf(">> Write Time: %u\n",millis()-m); 
    delete ini;
 }

  delay(50);
  // not used in this example
}
