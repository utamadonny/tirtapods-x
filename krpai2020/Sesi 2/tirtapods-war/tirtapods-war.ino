#include "ping.h"
#include "proxy.h"
#include "flame.h"
#include "legs.h"
#include "pump.h"
#include "activation.h"
#include "lcd.h"
#include "line.h"

bool state_isInversed = true;
bool state_isInitialized = false;
unsigned int state_startTime = 0;
unsigned int state_lastSWR = 0;

int CurrentState = 0;
int CounterRead = 0;
int CounterGlue = 0;
int CounterFloor = 0;

bool avoidWall(bool inverse = false);
bool avoid3Ladder(bool inverse = false);
bool getCloser2SRWR(bool inverse = false);
void traceRoute();
void traceRouteInversed();

void setup () {
  ping::setup();
  proxy::setup();
  legs::setup();
  pump::setup();
  activation::setup();
  lcd::setup();
  line::setup();

  Serial.begin(9600);
}

void loop () {
  // Serial.println (CounterRead);
  activation::update();

  if (activation::isON) {
    if (activation::isLowMove) {
      lcd::justPrint("Path on front     ", "Moving forward + ");
      legs::forward(true);
      return;
    }

    if (!state_isInitialized) {
      state_startTime = millis();
      state_isInversed = ping::checkShouldFollow();
      state_isInitialized = true;
    }

    ping::update();
    proxy::update();
    flame::update();
    line::update();
    //
    //    Serial.println(state_lastSWR);
    //
    //    if ((millis() - state_lastSWR) > 17000) {
    //      Serial.println("called");
    //      state_isInversed = !state_isInversed;
    //    }

    if (state_isInversed) {
      if (ping::isOnSLWR) {
        state_lastSWR = millis();
      }
      if (detectLine()) return;
      if (!avoidWall(true)) return;
      //      if (flameDetection()) return;
      if (!avoid3Ladder(true)) return;
      if (!getCloser2SRWR(true)) return;
      traceRouteInverse();
    } else {
      if (ping::isOnSRWR) {
        state_lastSWR = millis();
      }
      if (detectLine()) return;
      if (!avoidWall()) return;
      //      if (flameDetection()) return;
      if (!avoid3Ladder()) return;
      if (!getCloser2SRWR()) return;
      traceRoute();
    }
  } else {
    if (state_isInitialized) state_isInitialized = false;

    if (activation::isMenu) {
      if (activation::isMenuChanged) lcd::clean();
      switch (activation::activeMenu) {
        case 0:
          lcd::justPrint(ping::debug(), ping::debug1());
          break;
        case 1:
          lcd::justPrint(flame::debug(), flame::debug1());
          break;
        case 2:
          lcd::justPrint(proxy::debug(), activation::debugSoundActivation());
          break;
        case 3:
          lcd::justPrint(line::debug(), line::debug1());
          break;
        case 4:
          lcd::justPrint("Press start to", "extinguish");
          pump::activate(activation::isStartPushed);
          break;
        case 5:
          lcd::justPrint("IMU 9DOF", "push start");
          pump::activate(activation::isStartPushed);
          break;
        default:
          lcd::justPrint("== DEBUG MODE ==", "Press stop again");
      }
    } else {
      standBy();
    }
  }
}

void standBy () {
  if (legs::isStandby) {
    lcd::message(0, lcd::STANDBY);
    lcd::message(1, lcd::BLANK);
    return;
  }

  if (legs::isNormalized) {
    legs::standby();
    return;
  }

  lcd::message(0, lcd::NORMALIZING);
  lcd::message(1, lcd::BLANK);
  legs::normalize();
}

bool avoid3Ladder (bool inverse = false) {
  if (proxy::isDetectingSomething && CurrentState == 0) {
    lcd::message(0, lcd::THERE_IS_OBSTACLE);
    CurrentState++;
    if (inverse) {
      unsigned int startCounter = millis();
      unsigned int currentCounter = millis();
      while ((currentCounter - startCounter) <= (12300)) {
        legs::forwardHigher();
        currentCounter = millis();
      }
      while ((currentCounter - startCounter) <= (13100)) {
        legs::rotateCCW();
        currentCounter = millis();
      }
      while ((currentCounter - startCounter) <= (19000)) {
        legs::forward();
        currentCounter = millis();
      }
    }
    pingupdate();
    return true;
  }
  return true;
}

