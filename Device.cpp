// Device.cpp
#include "Device.h"
#include "Motor.h"

String Device::usedNames[10];
int Device::usedNamesCount = 0;
int Device::usedPins[20] = {0};
int Device::usedPinsCount = 0;
time_t Device::ntpSeconds = 0;
unsigned long Device::ntpMillis = 0;
uint8_t Device::clientNum = 0;
String deviceTypeToString(DeviceType type) {
  switch (type) {
    case DeviceType::MOTOR: return "MOTOR";
    case DeviceType::TANK: return "TANK";
    case DeviceType::VALVE: return "VALVE";
    case DeviceType::OTHER: return "OTHER";
    default: return "UNKNOWN";
  }
}

void Device::updateNtpTime(time_t seconds) {
  ntpSeconds = seconds;
  ntpMillis = millis();
}

uint64_t Device::getCurrentUtcMillis() {
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis >= ntpMillis
      ? currentMillis - ntpMillis
      : (0xFFFFFFFF - ntpMillis) + currentMillis + 1;
  return (uint64_t)ntpSeconds * 1000 + elapsedMillis;
}

Device::Device(String deviceName, int mainPin, int btnPin, bool activeHigh)
  : pin(mainPin), buttonPin(btnPin), byButton(false), isActive(false),
    timeOn(0), timeOff(0), lastDebounceTime(0), lastButtonState(false),
    autoTimeOff(0), activeHigh(activeHigh) {

  if (deviceName.length() > 32)
    deviceName = deviceName.substring(0, 32);

  name = deviceName;

  if (!isPinAvailable(mainPin)) {
    log("Error", "Main pin already used or invalid: " + String(mainPin));
    pin = -1;
  } else {
    registerPin(mainPin);
  }

  if (btnPin != -1) {
    if (!isPinAvailable(btnPin)) {
      log("Error", "Button pin already used or invalid: " + String(btnPin));
      buttonPin = -1;
    } else {
      registerPin(btnPin);
    }
  }

  if (usedNamesCount >= 10) {
    log("Error", "Too many devices, cannot register name");
    name += "_ERROR";
    return;
  }

  for (int i = 0; i < usedNamesCount; i++) {
    if (usedNames[i] == name) {
      name += "_" + String(usedNamesCount);
      log("Warning", "Duplicate name, renamed to " + name);
    }
  }
  usedNames[usedNamesCount++] = name;
}

bool Device::isPinAvailable(int testPin) const {
  if (testPin < 0 || testPin > 39) return false;
  for (int i = 0; i < usedPinsCount; i++)
    if (usedPins[i] == testPin) return false;
  return true;
}

void Device::registerPin(int validPin) {
  if (usedPinsCount < 20)
    usedPins[usedPinsCount++] = validPin;
}

String Device::formatTime(uint64_t ms) const {
  if (ms == 0) return "";
  time_t seconds = ms / 1000;
  unsigned int millisPart = ms % 1000;
  char buf[24];
  struct tm* tm = gmtime(&seconds);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
  snprintf(buf + 19, sizeof(buf) - 19, ".%03u UTC", millisPart);
  return String(buf);
}

void Device::log(const char* action, const String& details) const {
  char msg[128];
  snprintf(msg, sizeof(msg), "[%s] %s%s%s", name.c_str(), action,
           details.length() ? ": " : "", details.c_str());
  Serial.println(msg);
}

bool Device::validateMilliseconds(const String& param, unsigned long& milliseconds) const {
  if (param.length() == 0) {
    log("Error", "Missing milliseconds");
    return false;
  }
  char* end;
  milliseconds = strtoul(param.c_str(), &end, 10);
  if (*end != '\0') {
    log("Error", "Invalid milliseconds: " + param);
    return false;
  }
  return true;
}

void Device::begin() {
  if (pin != -1)
    pinMode(pin, OUTPUT);
  if (buttonPin != -1)
    pinMode(buttonPin, INPUT_PULLUP);
  timeOn = getCurrentUtcMillis();
  StaticJsonDocument<64> extra;
  //log("begin", name);
  
  extra["pin"] = pin;
  fileLog("debug", "Initialized", extra.as<JsonObject>(), false); 
  off();
}

void Device::on(unsigned long duration) {
  if (pin > 0){
    digitalWrite(pin, activeHigh ? HIGH : LOW);
  } 
  duration_ms = duration;
  isActive = true;
  timeOn = getCurrentUtcMillis();
  autoTimeOff = duration > 0 ? timeOn + duration : 0;
  StaticJsonDocument<64> extra;
  if (autoTimeOff > 0)
    extra["autoTimeOff"] = autoTimeOff;
  extra["ms"] = duration;
  Motor* motor = static_cast<Motor*>(this);
  float mspml = motor->getMillisecondsPerMl();
  if (mspml>0) {
    extra["ml"] =  static_cast<int>(duration/mspml);
  }
  fileLog("debug", "On", extra.as<JsonObject>(), true); 
}

void Device::off() {
  if (pin > 0) {
    digitalWrite(pin, activeHigh ? LOW : HIGH);
  }
  
  unsigned long duration = getActiveDuration();
  isActive = false;
  timeOff = getCurrentUtcMillis();
  StaticJsonDocument<64> extra;
  if(duration > 0){
    extra["ms"] = duration;
    Motor* motor = static_cast<Motor*>(this);
    float mspml = motor->getMillisecondsPerMl();
    if (mspml>0) {
        extra["ml"] =  static_cast<int>(duration/mspml);
    }
  }
  fileLog("debug", "Off", extra.as<JsonObject>(), true);
}

