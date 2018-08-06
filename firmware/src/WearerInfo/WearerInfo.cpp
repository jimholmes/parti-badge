#include "WearerInfo.h"

WearerInfo::WearerInfo() {
  EEPROM.get(EEPROM_SET_LOCATION, _isSet);

  if (_isSet == 1) {
    _firstName = getFromEEPROM(EEPROM_FIRSTNAME_LOCATION);
    _lastName = getFromEEPROM(EEPROM_LASTNAME_LOCATION);
    _twitter = getFromEEPROM(EEPROM_TWITTER_LOCATION);
  }
}

void WearerInfo::setFirstName(String name) {
  char stringBuff[STRING_BUFFER_SIZE];
  stringBuff[(sizeof(stringBuff) - 1)] = 0;
  name.getBytes((unsigned char *)stringBuff, sizeof(stringBuff));

  _firstName = name;
  _isSet = 1;

  EEPROM.put(EEPROM_FIRSTNAME_LOCATION, stringBuff);
  EEPROM.put(EEPROM_SET_LOCATION, _isSet);
}

void WearerInfo::setLastName(String name) {
  char stringBuff[STRING_BUFFER_SIZE];
  stringBuff[(sizeof(stringBuff) - 1)] = 0;
  name.getBytes((unsigned char *)stringBuff, sizeof(stringBuff));

  _lastName = name;
  _isSet = 1;

  EEPROM.put(EEPROM_LASTNAME_LOCATION, stringBuff);
  EEPROM.put(EEPROM_SET_LOCATION, _isSet);
}

void WearerInfo::setTwitter(String name) {
  char stringBuff[STRING_BUFFER_SIZE];
  stringBuff[(sizeof(stringBuff) - 1)] = 0;
  name.getBytes((unsigned char *)stringBuff, sizeof(stringBuff));

  _twitter = name;
  _isSet = 1;

  EEPROM.put(EEPROM_TWITTER_LOCATION, stringBuff);
  EEPROM.put(EEPROM_SET_LOCATION, _isSet);
}

boolean WearerInfo::isSet() {
  return _isSet;
}

String WearerInfo::getFirstName() {
  if (_firstName.length() == 0) {
    _firstName = getFromEEPROM(EEPROM_FIRSTNAME_LOCATION);
  }

  return _firstName;
}

String WearerInfo::getLastName() {
  if (_lastName.length() == 0) {
    _lastName = getFromEEPROM(EEPROM_LASTNAME_LOCATION);
  }

  return _lastName;
}

String WearerInfo::getTwitter() {
  if (_twitter.length() == 0) {
    _twitter = getFromEEPROM(EEPROM_TWITTER_LOCATION);
  }

  return _twitter;
}

String WearerInfo::getFromEEPROM(int address) {
  char value[STRING_BUFFER_SIZE];

  EEPROM.get(address, value);
  value[sizeof(value) - 1] = 0;

  if (value[0] != 255) {
    return String(value);
  } else {
    return String();
  }
}