bool avoidWall (bool inverse = false) {
  short int minPos = 0;
  short int maxPos = 8;
  bool minPosFound = false;
  bool maxPosFound = false;

  if (!minPosFound && ping::near_a) { minPos = 0; minPosFound = true; }
  if (!minPosFound && ping::near_b) { minPos = 2; minPosFound = true; }
  if (!minPosFound && ping::near_c) { minPos = 4; minPosFound = true; }
  if (!minPosFound && ping::near_d) { minPos = 6; minPosFound = true; }
  if (!minPosFound && ping::near_e) { minPos = 8; minPosFound = true; }

  if (!maxPosFound && ping::near_e) { maxPos = 8; maxPosFound = true; }
  if (!maxPosFound && ping::near_d) { maxPos = 6; maxPosFound = true; }
  if (!maxPosFound && ping::near_c) { maxPos = 4; maxPosFound = true; }
  if (!maxPosFound && ping::near_b) { maxPos = 2; maxPosFound = true; }
  if (!maxPosFound && ping::near_a) { maxPos = 0; maxPosFound = true; }

  if (!minPosFound || !maxPosFound) return true; // this means wall is successfully avoided, if it's not then continue below
  short int avg = (maxPos + minPos) / 2;
  if (avg < 1) {
    lcd::message(0, lcd::WALL_ON_RIGHT);
    lcd::message(1, lcd::SHIFTING_LEFT);
    legs::shiftLeft();
  } else if (1 <= avg && avg <= 3) {
    lcd::message(0, lcd::WALL_ON_RIGHT);
    lcd::message(1, lcd::ROTATING_CCW);
    legs::rotateCCW();
  } else if (3 < avg && avg < 5) {
    lcd::message(0, lcd::WALL_ON_FRONT);

    if ((maxPos - minPos) == 0) {
      lcd::message(1, lcd::MOVING_BACKWARD);
      legs::backward(); // wall surface is flat
    } else {
      lcd::message(1, lcd::ROTATING_CW);

      if (inverse) {
        legs::rotateCCW(); // wall surface detected is not flat
      } else {
        legs::rotateCW(); // wall surface detected is not flat
      }
    }
  } else if (5 <= avg && avg <= 7) {
    lcd::message(0, lcd::WALL_ON_LEFT);
    lcd::message(1, lcd::ROTATING_CW);
    legs::rotateCW();
  } else if (avg > 7) {
    lcd::message(0, lcd::WALL_ON_LEFT);
    lcd::message(1, lcd::SHIFTING_RIGHT);
    legs::shiftRight();
  }
  return false;
}
bool detectLine() {
  //Masuk Room 1
  if (line::isDetected && CounterRead == 0) {
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 3000) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 5000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentCounter = millis();
      legs::rotateCCW();
    }
    while ((currentCounter - startCounter) < 6500) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    while ((currentCounter - startCounter) < 7500) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
      pump::menyebar();
    }
    while ((currentCounter - startCounter) < 9000) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
      pump::menyebarstop();
    }
    unsigned int startFakeCounter = millis();
    unsigned int currentFakeCounter = millis();
    while ((currentFakeCounter - startFakeCounter) < 2000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentFakeCounter = millis();
      legs::rotateCCW();
    }
    while ((currentFakeCounter - startFakeCounter) < 3000) {
      lcd::message(1, lcd::NORMALIZING);
      currentFakeCounter = millis();
      legs::normalize();
    }
    while ((currentFakeCounter - startFakeCounter) < 8000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentFakeCounter = millis();
      legs::rotateCCW();
    }
    pingupdate();
    state_isInversed = false;
    return true;
  }
  
  //Keluar Room 1
  if (line::isDetected && CounterRead == 1) {
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 2000) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 4000) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    pingupdate();
    state_isInversed = true;
    return true;
  }
  //Masuk Room 3
  if (line::isDetected && CounterRead == 2) {
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 3000) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 5000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentCounter = millis();
      legs::rotateCCW();
    }
    while ((currentCounter - startCounter) < 6500) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    while ((currentCounter - startCounter) < 7500) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
      pump::menyebar();
    }
    while ((currentCounter - startCounter) < 9000) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
      pump::menyebarstop();
    }
    unsigned int startFakeCounter = millis();
    unsigned int currentFakeCounter = millis();
    while ((currentFakeCounter - startFakeCounter) < 2000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentFakeCounter = millis();
      legs::rotateCCW();
    }
    while ((currentFakeCounter - startFakeCounter) < 3000) {
      lcd::message(1, lcd::NORMALIZING);
      currentFakeCounter = millis();
      legs::normalize();
    }
    while ((currentFakeCounter - startFakeCounter) < 8000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentFakeCounter = millis();
      legs::rotateCCW();
    }
    pingupdate();
    state_isInversed = false;
    return true;
  }
  
  //Keluar Room 3
  if (line::isDetected && CounterRead == 3) {
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 1600) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    pingupdate();
    state_isInversed = false;
    return true;
  }

  //Masuk Room 4
  if (line::isDetected && CounterRead == 5) {
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 3000) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 5000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentCounter = millis();
      legs::rotateCCW();
    }
    while ((currentCounter - startCounter) < 6500) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    while ((currentCounter - startCounter) < 7500) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
      pump::menyebar();
    }
    while ((currentCounter - startCounter) < 9000) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
      pump::menyebarstop();
    }
    unsigned int startFakeCounter = millis();
    unsigned int currentFakeCounter = millis();
    while ((currentFakeCounter - startFakeCounter) < 2000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentFakeCounter = millis();
      legs::rotateCCW();
    }
    while ((currentFakeCounter - startFakeCounter) < 3000) {
      lcd::message(1, lcd::NORMALIZING);
      currentFakeCounter = millis();
      legs::normalize();
    }
    while ((currentFakeCounter - startFakeCounter) < 7500) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentFakeCounter = millis();
      legs::rotateCCW();
    }
    state_isInversed = false;
    return true;
  }

  //Keluar Room 4
  if (line::isDetected && CounterRead == 6) {
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 2500) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 5000) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    state_isInversed = true;
    return true;
  }

  //Masuk Room 2
  if (line::isDetected && CounterRead == 8) {
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 3000) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 5000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentCounter = millis();
      legs::rotateCCW();
    }
    while ((currentCounter - startCounter) < 6500) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    while ((currentCounter - startCounter) < 7500) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
      pump::menyebar();
    }
    while ((currentCounter - startCounter) < 9000) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
      pump::menyebarstop();
    }
    unsigned int startFakeCounter = millis();
    unsigned int currentFakeCounter = millis();
    while ((currentFakeCounter - startFakeCounter) < 2000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentFakeCounter = millis();
      legs::rotateCCW();
    }
    while ((currentFakeCounter - startFakeCounter) < 3000) {
      lcd::message(1, lcd::NORMALIZING);
      currentFakeCounter = millis();
      legs::normalize();
    }
    while ((currentFakeCounter - startFakeCounter) < 8000) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentFakeCounter = millis();
      legs::rotateCCW();
    }
    while ((currentFakeCounter - startFakeCounter) < 8500) {
      lcd::message(1, lcd::SHIFTING_RIGHT);
      currentFakeCounter = millis();
      legs::shiftRight();
    }
    return true;
  }

  //Keluar Room 2
  if (line::isDetected && CounterRead == 9) {
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 2300) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 4800) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentCounter = millis();
      legs::rotateCCW();
    }
    while ((currentCounter - startCounter) < 7800) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 8200) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentCounter = millis();
      legs::rotateCCW();
    }
    state_isInversed = true;
    return true;
  }
  
  //Indikator Lakban Room 3 Menuju Room 4
  if(line::isDetectedFloor && CounterRead == 4){
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 1600) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 1900){
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    while ((currentCounter - startCounter) < 4500) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 5100){
    lcd::message(1, lcd::ROTATING_CCW);
    currentCounter = millis();
    legs::rotateCCW();
    }
    while ((currentCounter - startCounter) < 9600) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 10600) {
      lcd::message(1, lcd::SHIFTING_LEFT);
      currentCounter = millis();
      legs::shiftLeft();
    }
    pingupdate();
    state_isInversed = true;
    return true;
  }
  
  //Indikator Keluar Room 4 Ke Room 2
  if(line::isDetectedGlue && CounterRead == 7){
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
  while ((currentCounter - startCounter) < 2000) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 4300) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentCounter = millis();
      legs::rotateCCW();
    }
    while ((currentCounter - startCounter) < 6800) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    state_isInversed = false;
    return true;
  }
  //Indikator Lakban PerEmpatan Menuju Home
  if(line::isDetectedFloor && CounterRead == 10){
    CounterRead = CounterRead + 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 2000) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 2500) {
      lcd::message(1, lcd::ROTATING_CCW);
      currentCounter = millis();
      legs::rotateCCW();
    }
    while ((currentCounter - startCounter) < 3500) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 3700) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    while ((currentCounter - startCounter) < 6300) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    while ((currentCounter - startCounter) < 9000) {
      lcd::message(1, lcd::ROTATING_CW);
      currentCounter = millis();
      legs::rotateCW();
    }
    while ((currentCounter - startCounter) < 39000) {
      lcd::message(1, lcd::ROCK_AND_ROLL);
      currentCounter = millis();
      legs::shiftLeftHigher();
    }
    while ((currentCounter - startCounter) < 42000) {
      lcd::message(1, lcd::MOVING_BACKWARD);
      currentCounter = millis();
      legs::backward();
    }
    while ((currentCounter - startCounter) < 45000) {
      lcd::message(1, lcd::ROCK_AND_ROLL);
      currentCounter = millis();
      legs::shiftLeftHigher();
    }
    pingupdate();
    state_isInversed = true;
    return true;
  }
  //Indikator Homing
  if(CounterRead == 11){
    standBy();
    return true;
  }
  if (line::isDetectedGlue){
    line::isDetectedGlue = false;
  }
  return false;
}
bool getCloser2SRWR (bool inverse = false) {
  if (inverse) {
    if (ping::far_e && !ping::far_d && !ping::isOnSLWR) {
      lcd::message(0, lcd::FOUND_SLWR);
      lcd::message(1, lcd::SHIFTING_LEFT);
      legs::shiftLeft();
      return false;
    }
  } else {
    if (ping::far_a && !ping::far_b && !ping::isOnSRWR) {
      lcd::message(0, lcd::FOUND_SRWR);
      lcd::message(1, lcd::SHIFTING_RIGHT);
      legs::shiftRight();
      return false;
    }
  }

  return true;
}

