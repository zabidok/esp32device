#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Device.h>

#include <map>
#include <WebSocketsServer.h>

extern WebSocketsServer webSocket;

class DeviceManager {
private:
  Device** devices;
  int numDevices;
  std::map<String, Device*> deviceMap;
  std::vector<Device*> devicesList;

  bool parseCommand(String& command, String& cmd, String& deviceName, String& param) {
    const char* ERR_NO_DEVICE_NAME = "Error: Command must contain device name";
    command.trim();
    char buf[65];
    strncpy(buf, command.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char cmdBuf[16], nameBuf[32], paramBuf[32];
    if (sscanf(buf, "%15s %31s %31s", cmdBuf, nameBuf, paramBuf) < 2) {
      Serial.println(ERR_NO_DEVICE_NAME);
      return false;
    }
    cmd = cmdBuf;
    deviceName = nameBuf;
    param = paramBuf;
    return true;
  }

  void loadConfig(const char* jsonConfig){
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, jsonConfig);
    if (err) {
      Serial.println("Error parsing config: " + String(err.c_str()));
      return;
    }
    for (JsonObject obj : doc.as<JsonArray>()) {
    String type = obj["type"];
    String name = obj["name"];

    if (type == "TANK") {
      int capacity = obj["capacity"] | 0;
      float level = obj["currentLevel"] | 0;
      Tank* tank = new Tank(name, capacity);
      tank->setCurrentLevel(level);
      devicesList.push_back(tank);
    } else if (type == "VALVE") {
      int pin = obj["pin"];
      int btnPin = obj["btnPin"] | -1;
      String out1Name = obj["out1"] | "";
      String out2Name = obj["out2"] | "";
      Tank* out1 = nullptr;
      Tank* out2 = nullptr;
      for (Device* d : devicesList) {
        if (d->getDeviceType() == DeviceType::TANK) {
          if (d->getName() == out1Name) out1 = static_cast<Tank*>(d);
          if (d->getName() == out2Name) out2 = static_cast<Tank*>(d);
        }
      }
      Valve* valve = new Valve(name, pin, out1, out2);
      devicesList.push_back(valve);
    } else if (type == "MOTOR") {
      int pin = obj["pin"];
      float msPerMl = obj["millisecondsPerMl"];
      int btnPin = obj["btnPin"] | -1;
      String inTankName = obj["inTank"] | "";
      String outTankName = obj["outTank"] | "";
      Tank* inTank = nullptr;
      Tank* outTank = nullptr;
      for (Device* d : devicesList) {
        if (d->getDeviceType() == DeviceType::TANK) {
          if (d->getName() == inTankName) inTank = static_cast<Tank*>(d);
          if (d->getName() == outTankName) outTank = static_cast<Tank*>(d);
        }
      }


      String inValveName = obj["inValve"] | "";
      String outValveName = obj["outValve"] | "";
      Valve* inValve = nullptr;
      Valve* outValve = nullptr;
      for (Device* d : devicesList) {
        if (d->getDeviceType() == DeviceType::VALVE) {
          if (d->getName() == inValveName) inValve = static_cast<Valve*>(d);
          if (d->getName() == outValveName) outValve = static_cast<Valve*>(d);
        }
      }
      Motor* motor = new Motor(name, pin, msPerMl, btnPin, inTank, outTank, inValve, outValve);
      devicesList.push_back(motor);
    }
  }
  devices = devicesList.data();
  numDevices = devicesList.size();
  }
public:

