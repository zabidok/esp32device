#ifndef MOTOR_H
#define MOTOR_H

#include <Device.h>
#include <Tank.h>
#include <Valve.h>

class Motor : public Device {
protected:
  float millisecondsPerMl;
  Tank* inTank; // Опциональный танк для входа (drain)
  Tank* outTank; // Опциональный танк для выхода (fill)
  
  Valve* inValve; // Опциональный входной клапан
  Valve* outValve;// Опциональный выходной клапан
  
  bool validateMilliliters(const String& param, float& milliliters) {
    if (param.isEmpty()) {
      log("Error", "Missing milliliters");
      return false;
    }
    char* end;
    milliliters = strtof(param.c_str(), &end);
    if (*end != '\0' || milliliters <= 0) {
      log("Error", "Invalid milliliters: " + param);
      return false;
    }
    return true;
  }

public:
  Motor(String deviceName, int pin, float msPerMl, int btnPin = -1, Tank* in = nullptr, Tank* out = nullptr, Valve* inV = nullptr, Valve* outV = nullptr)
        : Device(deviceName, pin, btnPin), millisecondsPerMl(msPerMl), inTank(in), outTank(out), inValve(inV), outValve(outV) {if (msPerMl <= 0) {
      log("Error", "Invalid msPerMl: " + String(msPerMl));
      millisecondsPerMl = 1.0;
    }
    if (inTank) {
      inTank->setMillisecondsPerMl(millisecondsPerMl);
      inTank->setIn(true); // Устанавливаем флаг входа
    }
    
    if (outTank) {
      outTank->setMillisecondsPerMl(millisecondsPerMl);
      outTank->setIn(false); // Устанавливаем флаг входа
    }
    if (inValve) {
      inValve->getOut1()->setMillisecondsPerMl(millisecondsPerMl);
      inValve->getOut1()->setIn(true);
      inValve->getOut2()->setMillisecondsPerMl(millisecondsPerMl);
      inValve->getOut2()->setIn(true);
      inValve->motor_name = deviceName;
      inValve->in = true;
      setInTank(inValve->getOut1());
    }
    if (outValve) {
      outValve->getOut1()->setMillisecondsPerMl(millisecondsPerMl);
      outValve->getOut2()->setMillisecondsPerMl(millisecondsPerMl);
      outValve->motor_name = deviceName;
      setOutTank(outValve->getOut1());
    }
  }
  float getActiveDurationMl() const {
    if (!isActive) return 0;
    unsigned long duration_ms = getActiveDuration();
    float ml = duration_ms / millisecondsPerMl;
    return roundf(ml * 100) / 100.0f;
  }
  float getDurationMl() const {
    if (!isActive) return 0;
    unsigned long duration_ms = getDurationMs();
    float ml = duration_ms / millisecondsPerMl;

    // Округлим до 2 знаков после запятой
    return roundf(ml * 100) / 100.0f;
  }
  DeviceType getDeviceType() const override { return DeviceType::MOTOR; }
  Tank* getInTank() const { 
    if(inValve) {
      return inValve->getActiveTank();
    }
    return inTank; 
  }
  Tank* getOutTank() const { 
    if(outValve) {
      return outValve->getActiveTank();
    }
    return outTank; 
  }
  Valve* getInValve() const { return inValve; }
  Valve* getOutValve() const { return outValve; }
  void setInValve(Valve* valve) {
    inValve = valve;
    if (inValve) {
      setInTank(inValve->getOut1());
    }
  }
  void setOutValve(Valve* valve) {
    outValve = valve;
    if (outValve) {
      setOutTank(outValve->getOut1());
    }
  }
  void setInTank(Tank* tank) {
    tank->setMillisecondsPerMl(millisecondsPerMl);
    tank->setIn(true);
    inTank = tank;
    //StaticJsonDocument<1> dummyDoc;
    //fileLog("debug", "set inTank="+inTank->getName(), dummyDoc.to<JsonObject>(), true); 
  }
  void setOutTank(Tank* tank) {
    tank->setMillisecondsPerMl(millisecondsPerMl);
    outTank = tank;
    //StaticJsonDocument<1> dummyDoc;
    //fileLog("debug", "set outTank="+outTank->getName(), dummyDoc.to<JsonObject>(), true); 
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

  float getMillisecondsPerMl() const { return millisecondsPerMl; }

  void dispense(float milliliters) {
    if (milliliters <= 0 || millisecondsPerMl <= 0) {
      log("Error", "Invalid dispense request");
      return;
    }
    unsigned long duration = (unsigned long)(milliliters * millisecondsPerMl);
    if (duration > 0xFFFFFFF0) {
      log("Error", "Duration too large: " + String(duration));
      return;
    }
    on(duration);
  }

  bool handleCommand(const String& cmd, const String& param) override {
    if (cmd == "M_ON") {
      unsigned long milliseconds;
      if (!validateMilliseconds(param, milliseconds)) return false;
      on(milliseconds);
      return true;
    } else if (cmd == "M_OFF") {
      off();
      return true;
    } else if (cmd == "M_DISPENSE") {
      float milliliters;
      if (!validateMilliliters(param, milliliters)) return false;
      dispense(milliliters);
      return true;
    } else if (cmd == "M_SET_MSPERML") {
      float msPerMl = param.toFloat();
      if (msPerMl <= 0) {
        log("Error", "Invalid msPerMl: " + param);
        return false;
      }
      setMillisecondsPerMl(msPerMl);
      log("Set msPerMl", String(msPerMl));
      return true;
    } else if (cmd == "M_STATUS") {
      log("Status", "active=" + String(isActive ? "Yes" : "No") +
                    ", msPerMl=" + String(millisecondsPerMl) +
                    ", timeOn=" + formatTime(timeOn) +
                    ", timeOff=" + formatTime(timeOff) +
                    ", activeDuration=" + String(getActiveDuration()) + " ms");
      return true;
    }
    log("Error", "Unknown command: " + cmd);
    return false;
  }

  String getStatus() const override {
    return Device::getStatus() + ", msPerMl=" + String(millisecondsPerMl);
  }

  void on(unsigned long duration = 0) override {
    
    if (getInTank()) {
      //inTank->setDeviceActive(true);
      getInTank()->on();
    }
    if (getOutTank()) {
      getOutTank()->on();
    }
    Device::on(duration);



    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject md = arr.createNestedObject();
    md["n"] = name;
    md["t"] = "MOTOR";
    md["a"] = true;

    const auto duration_s = getDurationMs();
    const auto activeDur = getActiveDuration();
    md["dms"] = duration_s;
    md["ams"] = activeDur;
    md["lms"] = (duration_s > 0) ? (duration_s - activeDur) : 0;

    md["it"] = getInTank() ? getInTank()->getName() : "";
    md["ot"] = getOutTank() ? getOutTank()->getName() : "";

    const float d_ml = getDurationMl();
    const float ad_ml = getActiveDurationMl();
    md["dml"] = d_ml;
    md["aml"] = ad_ml;
    md["lml"] = (d_ml > 0) ? (d_ml - ad_ml) : 0;

    if (getInTank()) {
      JsonObject it = arr.createNestedObject();
      it["n"] = getInTank()->getName();
      it["t"] = "TANK";
      it["a"] = true;
      it["cl"] = getInTank()->getCurrentLevel() + getInTank()->getActiveDurationMl();
    }
    if (getOutTank()) {
      JsonObject ot = arr.createNestedObject();
      ot["n"] = getOutTank()->getName();
      ot["a"] = true;
      ot["t"] = "TANK";
      ot["cl"] = getOutTank()->getCurrentLevel() + getOutTank()->getActiveDurationMl();
    }
    String socketStr;
    serializeJson(doc, socketStr);
    webSocket.sendTXT(clientNum, socketStr);
  }

  void off() override{
    float amount = getActiveDuration() / millisecondsPerMl;
    amount = roundf(amount * 100) / 100.0;
    const auto duration = getActiveDuration();
    const float d_ml = getActiveDuration() / millisecondsPerMl;
    Device::off();

    if (getInTank()) {
      getInTank()->drain(amount);
      getInTank()->setDeviceActive(false);
      getInTank()->off();
      //inTank->update();
    }
    if (getOutTank()) {
      getOutTank()->fill(amount);
      getOutTank()->setDeviceActive(false);
      getOutTank()->off();
      //outTank->update();
    }
    lastUpdateTime = 0;
    
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject md = arr.createNestedObject();
    md["n"] = name;
    md["t"] = "MOTOR";

    const auto duration_s = getDurationMs();
    const auto activeDur = getActiveDuration();
    md["dms"] = duration_s;
    md["ams"] = activeDur;
    md["lms"] = (duration_s > 0) ? (duration_s - activeDur) : 0;

    md["it"] = getInTank() ? getInTank()->getName() : "";
    md["ot"] = getOutTank() ? getOutTank()->getName() : "";

    const float d_ml1 = getDurationMl();
    const float ad_ml = getActiveDurationMl();
    md["dml"] = d_ml1;
    md["aml"] = ad_ml;
    md["lml"] = (d_ml1 > 0) ? (d_ml1 - ad_ml) : 0;

    if (getInTank()) {
      JsonObject it = arr.createNestedObject();
      it["n"] = getInTank()->getName();
      it["t"] = "TANK";
      it["cl"] = getInTank()->getCurrentLevel() + getInTank()->getActiveDurationMl();
    }
    if (getOutTank()) {
      JsonObject ot = arr.createNestedObject();
      ot["n"] = getOutTank()->getName();
      ot["t"] = "TANK";
      ot["cl"] = getOutTank()->getCurrentLevel() + getOutTank()->getActiveDurationMl();
    }
    String socketStr;
    serializeJson(doc, socketStr);
    webSocket.sendTXT(clientNum, socketStr);
  }

  void update() override {
    Device::update();
  }
  
  void setContext(JsonObject& context) const override{
    Device::setContext(context);
    context["now"] = formatTime(getCurrentUtcMillis());
    context["timeOn"] = formatTime(timeOn);
    context["timeOff"] =formatTime(timeOff);
    context["autoTimeOff"] = formatTime(autoTimeOff);
    context["activeDuration"] = getActiveDuration();
    context["millisecondsPerMl"] = getMillisecondsPerMl();
    if (inTank) context["inTank"] = inTank->getName();
    if (outTank) context["outTank"] = outTank->getName();
  }
  private:
    uint64_t lastUpdateTime = 0; // Время последнего обновления
    int calculateAmount() {
      if (!isActive) {
        lastUpdateTime = 0;
        return 0;
      }

      uint64_t now = getCurrentUtcMillis();
      if (lastUpdateTime == 0) {
        lastUpdateTime = now;
        return 0; // Первое обновление, пока не считаем
      }

      unsigned long elapsed = now - lastUpdateTime; // Время с последнего обновления
      lastUpdateTime = now; // Обновляем время

      // Вычисляем объем с учетом millisecondsPerMl
      float amount = static_cast<float>(elapsed) / millisecondsPerMl;
      return static_cast<int>(amount); // Округляем до целого
    }
};

#endif