void Device::checkButton() {
  if (buttonPin == -1) return;
  bool currentButtonState = digitalRead(buttonPin) == LOW;
  if (currentButtonState != lastButtonState)
    lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (currentButtonState && !isActive) {
      byButton = true;
      on();
    } else if (!currentButtonState && isActive && byButton) {
      byButton = false;
      off();
    }
  }
  lastButtonState = currentButtonState;
}

void Device::update() {
  checkButton();
  if (!isActive || autoTimeOff == 0) return;
  uint64_t now = getCurrentUtcMillis();
  if (now >= autoTimeOff) {
    off();
    //log("Auto-off triggered");
  }
}

bool Device::isDeviceActive() const { return isActive; }
void Device::setDeviceActive(bool active) { isActive = active; }
int Device::getPin() const { return pin; }
int Device::getButtonPin() const { return buttonPin; }
String Device::getName() const { return name; }
String Device::getTimeOn() const { return formatTime(timeOn); }
String Device::getTimeOff() const { return formatTime(timeOff); }
String Device::getAutoTimeOff() const { return formatTime(autoTimeOff); }

unsigned long Device::getActiveDuration() const {
  if (!isActive) return 0;
  uint64_t now = getCurrentUtcMillis();
  return (now - timeOn);
}
unsigned long Device::getDurationMs() const {
  if (!isActive) return 0;
  return duration_ms;
}

String Device::getStatus() const {
  return "active=" + String(isActive ? "Yes" : "No") +
         ", activeDuration=" + String(getActiveDuration()) + " ms";
}

void Device::setContext(JsonObject& context) const {
    //context["now"] = formatTime(getCurrentUtcMillis());
    //context["timeOn"] = formatTime(timeOn);
    //context["timeOff"] =formatTime(timeOff);
    //context["autoTimeOff"] = formatTime(autoTimeOff);
    //context["activeDuration"] = getActiveDuration();
};
void Device::setPin(int p){
    if (isActive) return;
    pin = p;
    //registerPin(pin);
}

void Device::setButtonPin(int bp){
    if (isActive) return;

    buttonPin = bp;
    //registerPin(buttonPin);
}
void Device::fileLog(const String& type, const String& message, JsonObject extra, bool write) const{
  
  StaticJsonDocument<512> doc;
  doc["time"] = getCurrentUtcMillis();
  doc["type"] = type;
  doc["name"] = name;
  doc["message"] = message;
  if (extra) {
    JsonObject dest = doc.createNestedObject("extra");
    for (JsonPair kv : extra) {
      dest[kv.key()] = kv.value();
    }
  }
  String newLine;
  serializeJson(doc, newLine);
  Serial.println(newLine);
  if (!write) return;
  logBuffer.push_back(newLine);
  
  DynamicJsonDocument status(512);
  status["name"] = getName();
  status["type"] = deviceTypeToString(getDeviceType());
  status["pin"] = getPin();
  status["buttonPin"] = getButtonPin();
  status["active"] = isDeviceActive();

  JsonObject context_status = status.createNestedObject("context");
  setContext(context_status);  // 👈 пусть device сам формирует context
  
  JsonArray logs = status.createNestedArray("logs");
  //std::vector<String> lastLogs = getLastLogs(10);
  //for (const String& line : lastLogs) {
  //  DynamicJsonDocument logDoc(256);
  //  DeserializationError err = deserializeJson(logDoc, line);
  //  if (!err) {
  //    //logDoc["time_str"] = device->formatTime(logDoc["time"]);
  //    logs.add(logDoc.as<JsonObject>());
  //  }
  //}

  
  //doc["status"] = status;
  String socketStr;
  //serializeJson(doc, socketStr);
  serializeJson(status, socketStr);
  //webSocket.sendTXT(clientNum, socketStr);
  uint64_t now = getCurrentUtcMillis();
  if (now - lastWriteTime >= SAVE_INTERVAL_MS) {
    flushLogs();
  }
}
std::vector<String> Device::getLastLogs(int count) const {
  std::vector<String> allLines;
  String path = "/" + name + ".log";

  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, FILE_READ);
    while (file && file.available()) {
      allLines.push_back(file.readStringUntil('\n'));
    }
    file.close();
  }

  // Добавим буфер
  allLines.insert(allLines.end(), logBuffer.begin(), logBuffer.end());

  // Вернём последние `count` строк в обратном порядке
  std::vector<String> result;
  int start = allLines.size() > count ? allLines.size() - count : 0;
  for (int i = allLines.size() - 1; i >= start; --i) {
    result.push_back(allLines[i]);
  }

  return result;
}
void Device::flushLogs() const {
  if (logBuffer.empty()) return;

  String path = "/" + name + ".log";
  std::vector<String> lines;

  // Читаем существующие строки
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, FILE_READ);
    while (file && file.available()) {
      lines.push_back(file.readStringUntil('\n'));
    }
    file.close();
  }

  // Добавляем из буфера
  lines.insert(lines.end(), logBuffer.begin(), logBuffer.end());

  // Обрезаем если больше 100
  while (lines.size() > 100) lines.erase(lines.begin());

  // Записываем
  File file = LittleFS.open(path, FILE_WRITE);
  for (const String& line : lines) {
    file.println(line);
  }
  file.close();

  logBuffer.clear();
  lastWriteTime = getCurrentUtcMillis();
  StaticJsonDocument<1> dummyDoc;
  fileLog("debug", "log updated", dummyDoc.to<JsonObject>(), false); 
}
