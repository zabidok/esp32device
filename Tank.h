#ifndef TANK_H
#define TANK_H

#include <Device.h>
#include <Preferences.h>

extern Preferences prefs;

class Tank : public Device {
protected:
  int capacity;
  float currentLevel;
  uint64_t lastSaveTime = 0;      // Время последней записи
  float lastSavedLevel = -1;  // -1 означает, что ничего ещё не сохранено
  float millisecondsPerMl = 0;
  int in = 0;
public:
  Tank(String deviceName, int tankCapacity) : Device(deviceName, -1), capacity(tankCapacity), currentLevel(0) {}
  void begin() override {
    currentLevel = prefs.getFloat((getName() + "_level").c_str(), 0);
    lastSavedLevel = currentLevel;
    lastSaveTime = Device::getCurrentUtcMillis();
    Device::begin();
  }
  DeviceType getDeviceType() const override { return DeviceType::TANK; }
  void setIn(bool in_t) {
    in = in_t ? 1 : 0;
  }
  void setMillisecondsPerMl(float msPerMl) {
    if (msPerMl > 0) {
      millisecondsPerMl = msPerMl;
      //StaticJsonDocument<1> dummyDoc;
      //fileLog("debug", "set millisecondsPerMl="+String(millisecondsPerMl), dummyDoc.to<JsonObject>(), true); 
    } else {
      log("Error", "Invalid msPerMl: " + String(msPerMl));
    }
  }
  float getActiveDurationMl() const {
    if (!isActive) return 0;
    unsigned long duration_ms = getActiveDuration();
    float ml = duration_ms / millisecondsPerMl;
    if(in){
      ml = -ml;
    }
    return roundf(ml * 100) / 100.0f;
  }
  float getDurationMl() const {
    if (!isActive) return 0;
    unsigned long duration_ms = getDurationMs();
    float ml = duration_ms / millisecondsPerMl;

    // Округлим до 2 знаков после запятой
    return roundf(ml * 100) / 100.0f;
  }
  int getCapacity() const { return capacity; }
  void setCapacity(int c) {
    capacity = c;
    //StaticJsonDocument<64> extra;
    //extra["ml"] = capacity;
    //fileLog("debug", "set capacity", extra.as<JsonObject>(), true); 
  }
  float getCurrentLevel() const {
    return currentLevel; 
  }
  void setCurrentLevel(float ml) { 
    if (ml != currentLevel) {
      currentLevel = ml;
      prefs.putFloat((getName() + "_level").c_str(), currentLevel);
      lastSavedLevel = currentLevel;
      lastSaveTime = getCurrentUtcMillis();
    }
    //StaticJsonDocument<64> extra;
    //extra["ml"] = currentLevel;
    //fileLog("debug", "set currentLevel", extra.as<JsonObject>(), true); 
  }
  void fill(float amount) {
    if (amount <= 0) {
      return;
    }
    currentLevel += amount;
    //StaticJsonDocument<64> extra;
    //extra["ml"] = amount;
    //extra["currentLevel"] = currentLevel;
    //fileLog("debug", "Fill", extra.as<JsonObject>(), true);
  }

  void drain(float amount) {
    if (amount <= 0) {
      return;
    }
    currentLevel -= amount;
    if (currentLevel < 0) {
      //currentLevel = 0;
    }
    //prefs.putInt((getName() + "_level").c_str(), currentLevel);
    //StaticJsonDocument<64> extra;
    //extra["ml"] = amount;
    //extra["currentLevel"] = currentLevel;
    //fileLog("debug", "Drain", extra.as<JsonObject>(), true); 
  }

  bool handleCommand(const String& cmd, const String& param) override {
    if (cmd == "T_FILL") {
      fill(param.toInt());
      return true;
    } else if (cmd == "T_DRAIN") {
      drain(param.toInt());
      return true;
    } else if (cmd == "T_SET_LEVEL") {
       setCurrentLevel(param.toInt());
       return true;
    }
    log("Error", "Unknown command: " + cmd);
    return false;
  }
  String getStatus() const override {
    return Device::getStatus() + ", currentLevel=" + String(currentLevel) + ", capacity=" + String(capacity);
  }

  void update() override {
    
    Device::update();
    uint64_t now = getCurrentUtcMillis();
    
    if (currentLevel != lastSavedLevel && now - lastSaveTime >= SAVE_INTERVAL_MS) {
      prefs.putInt((getName() + "_level").c_str(), currentLevel);
      lastSavedLevel = currentLevel;
      lastSaveTime = now;
      log("Level saved", "currentLevel=" + String(currentLevel));
    }
  }
  void on(unsigned long duration = 0) override {
    Device::on(duration);
  }
  void off() override{
    isActive = false;
    Device::off();
  }
  void setContext(JsonObject& context) const override {
    Device::setContext(context);
    context["capacity"] = getCapacity();
    context["currentLevel"] = getCurrentLevel();
  }
};

#endif
