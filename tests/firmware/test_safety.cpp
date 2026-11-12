// test_safety.cpp - behaviour tests for the firmware, run on a PC.
//
// Each test powers up a fresh simulated machine (in its own process), changes the
// world around it and checks what the firmware did. Run them with `make test`.
// Add a test: write TEST(name) { ... } below. Keep each one about ONE behaviour.
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "app.h"
#include "sim.h"

// ----------------------------------------------------------- tiny test runner
struct TestCase { const char* name; void (*fn)(); };
static std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
struct Register { Register(const char* n, void (*f)()) { registry().push_back({ n, f }); } };
#define TEST(name) static void name(); static Register reg_##name(#name, name); static void name()

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { failures++; \
  fprintf(stderr, "      check failed at %s:%d: %s   (t = %.2f s, state %d, reason %d)\n", \
          __FILE__, __LINE__, #cond, now(), state, latchedReason); } } while (0)

static void startMotor() {             // power up, press GO, wait for the baseline
  boot();
  run(1);
  pressGo();
  run(8);
}

// ================================================================== power-up
TEST(power_up_waits_in_idle_with_motor_off) {
  boot();
  run(3);
  CHECK(state == ST_IDLE);
  CHECK(!relayClosed());
  CHECK(motorPwm() == 0);
  CHECK(sawLine("SELFTEST,MPU6050=OK DHT22=OK"));
  run(10);                                       // never starts by itself
  CHECK(state == ST_IDLE);
  CHECK(!relayClosed());
}

TEST(missing_mpu6050_is_a_fault_at_power_up) {
  world.mpuPresent = false;
  boot();
  run(1);
  CHECK(state == ST_FAULT);
  CHECK(latchedReason == R_SENS_MPU);
  pressGo();
  run(1);
  CHECK(state == ST_FAULT);                     // GO re-tests, the sensor is still missing
  CHECK(!relayClosed());
}

TEST(dht11_is_detected_automatically) {
  world.dhtModel = 11;
  boot();
  run(3);
  CHECK(dhtModel == 11);
  CHECK(fabsf(tempC - 30) < 1.0f);
  CHECK(sawLine("DHT11=OK"));
}

// ================================================================== running
TEST(go_starts_the_motor_with_a_soft_start) {
  boot();
  run(1);
  pressGo();
  run(0.3);
  CHECK(state == ST_RUN);
  CHECK(relayClosed());
  CHECK(motorPwm() < pctToPwm(SPEED_DEFAULT));  // still ramping up
  run(1);
  CHECK(motorPwm() == pctToPwm(SPEED_DEFAULT));
}

TEST(vibration_baseline_is_learned_after_start) {
  startMotor();
  CHECK(sawLine("BASELINE,speed_pct=100"));
  CHECK(vibBase > 15 && vibBase < 60);
  CHECK(vibWarn >= VIB_FLOOR_MG && vibWarn < VIB_ABS_STOP_MG);
  CHECK(vibStop > vibWarn);
}

TEST(each_speed_learns_its_own_baseline_once) {
  startMotor();
  sendSerial("50\n");
  run(8);
  CHECK(sawLine("BASELINE,speed_pct=50"));
  size_t learned = countLines("BASELINE");
  sendSerial("100\n");                           // already known: no new learning
  run(8);
  CHECK(countLines("BASELINE") == learned);
  CHECK(state == ST_RUN);
}

// ================================================================== proximity
TEST(person_in_warning_zone_slows_the_motor_then_recovers) {
  startMotor();
  world.distCm = 30;
  run(1.5);
  CHECK(state == ST_WARN);
  CHECK(worstReason == R_PROX);
  CHECK(motorPwm() == warnPwm());
  world.distCm = 120;
  run(2);
  CHECK(state == ST_WARN);                       // must stay clear for 3 s first
  run(2.5);
  CHECK(state == ST_RUN);
  CHECK(motorPwm() == pctToPwm(SPEED_DEFAULT));
}

TEST(person_in_stop_zone_latches_an_emergency_stop) {
  startMotor();
  world.distCm = 10;
  run(1);
  CHECK(state == ST_ESTOP);
  CHECK(latchedReason == R_PROX);
  CHECK(!relayClosed());
  CHECK(motorPwm() == 0);
  pressGo();
  run(0.5);
  CHECK(state == ST_ESTOP);                      // refused: the person is still there
  world.distCm = 120;
  run(2);
  CHECK(state == ST_ESTOP);                      // latched: never resets by itself
  pressGo();
  run(0.5);
  CHECK(state == ST_IDLE);
  pressGo();
  run(0.5);
  CHECK(state == ST_RUN);
}

// ================================================================== E-stop
TEST(estop_button_cuts_the_motor_inside_the_interrupt) {
  startMotor();
  CHECK(relayClosed());
  pressEstop();                                  // no loop() has run yet:
  CHECK(!relayClosed());                         // the interrupt itself cut the power
  CHECK(motorPwm() == 0);
  run(0.2);
  CHECK(state == ST_ESTOP);
  CHECK(latchedReason == R_ESTOP_BTN);
  pressGo();
  run(0.5);
  CHECK(state == ST_ESTOP);                      // refused while E-STOP is held
  releaseEstop();
  run(0.3);
  pressGo();
  run(0.5);
  CHECK(state == ST_IDLE);
}

TEST(remote_stop_from_the_pc) {
  startMotor();
  sendSerial("x");
  run(0.3);
  CHECK(state == ST_ESTOP);
  CHECK(latchedReason == R_REMOTE);
  CHECK(!relayClosed());
}

// ================================================================== machine faults
TEST(abnormal_vibration_trips_the_machine) {
  startMotor();
  world.extraVibMg = 300;                        // an unbalanced weight on the shaft
  run(3);
  CHECK(state == ST_ESTOP);
  CHECK(latchedReason == R_VIB);
}

TEST(jammed_shaft_is_detected_as_a_stall) {
  startMotor();
  world.jam = true;
  run(3);
  CHECK(state == ST_ESTOP);
  CHECK(latchedReason == R_STALL);
}

TEST(tilt_warns_then_stops) {
  startMotor();
  world.tiltDeg = 15;
  run(2);
  CHECK(state == ST_WARN);
  CHECK(worstReason == R_TILT);
  world.tiltDeg = 30;
  run(2);
  CHECK(state == ST_ESTOP);
  CHECK(latchedReason == R_TILT);
}

TEST(fast_heating_trips_before_the_temperature_limit) {
  startMotor();
  for (int i = 0; i < 60 && state != ST_ESTOP; i++) {   // +0.5 C per second
    world.tempC += 0.25f;
    run(0.5);
  }
  CHECK(state == ST_ESTOP);
  CHECK(latchedReason == R_TRATE);
  CHECK(world.tempC < TEMP_STOP_C);              // caught by the rate, before 50 C
}

TEST(temperature_sensor_lost_while_running_stops_the_machine) {
  startMotor();
  world.dhtPresent = false;
  run(20);
  CHECK(state == ST_ESTOP);                      // while running: a latched stop
  CHECK(latchedReason == R_SENS_TEMP);
  CHECK(!relayClosed());
}

// ================================================================== commands and data
TEST(speed_commands_are_checked) {
  boot();
  run(1);
  sendSerial("60\n");
  run(1);
  CHECK(speedPct == 60);
  CHECK(sawLine("SPEED"));
  sendSerial("5\n");                             // below the 10 % minimum: refused
  run(1);
  CHECK(speedPct == 60);
  sendSerial("g");
  run(2);
  CHECK(motorPwm() == pctToPwm(60));
}

TEST(data_mode_rows_match_the_header) {
  boot();
  run(1);
  sendSerial("d");
  run(3);
  const auto& ls = serialLines();
  size_t cols = 0, rows = 0;
  bool consistent = true;
  for (const auto& l : ls) {
    size_t n = 1;
    for (char c : l) n += (c == ',');
    if (l.rfind("H,", 0) == 0) cols = n;
    if (l.rfind("D,", 0) == 0) { rows++; if (cols && n != cols) consistent = false; }
  }
  CHECK(cols == 20);                             // "H" + 19 column names
  CHECK(rows >= 4);                              // one row every 0.5 s
  CHECK(consistent);
  CHECK(sawLine("L,dist_warn_cm=40"));
}

// ================================================================== runner
int main(int argc, char** argv) {
  const char* only = argc > 1 ? argv[1] : nullptr;
  int ran = 0, failed = 0;
  for (const auto& t : registry()) {
    if (only && !strstr(t.name, only)) continue;
    fflush(stdout);
    pid_t pid = fork();                          // a fresh machine for every test
    if (pid == 0) {
      t.fn();
      if (failures) {
        fprintf(stderr, "      last serial lines:\n");
        const auto& ls = serialLines();
        for (size_t i = ls.size() > 12 ? ls.size() - 12 : 0; i < ls.size(); i++)
          fprintf(stderr, "        %s\n", ls[i].c_str());
      }
      _exit(failures ? 1 : 0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    printf("  %s  %s\n", ok ? "PASS" : "FAIL", t.name);
    ran++;
    if (!ok) failed++;
  }
  printf("\n%d tests, %d failed\n", ran, failed);
  return failed ? 1 : 0;
}
