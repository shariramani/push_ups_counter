//EEPROM
//https://roboticsbackend.com/arduino-store-int-into-eeprom/
void saveSettingsToEEPROM() {
  PRINTLN("Saving Settings to eeprom");
  EEPROM.write(1, Piano);
  EEPROM.write(2, Tabla);
  EEPROM.write(3, WindChime);
  EEPROM.write(4, Random);
  EEPROM.write(5, Counter);
  EEPROM.write(6, Distance);
  EEPROM.write(10, volume);
  EEPROM.write(11, sensitivity);
  writeIntIntoEEPROM(12,minRange); //2 bytes
  writeIntIntoEEPROM(14,maxRange); //2 bytes
  writeIntIntoEEPROM(16,waitAfterPlay); //2 bytes
  EEPROM.commit();
}



void loadSettingsFromEEPROM() {
  PRINTLN("Reading Settings from eeprom");
  Piano = EEPROM.read(1);
  Tabla = EEPROM.read(2);
  WindChime = EEPROM.read(3);
  Random = EEPROM.read(4);
  Counter = EEPROM.read(5);
  Distance = EEPROM.read(6);
  volume = EEPROM.read(10);
  sensitivity = EEPROM.read(11);
  minRange = readIntFromEEPROM(12);
  maxRange = readIntFromEEPROM(14);
  waitAfterPlay = readIntFromEEPROM(16);
}


//An int takes 2 bytes.
void writeIntIntoEEPROM(int address, int number)
{ 
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
}

int readIntFromEEPROM(int address)
{
  return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}


//An unsigned int takes 2 bytes.
void writeUnsignedIntIntoEEPROM(int address, unsigned int number)
{ 
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
}

unsigned int readUnsignedIntFromEEPROM(int address)
{
  return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}


//Long takes 4 bytes.

void writeLongIntoEEPROM(int address, long number)
{ 
  EEPROM.write(address, (number >> 24) & 0xFF);
  EEPROM.write(address + 1, (number >> 16) & 0xFF);
  EEPROM.write(address + 2, (number >> 8) & 0xFF);
  EEPROM.write(address + 3, number & 0xFF);
}
long readLongFromEEPROM(int address)
{
  return ((long)EEPROM.read(address) << 24) +
         ((long)EEPROM.read(address + 1) << 16) +
         ((long)EEPROM.read(address + 2) << 8) +
         (long)EEPROM.read(address + 3);
}


//Time is same as long
void EEPROMWriteTime(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
  EEPROM.commit();
  Serial.println("EEPROM write");
}


long EEPROMReadTime(int address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);
 
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}



//String to eeprom
void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
byte len = strToWrite.length();
EEPROM.write(addrOffset, len);
for (int i = 0; i < len; i++)
{
EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
}
EEPROM.commit();
}


String readStringFromEEPROM(int addrOffset)
{
int newStrLen = EEPROM.read(addrOffset);
char data[newStrLen + 1];
for (int i = 0; i < newStrLen; i++)
{
data[i] = EEPROM.read(addrOffset + 1 + i);
}
data[newStrLen] = '\0';
return String(data);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////

String time2string(time_t &thisTime) {  //convert time to time and date string for display in webpage.
  // Populate myDate String
  String time2str;
  time2str = hour(thisTime);

  if (minute(thisTime) < 10) {
    time2str = time2str + ":0" + minute(thisTime);
  }
  else {
    time2str = time2str + ":" + minute(thisTime);
  }


  if (second(thisTime) < 10) {
    time2str = time2str + ":0" + second(thisTime);
  }
  else {
    time2str = time2str + ":" + second(thisTime);
  }

  time2str = time2str + "  " + dayShortStr(weekday(thisTime)) + "  " + day(thisTime) + " " + monthShortStr(month(thisTime)) + " " + year(thisTime) ;
  //time2str= (" " + myTime +" ");
  //Serial.println("time2str: " + time2str);

  return time2str;
}

/////////////////////
//
time_t getEpochFromString(String nextTimer) {
  //Make time from event
  time_t startEvt;// Declare a time_t object for the start time (time_t)
  tmElements_t tmStartEvt; // Declare a tmElement_t object to manually input the start Time


  // Creates a tmElement by putting together all of Maggies Start Time and Dates
  tmStartEvt.Year = nextTimer.substring(2, 4).toInt() + 30; // years since 1970 in two digits
  tmStartEvt.Month = nextTimer.substring(5, 7).toInt();
  tmStartEvt.Day = nextTimer.substring(8, 10).toInt();
  tmStartEvt.Hour = nextTimer.substring(11, 13).toInt();
  tmStartEvt.Minute = nextTimer.substring(14, 16).toInt();
  tmStartEvt.Second = nextTimer.substring(17, 19).toInt();

  // Takes the tmElement and converts it to a time_t variable. Which can now be compared to the current (now) unix time
  startEvt = makeTime(tmStartEvt);

  PRINT("\nTimerEnd received: ", startEvt);

  PRINT("\n Timer Start at : ", timeClient.getEpochTime());

  return (startEvt);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Serial Print Time and Date Function
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void timeDisplay() {
// Populate myTime String
  myTime=timeClient.getFormattedTime();
  myTime= (" " + myTime + "               ");
//#  Serial.println("\nmyTime: " + myTime);


int myTime_len = myTime.length() + 1;
char myTime_charArray[myTime_len];
myTime.toCharArray(myTime_charArray, myTime_len);

//# PRINT("myTime_charArray: ",myTime_charArray);
//Serial.println(myTime_charArray);
//scrollText(myTime_charArray);
}

void dateDisplay() {
uint8_t myMonth = timeClient.getFormattedDate().substring(5,7).toInt();

   // Populate myDate Strin
myDate= dayShortStr(timeClient.getDay()+1) ;
myDate= myDate + "  " + timeClient.getFormattedDate().substring(8,10) + " " +monthShortStr(myMonth) + " " +  timeClient.getFormattedDate().substring(0,4) ;
myDate= (" " + myDate + "               ");
PRINT("\nmyDate: " , myDate);

// covert from string/float to char array
int myDate_len = myDate.length() + 1;
char myDate_charArray[myDate_len];
myDate.toCharArray(myDate_charArray, myDate_len);


PRINT("\nmyDate_charArray: ",myDate_charArray );
//scrollText(myDate_charArray);
}
