/*
 *
 * This file is part of the SCK v0.9 - SmartCitizen
 *
 * This file may be licensed under the terms of of the
 * GNU General Public License Version 2 (the ``GPL'').
 *
 * Software distributed under the License is distributed
 * on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied. See the GPL for the specific language
 * governing rights and limitations.
 *
 * You should have received a copy of the GPL along with this
 * program. If not, go to http://www.gnu.org/licenses/gpl.html
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */



#include "Constants.h"
#include "SCKServer.h"
#include "SCKBase.h"
#include "SCKAmbient.h"
#include <Wire.h>
#include <EEPROM.h>

#define debugServer   false

#define TIME_BUFFER_SIZE 20 

boolean SCKServer::time(char *time_) {
  boolean ok=false;
  uint8_t count = 0;
  byte retry=0;
  while (retry<5)
  {
   retry++;
   if (base_.enterCommandMode()) 
    {
      if (base_.open(WEB[0], 80))
       {
        for(byte i = 0; i<3; i++) Serial1.print(WEBTIME[i]); //Requests to the server time
        if (base_.findInResponse("UTC:", 2000)) 
          {
              char newChar;
              byte offset = 0;
              unsigned long time = millis();
              while (offset < TIME_BUFFER_SIZE) {
                if (Serial1.available())
                {
                  newChar = Serial1.read();
                  time = millis();
                  if (newChar == '#') {
                    ok = true;
                    time_[offset] = '\x00';
                    return ok;
                  } 
                  else if (newChar != -1) {
                    if (newChar==',') 
                    {
                      if (count<2) time_[offset]='-';
                      else if (count>2) time_[offset]=':';
                      else time_[offset]=' ';
                      count++;
                    }
                    else time_[offset] = newChar;
                    offset++;
                  }
                }
                else if((millis()-time)>1000)
                {
                  ok = false;
                  break; 
                }
              }
           }
         base_.close();
       }
    }
  }
  time_[0] = '#';
  time_[1] = 0x00;
  base_.exitCommandMode();
  return ok;
}

void SCKServer::json_update(uint16_t updates, long *value, char *time, boolean isMultipart)
{  
      #if debugServer
         Serial.print(F("["));
      #endif
      Serial1.print(F("["));  
      for (int i = 0; i< updates;i++)
        {
          readFIFO();
          if ((i< (updates - 1)) || (isMultipart))
            {
              Serial1.print(F(","));
              #if debugServer
                Serial.print(F(","));
              #endif
            }
        }
 
 if (isMultipart)
   {
      byte i;
      for (i = 0; i<9; i++)
        {
          Serial1.print(SERVER[i]);
          Serial1.print(value[i]); 
        }  
      Serial1.print(SERVER[i]);  
      Serial1.print(time);
      Serial1.print(SERVER[i+1]);
      Serial1.println(F("]"));
      Serial1.println();
      
      #if debugServer
         for (i = 0; i<9; i++)
         {
         Serial.print(SERVER[i]);
         Serial.print(value[i]);
         }
         Serial.print(SERVER[i]);
         Serial.print(time);
         Serial.print(SERVER[i+1]);
      #endif
   }
      Serial1.println(F("]"));
      Serial1.println();
      #if debugServer
         Serial.println(F("]"));
      #endif
}  

void SCKServer::addFIFO(long *value, char *time)
  {
    int eeaddress = base_.readData(EE_ADDR_NUMBER_WRITE_MEASURE, INTERNAL);
    int i = 0;
    for (i = 0; i<9; i++)
      {
        base_.writeData(eeaddress + i*4, value[i], EXTERNAL);
      } 
    base_.writeData(eeaddress + i*4, 0, time, EXTERNAL);
    eeaddress = eeaddress + (SENSORS)*4 + TIME_BUFFER_SIZE;
    base_.writeData(EE_ADDR_NUMBER_WRITE_MEASURE, eeaddress, INTERNAL);
  }

void SCKServer::readFIFO()
  {   
    int i = 0;
    int eeaddress = base_.readData(EE_ADDR_NUMBER_READ_MEASURE, INTERNAL);
    for (i = 0; i<9; i++)
      {
        Serial1.print(SERVER[i]);
        Serial1.print(base_.readData(eeaddress + i*4, EXTERNAL)); //SENSORS
      }  
    Serial1.print(SERVER[i]);  
    Serial1.print(base_.readData(eeaddress + i*4, 0, EXTERNAL)); //TIME
    Serial1.print(SERVER[i+1]);
    
    #if debugServer
      for (i = 0; i<9; i++)
       {
         Serial.print(SERVER[i]);
         Serial.print(base_.readData(eeaddress + i*4, EXTERNAL)); //SENSORS
       }
      Serial.print(SERVER[i]);
      Serial.print(base_.readData(eeaddress + i*4, 0, EXTERNAL)); //TIME
      Serial.print(SERVER[i+1]);
    #endif

    eeaddress = eeaddress + (SENSORS)*4 + TIME_BUFFER_SIZE;
    if (eeaddress == base_.readData(EE_ADDR_NUMBER_WRITE_MEASURE, INTERNAL))
      {
        base_.writeData(EE_ADDR_NUMBER_WRITE_MEASURE, 0, INTERNAL);
        base_.writeData(EE_ADDR_NUMBER_READ_MEASURE, 0, INTERNAL);
      }
    else base_.writeData(EE_ADDR_NUMBER_READ_MEASURE, eeaddress, INTERNAL);
  }  
  
