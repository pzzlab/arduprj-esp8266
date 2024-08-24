/**
 * The MIT License (MIT)
 * Copyright (c) 2015 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "NTPClient.h"

NTPClient::NTPClient(UDP& udp) {
  _udp            = &udp;
}

NTPClient::NTPClient(UDP& udp, long timeOffset) {
  _udp            = &udp;
  _timeOffset     = timeOffset;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName) {
  _udp            = &udp;
  _poolServerName = poolServerName;
}

NTPClient::NTPClient(UDP& udp, IPAddress poolServerIP) {
  _udp            = &udp;
  _poolServerIP   = poolServerIP;
  _poolServerName = NULL;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName, long timeOffset) {
  _udp            = &udp;
  _timeOffset     = timeOffset;
  _poolServerName = poolServerName;
}

NTPClient::NTPClient(UDP& udp, IPAddress poolServerIP, long timeOffset){
  _udp            = &udp;
  _timeOffset     = timeOffset;
  _poolServerIP   = poolServerIP;
  _poolServerName = NULL;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName, long timeOffset, unsigned long updateInterval) {
  _udp            = &udp;
  _timeOffset     = timeOffset;
  _poolServerName = poolServerName;
  _updateInterval = updateInterval;
}

NTPClient::NTPClient(UDP& udp, IPAddress poolServerIP, long timeOffset, unsigned long updateInterval) {
  _udp            = &udp;
  _timeOffset     = timeOffset;
  _poolServerIP   = poolServerIP;
  _poolServerName = NULL;
  _updateInterval = updateInterval;
}

void NTPClient::begin() {
  begin(NTP_DEFAULT_LOCAL_PORT);
}

void NTPClient::begin(unsigned int port) 
{
  _port = port;
  _udp->begin(_port);
  _udpSetup = true;
}

bool NTPClient::forceUpdate() 
// -------------------------------------------
// Modified version: 
// If lost connection,continue to count clock
// At first call return false if not NTP time 
// after, update (if available) or continue in
// simulation (compensate the calling delay)
// -------------------------------------------

{
  byte timeout; int cb;
  unsigned intime = millis();
  
  #define NRETRY 5
  
  // flush any existing packets 
  _udp->flush();
//  while(_udp->parsePacket() != 0)  _udp->flush();                       
  // requeat time 
  sendNTPPacket();                                                      
  
  // Wait data rx  or timeout...
  for (cb = timeout = 0; !cb && (timeout < NRETRY); delay(10), timeout++) 
       {cb = _udp->parsePacket();}
  // return fail if not connected at first time (save current millis())
  if ((timeout >= NRETRY) && !_lastUpdate) {_lastUpdate = intime; return(false);}
  // valid response
  if (cb && (timeout < NRETRY))
      {
       // get data rx 
       _udp->read(_packetBuffer, NTP_PACKET_SIZE);
       // isolate bytes 40..43 NTP time (seconds since Jan 1 1900)
       unsigned secsSince1900 = (word(_packetBuffer[40], _packetBuffer[41]) << 16) 
                               | word(_packetBuffer[42], _packetBuffer[43]);
  #ifdef DBG_NTP 
	    Serial.printf("<Sync, Diff(int->NTP): %u >\n",((secsSince1900 - SEVENZYYEARS - _currentEpoc) % 60));
  #endif
      _currentEpoc = secsSince1900 - SEVENZYYEARS;
      }  
    else
        // no response or timeout,update time and compensate delay
       {
         _currentEpoc += (intime - _lastUpdate ) / 1000;      
  #ifdef DBG_NTP 
	       Serial.printf("<--- %u \n",_currentEpoc %60);
  #endif
         _lastUpdate = intime;                              
        return(false);
       }
  _lastUpdate = intime;                                       // update for next interval
  return true;
}

bool NTPClient::update() 
{
  // Update after _updateInterval
  if (((millis() - _lastUpdate) >= _updateInterval) || !_lastUpdate) 
  {                                // Update if there was no update yet.
    if (!_udpSetup || _port != NTP_DEFAULT_LOCAL_PORT) begin(_port); // setup the UDP client if needed
    return forceUpdate();
  }
  return false;   // return false if update does not occur
}

bool NTPClient::isTimeSet() const {
  return (_lastUpdate != 0); // returns true if the time has been set, else false
}

unsigned long NTPClient::getEpochTime() const {
  return _timeOffset + // User offset
         _currentEpoc + // Epoch returned by the NTP server
         ((millis() - _lastUpdate) / 1000); // Time since last update
}

int NTPClient::getDay() const {
  return (((getEpochTime()  / 86400L) + 4 ) % 7); //0 is Sunday
}
int NTPClient::getHours() const {
  return ((getEpochTime()  % 86400L) / 3600);
}
int NTPClient::getMinutes() const {
  return ((getEpochTime() % 3600) / 60);
}
int NTPClient::getSeconds() const {
  return (getEpochTime() % 60);
}

String NTPClient::getFormattedTime() const {
  unsigned long rawTime = getEpochTime();
  unsigned long hours = (rawTime % 86400L) / 3600;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  unsigned long seconds = rawTime % 60;
  String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

  return hoursStr + ":" + minuteStr + ":" + secondStr;
}

void NTPClient::end() {
  _udp->stop();

  _udpSetup = false;
}

void NTPClient::setTimeOffset(int timeOffset) {
  _timeOffset     = timeOffset;
}

void NTPClient::setUpdateInterval(unsigned long updateInterval) {
  _updateInterval = updateInterval;
}

void NTPClient::setPoolServerName(const char* poolServerName) {
    _poolServerName = poolServerName;
}

void NTPClient::sendNTPPacket() {
  // set all bytes in the buffer to 0
  memset(_packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  _packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  _packetBuffer[1] = 0;     // Stratum, or type of clock
  _packetBuffer[2] = 6;     // Polling Interval
  _packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  _packetBuffer[12]  = 49;
  _packetBuffer[13]  = 0x4E;
  _packetBuffer[14]  = 49;
  _packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  if  (_poolServerName) {
    _udp->beginPacket(_poolServerName, 123);
  } else {
    _udp->beginPacket(_poolServerIP, 123);
  }
  _udp->write(_packetBuffer, NTP_PACKET_SIZE);
  _udp->endPacket();
}

void NTPClient::setRandomPort(unsigned int minValue, unsigned int maxValue) {
  randomSeed(analogRead(0));
  _port = random(minValue, maxValue);
}