  DeviceManager(const char* jsonConfig){
    loadConfig(jsonConfig);
    for (int i = 0; i < numDevices; i++) {
      deviceMap[devices[i]->getName()] = devices[i];
    }
  }
  uint8_t clientNum = 0;
  void init() {
    printHelp();
    for (int i = 0; i < numDevices; i++) {
      devices[i]->begin();
    }
  }
  StaticJsonDocument<4096> deviceJsonStatic(bool activeOnly) {
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < numDevices; i++) {
      if(activeOnly && !devices[i]->isDeviceActive()) continue;
      Device* device = devices[i];
      JsonObject jsonDevice = arr.createNestedObject();
      bool active = device->isDeviceActive();
      jsonDevice["n"] = device->getName();
      jsonDevice["t"] = deviceTypeToString(device->getDeviceType());
      jsonDevice["a"] = active;
      if(!activeOnly){
        int pin = device->getPin();
        int bp = device->getButtonPin();
        if (pin >= 0) jsonDevice["p"] = pin;
        if (bp >= 0) jsonDevice["bp"] = bp;
      }
      if (active) {
        const auto duration = device->getDurationMs();
        const auto activeDur = device->getActiveDuration();
        jsonDevice["dms"] = duration;
        jsonDevice["ams"] = activeDur;
        jsonDevice["lms"] = (duration > 0) ? (duration - activeDur) : 0;
      }
      
      if (device->getDeviceType() == DeviceType::MOTOR) {
        Motor* motor = static_cast<Motor*>(device);
        String it = motor->getInTank() ? motor->getInTank()->getName() : "";
        String ot = motor->getOutTank() ? motor->getOutTank()->getName() : "";
        //if (motor->getInValve()) {
          //it=motor->getInValve()->getActiveTank()->getName();
        //}
        //if (motor->getOutValve()) {
          //ot=motor->getOutValve()->getActiveTank()->getName();
          //ot = motor->getOutValve()->getActiveTank() ? motor->getOutValve()->getActiveTank()->getName() : ot;
        //}
        //jsonDevice["it"] = motor->getInValve() ? motor->getInValve()->getActiveTank()->getName() : motor->getInTank() ? motor->getInTank()->getName() : "";
        //jsonDevice["ot"] = motor->getOutValve() ? motor->getOutValve()->getActiveTank()->getName() : motor->getOutTank() ? motor->getOutTank()->getName() : "";
        jsonDevice["it"] = it;
        jsonDevice["ot"] = ot;
        if(!activeOnly){
          jsonDevice["mpm"] = motor->getMillisecondsPerMl();
          jsonDevice["iv"] = motor->getInValve() ? motor->getInValve()->getName() : "";
          jsonDevice["ov"] = motor->getOutValve() ? motor->getOutValve()->getName() : "";
        }
        
        if (active) {
          const float d_ml = motor->getDurationMl();
          const float ad_ml = motor->getActiveDurationMl();
          jsonDevice["dml"] = d_ml;
          jsonDevice["aml"] = ad_ml;
          jsonDevice["lml"] = (d_ml > 0) ? (d_ml - ad_ml) : 0;
        }
      } else if (device->getDeviceType() == DeviceType::VALVE) {
        Valve* valve = static_cast<Valve*>(device);
        jsonDevice["at"] = valve->getActiveTank() ? valve->getActiveTank()->getName() : "";
        jsonDevice["o1"] = valve->getOut1() ? valve->getOut1()->getName() : "";
        jsonDevice["o2"] = valve->getOut2() ? valve->getOut2()->getName() : "";
        if(!activeOnly){
          
        }
      } else if (device->getDeviceType() == DeviceType::TANK) {
        Tank* tank = static_cast<Tank*>(device);
        jsonDevice["sc"] = tank->getCapacity();
        if(!activeOnly){
          jsonDevice["c"] = tank->getCapacity();
        }
        if (active) {
          jsonDevice["cl"] = tank->getCurrentLevel() + tank->getActiveDurationMl();
        } else {
          jsonDevice["cl"] = tank->getCurrentLevel();
        }
      }
    }
    return doc;
  }

  void sendSocketDevices(bool activeOnly = false) {
    StaticJsonDocument<4096> doc = deviceJsonStatic(activeOnly);
    String socketStr;
    serializeJson(doc, socketStr);
    webSocket.broadcastTXT(socketStr);
  }

  
  void printHelp() {
    Serial.println("Allowed commands:");
    Serial.println("M_ON <device> <milliseconds> - Turn on mortor for specified time");
    Serial.println("M_OFF <device> - Turn off motor");
    Serial.println("M_DISPENSE <device> <milliliters> - Dispense specified volume");
    Serial.println("M_SET_MSPERML <device> <msPerMl> - Set ms/ml");
    Serial.println("M_STATUS <device> - Show motor status");
    Serial.println("T_FILL <device> <milliliters> - Fill tank");
    Serial.println("T_DRAIN <device> <milliliters> - Drain tank");
    Serial.println("T_SET_LEVEL <device> <milliliters> - Set current level ml in tank");
  }
  Device* findByName(const String& name) {
    auto it = deviceMap.find(name);
    return (it != deviceMap.end()) ? it->second : nullptr;
  }
  void runPol() {
    Motor* motor = static_cast<Motor*>(findByName("motor"));
    Valve* inValve = motor->getInValve();
    Valve* outValve = motor->getOutValve();
    //andrey
    int ml = 300;
    int ms = ml * motor->getMillisecondsPerMl();
    inValve->on();
    outValve->off();
    motor->on();
    delay(ms); 
    motor->off();
    //Valve* inValve = static_cast<Valve*>(findByName("inValve"));

    delay(200); // Задержка для стабилизации
    inValve->off();
    delay(200);

    //vova
    outValve->on();
    delay(100); // Задержка для стабилизации
    int mlv = 250;
    int msv = mlv * motor->getMillisecondsPerMl();
    motor->on();
    delay(msv); 
    motor->off();
    delay(100);
    inValve->off();
    outValve->off();

  }
  void update() {
    const char* ERR_DEVICE_NOT_FOUND = "Unknown device: %s";
    for (int i = 0; i < numDevices; i++) {
      devices[i]->update();
    }
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      const size_t MAX_COMMAND_LENGTH = 64;
      if (command.length() > MAX_COMMAND_LENGTH) {
        Serial.printf("Error: Command too long: %s\n", command.c_str());
        return;
      }
      String cmd, deviceName, param;
      if (!parseCommand(command, cmd, deviceName, param)) return;
      
      auto it = deviceMap.find(deviceName);
      if (it == deviceMap.end()) {
        Serial.printf(ERR_DEVICE_NOT_FOUND, deviceName.c_str());
        return;
      }
      it->second->handleCommand(cmd, param);
    }
  }
};

#endif