#define numbers_retry 5

boolean SCKServer::update(long *value, char *time_)
{
  value[8] = base_.scan();  //Wifi Nets
  byte retry = 0;
  if (time(time_)) //Update server time
  {  
    if (base_.checkRTC())
    {
      while (!base_.RTCadjust(time_)&&(retry<numbers_retry)) retry++;
    }
  }
  else if (base_.checkRTC())
         {
           base_.RTCtime(time_);
           #if debugEnabled
              if (!ambient_.debug_state()) Serial.println(F("Fail server time!!"));
           #endif
         }
  else 
    {
      time_ = "#";
      return false;
    }
  return true; 
}

boolean SCKServer::connect()
{
  int retry = 0;
  while (true){
    if (base_.open(WEB[0], 80)) break;
    else 
    {
      retry++;
      if (retry >= numbers_retry) return false;
    }
  }    
  for (byte i = 1; i<5; i++) Serial1.print(WEB[i]);
  Serial1.println(base_.readData(EE_ADDR_MAC, 0, INTERNAL)); //MAC ADDRESS
  Serial1.print(WEB[5]);
  Serial1.println(base_.readData(EE_ADDR_APIKEY, 0, INTERNAL)); //Apikey
  Serial1.print(WEB[6]);
  Serial1.println(FirmWare); //Firmware version
  Serial1.print(WEB[7]);
  return true; 
}


void SCKServer::send(boolean sleep, boolean *wait_moment, long *value, char *time) {  
  *wait_moment = true;
  uint16_t updates = (base_.readData(EE_ADDR_NUMBER_WRITE_MEASURE, INTERNAL)-base_.readData(EE_ADDR_NUMBER_READ_MEASURE, INTERNAL))/((SENSORS)*4 + TIME_BUFFER_SIZE);
  uint16_t NumUpdates = base_.readData(EE_ADDR_NUMBER_UPDATES, INTERNAL); // Number of readings before batch update
  if (updates>=(NumUpdates - 1))
    { 
      if (sleep)
        {
          #if debugEnabled
              if (!ambient_.debug_state()) Serial.println(F("SCK Waking up..."));
          #endif
          digitalWrite(AWAKE, HIGH);
        }
      if (base_.connect())  //Wifi connect
        {
          #if debugEnabled
              if (!ambient_.debug_state()) Serial.println(F("SCK Connected!!")); 
          #endif   
          if (update(value, time)) //Update time and nets
          {
            #if debugEnabled
                if (!ambient_.debug_state())
                {
                  Serial.print(F("updates = "));
                  Serial.println(updates + 1);
                }
            #endif
            int num_post = updates;
            int cycles = cycles = updates/POST_MAX;;
            if (updates > POST_MAX) 
              {
                for (int i=0; i<cycles; i++)
                {
                  connect();
                  json_update(POST_MAX, value, time, false);
                }
                num_post = updates - cycles*POST_MAX;
              }
            connect();
            json_update(num_post, value, time, true);
            #if debugEnabled
                  if (!ambient_.debug_state()) Serial.println(F("Posted to Server!")); 
            #endif
            
          }
          else 
          {
            #if debugEnabled
                if (!ambient_.debug_state())
                {
                  Serial.println(F("Error updating time Server..!"));
                }
            #endif
          }
          #if debugEnabled
              if (!ambient_.debug_state()) Serial.println(F("Old connection active. Closing..."));
          #endif
          base_.close();
        }
      else //No connect
        {
          addFIFO(value, time);
          #if debugEnabled
              if (!ambient_.debug_state()) Serial.println(F("Error in connectionn!!"));
          #endif
        }
      if (sleep)
        {
          base_.sleep();
          #if debugEnabled
              if (!ambient_.debug_state())
              {
                Serial.println(F("SCK Sleeping")); 
                Serial.println(F("*******************"));
              }
          #endif
          digitalWrite(AWAKE, LOW); 
       }
    }
  else
    {
        //value[8] = 0;  //Wifi Nets
        if (base_.checkRTC()) base_.RTCtime(time);
        else time = "#";
        addFIFO(value, time);
        #if debugEnabled
          if (!ambient_.debug_state()) Serial.println(F("Saved in memory!!"));
        #endif
    }
  *wait_moment = false;
}