void traceRoute () {
  if (!ping::far_a && !ping::far_b) {
    lcd::message(0, lcd::PATH_ON_RIGHT);
    lcd::message(1, lcd::TURNING_RIGHT);
    legs::turnRight();
  } else if (ping::far_a && !ping::far_c) {
    lcd::message(0, lcd::PATH_ON_FRONT);
    lcd::message(1, lcd::MOVING_FORWARD);
    legs::forward();
  } else if (ping::far_a && ping::far_c && !ping::far_e) {
    lcd::message(0, lcd::PATH_ON_LEFT);
    lcd::message(1, lcd::TURNING_LEFT);
    legs::turnLeft();
  } else {
    lcd::message(0, lcd::NO_PATH);
    lcd::message(1, lcd::ROTATING_CCW);
    legs::rotateCCW(700);
    pingupdate();
  }
}
void traceRouteInverse () {
  if (!ping::far_e && !ping::far_d) {
    lcd::message(0, lcd::PATH_ON_LEFT);
    lcd::message(1, lcd::TURNING_LEFT);
    legs::turnLeft();
  } else if (ping::far_e && !ping::far_c) {
    lcd::message(0, lcd::PATH_ON_FRONT);
    lcd::message(1, lcd::MOVING_FORWARD);
    legs::forward();
  } else if (ping::far_e && ping::far_c && !ping::far_a) {
    lcd::message(0, lcd::PATH_ON_RIGHT);
    lcd::message(1, lcd::TURNING_RIGHT);
    legs::turnRight();
  } else {
    lcd::message(0, lcd::NO_PATH);
    lcd::message(1, lcd::ROTATING_CW);
    legs::rotateCW(700);
    pingupdate();
  }
}
void pingupdate(){
    ping::update();
    ping::update(); 
    ping::update();
    ping::update();
    ping::update();
}
