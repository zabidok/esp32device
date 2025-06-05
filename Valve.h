#ifndef VALVE_H
#define VALVE_H

#include <Device.h>
#include <Tank.h>



class Valve : public Device {
protected:
    Tank* out1;
    Tank* out2;
    Tank* activeTank;
public:
    Valve(String deviceName, int pin, Tank* out1 = nullptr, Tank* out2 = nullptr)
        : Device(deviceName, pin, -1, true), out1(out1), out2(out2){
          activeTank = out1;
    }
    String motor_name = ""; 
    bool in = false;
    void begin() override {
      Device::begin();
    }
    DeviceType getDeviceType() const override { return DeviceType::VALVE; }

    Tank* getOut1() const { return out1; }
    Tank* getOut2() const { return out2; }
    Tank* getActiveTank() const { return activeTank; }

    void setOut1(Tank* tank) {
        out1 = tank;
        //StaticJsonDocument<64> extra;
        //extra["out1"] = tank ? tank->getName() : "";
        //fileLog("debug", "Установлен out1", extra.as<JsonObject>(), true);
    }

    void setOut2(Tank* tank) {
        out2 = tank;
        //StaticJsonDocument<64> extra;
        //extra["out2"] = tank ? tank->getName() : "";
        //fileLog("debug", "Установлен out2", extra.as<JsonObject>(), true);
    }
    void setActiveTank(Tank* tank) {
        activeTank = tank;
    }
    void on(unsigned long duration = 0) override {
      setActiveTank(getOut2());
      Device::on(duration);
    }

    void off() override {
      setActiveTank(getOut1());
      Device::off();

      StaticJsonDocument<4096> doc;
      JsonArray arr = doc.to<JsonArray>();
      JsonObject vd = arr.createNestedObject();
      vd["n"] = name;
      vd["t"] = "VALVE";
      vd["at"] = getActiveTank() ? getActiveTank()->getName() : "";
      vd["o1"] = getOut1() ? getOut1()->getName() : "";
      vd["o2"] = getOut2() ? getOut2()->getName() : "";
      JsonObject vm = arr.createNestedObject();
      vm["n"] = motor_name;
      vm["t"] = "MOTOR";
      vm[(in ? "it" : "ot")] = getActiveTank() ? getActiveTank()->getName() : "";

      String socketStr;
      serializeJson(doc, socketStr);
      webSocket.sendTXT(clientNum, socketStr);

        //digitalWrite(pin, LOW); // Выключение клапана (out1 активен)
        //StaticJsonDocument<4096> doc;
        //JsonArray arr = doc.to<JsonArray>();
        //JsonObject vd = arr.createNestedObject();
        //vd["n"] = name;
        //vd["t"] = "VALVE";
        //vd["a"] = false;
        //vd["dms"] = getDurationMs();
        //vd["ams"] = getActiveDuration();
        //vd["lms"] = 0;
        //if (out1) {
        //    JsonObject ot1 = arr.createNestedObject();
        //    ot1["n"] = out1->getName();
        //    ot1["t"] = "TANK";
        //    ot1["c"] = out1->getCapacity();
        //    ot1["cl"] = out1->getCurrentLevel();
        //}
        //String socketStr;
        //serializeJson(doc, socketStr);
        //webSocket.sendTXT(clientNum, socketStr);
    }

    bool handleCommand(const String& cmd, const String& param) override {
        if (cmd == "V_ON") {
            unsigned long milliseconds;
            if (!validateMilliseconds(param, milliseconds)) return false;
            on(milliseconds);
            return true;
        } else if (cmd == "V_OFF") {
            off();
            return true;
        } else if (cmd == "V_STATUS") {
            log("Status", "active=" + String(isActive ? "Yes" : "No") +
                          ", out1=" + (out1 ? out1->getName() : "None") +
                          ", out2=" + (out2 ? out2->getName() : "None") +
                          ", timeOn=" + formatTime(timeOn) +
                          ", timeOff=" + formatTime(timeOff));
            return true;
        }
        log("Error", "Неизвестная команда: " + cmd);
        return false;
    }
};

#endif