// Device.h
#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>
#include <vector>
#include <WebSocketsServer.h>

extern WebSocketsServer webSocket;

enum class DeviceType { MOTOR, TANK, VALVE, OTHER };

String deviceTypeToString(DeviceType type);

class Device {
protected:
  String name;
  int pin;
  int buttonPin;
  bool byButton;
  bool isActive;
  uint64_t timeOn;
  uint64_t timeOff;
  unsigned long lastDebounceTime;
  bool lastButtonState;
  uint64_t autoTimeOff;
  uint64_t duration_ms;
  bool activeHigh;

  static const unsigned long DEBOUNCE_DELAY = 50;
  static String usedNames[10];
  static int usedNamesCount;
  static int usedPins[20];
  static int usedPinsCount;
  static time_t ntpSeconds;
  static unsigned long ntpMillis;


  void log(const char* action, const String& details = "") const;
  bool validateMilliseconds(const String& param, unsigned long& milliseconds) const;
  bool isPinAvailable(int testPin) const;
  void registerPin(int validPin);

  mutable std::vector<String> logBuffer;
  mutable uint64_t lastWriteTime = 0;
  static const unsigned long SAVE_INTERVAL_MS = 2 * 60 * 60 * 1000; // 2 часа
  void flushLogs() const;
public:
  String formatTime(uint64_t ms) const;
  static void updateNtpTime(time_t seconds);
  static uint64_t getCurrentUtcMillis();
  static uint8_t clientNum;
  Device(String deviceName, int mainPin, int btnPin = -1, bool activeHigh = true);
  virtual ~Device() {}

  virtual DeviceType getDeviceType() const = 0;

  virtual void begin();
  virtual void on(unsigned long duration = 0);
  virtual void off();
  virtual void update();

  void checkButton();

  bool isDeviceActive() const;
  void setDeviceActive(bool active);
  int getPin() const;
  void setPin(int pin);
  int getButtonPin() const;
  void setButtonPin(int buttonPin);
  String getName() const;
  String getTimeOn() const;
  String getTimeOff() const;
  String getAutoTimeOff() const;
  virtual void setContext(JsonObject& context) const;
  unsigned long getActiveDuration() const;
  unsigned long getDurationMs() const;
  virtual String getStatus() const;
  virtual void fileLog(const String& type, const String& message, JsonObject extra, bool skipWrite) const;
  std::vector<String> getLastLogs(int count) const;
  virtual bool handleCommand(const String& cmd, const String& param) = 0;
};

#endif
