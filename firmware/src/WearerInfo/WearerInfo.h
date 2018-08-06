#ifndef Morse_h
#define Morse_h

#include <Particle.h>

#define EEPROM_SET_LOCATION 1
#define EEPROM_FIRSTNAME_LOCATION 50
#define EEPROM_LASTNAME_LOCATION 120
#define EEPROM_TWITTER_LOCATION 190
#define STRING_BUFFER_SIZE 50

class WearerInfo {
  public:
    WearerInfo();

    void setFirstName(String name);
    void setLastName(String name);
    void setTwitter(String name);
    String getFirstName();
    String getLastName();
    String getTwitter();
    boolean isSet();

  private:
    void putToEEPROM(int address, char* value);
    String getFromEEPROM(int address);
    int _isSet = 0;
    String _firstName;
    String _lastName;
    String _twitter;
    char _stringBuff[STRING_BUFFER_SIZE];
};

#endif
