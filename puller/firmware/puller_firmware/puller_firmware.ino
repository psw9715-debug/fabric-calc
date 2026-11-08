// puller_firmware.ino — 스텝모터 전자 종감속기(풀러) v0.1
//
// 미싱 풀리(주축)에 단 증분형 엔코더의 펄스를 읽어, 정해진 비율로 스텝모터를 돌린다.
// = 기계식 종감속기의 "벨트 + 감속 기어"를 "엔코더 + 전자 기어(분수 누산기)"로 바꾼 것.
//
// 대상 보드: Arduino Uno / Nano (ATmega328P, 16MHz). 외부 라이브러리 없음.
// 설정·보정은 시리얼 모니터(115200, 줄바꿈 LF)로 한다. 'h' 를 치면 명령 목록.
//
// 배선 (자세한 내용은 puller/README.md 6장)
//   D2  ← 엔코더 A (INT0, 상승엣지만 센다)
//   D4  ← 엔코더 B (방향 판정)
//   D5  → 드라이버 STEP
//   D6  → 드라이버 DIR
//   D7  → 드라이버 EN
//   D8  ← 역박음(되돌아박기) 신호, LOW = 역박음 중 (포토커플러 경유, 내부 풀업)
//   D9  ← 풀러 ON/OFF 스위치, LOW = ON (내부 풀업). 스위치 없으면 GND에 묶는다.
//   D13 → 상태 LED

#include <EEPROM.h>
#include "puller_core.h"

// ---------- 하드웨어 상수 (드라이버에 맞춰 확인) ----------
#define PIN_ENC_A   2
#define PIN_ENC_B   4
#define PIN_STEP    5
#define PIN_DIR     6
#define PIN_EN      7
#define PIN_REV     8
#define PIN_RUN     9
#define PIN_LED     13

#define EN_ACTIVE_LOW   1   // A4988/DRV8825/TMC2209 = LOW에서 동작. TB6600 등은 배선 따라 확인
#define STEP_PULSE_US   3   // A4988 ≥1µs, DRV8825 ≥1.9µs, TB6600 계열 ≥2.5µs 권장 → 3µs
#define DIR_SETUP_US    5   // DIR 바꾼 뒤 첫 STEP까지 대기 (TB6600 계열 5µs)

// ATmega328P 직접 포트 접근 (digitalWrite는 ~4µs라 ISR에서 쓰기엔 느리다)
#if defined(__AVR_ATmega328P__)
  #define ENC_B_HIGH()  (PIND & _BV(4))
  #define STEP_HIGH()   (PORTD |= _BV(5))
  #define STEP_LOW()    (PORTD &= ~_BV(5))
  #define DIR_SET(f)    do { if (f) PORTD |= _BV(6); else PORTD &= ~_BV(6); } while (0)
  #define REV_ACTIVE()  (!(PINB & _BV(0)))   // D8 = PB0, LOW = 역박음 중
#else
  #define ENC_B_HIGH()  (digitalRead(PIN_ENC_B) == HIGH)
  #define STEP_HIGH()   digitalWrite(PIN_STEP, HIGH)
  #define STEP_LOW()    digitalWrite(PIN_STEP, LOW)
  #define DIR_SET(f)    digitalWrite(PIN_DIR, (f) ? HIGH : LOW)
  #define REV_ACTIVE()  (digitalRead(PIN_REV) == LOW)
#endif

// ---------- 저장 설정 ----------
#define CFG_MAGIC   0x5055   // 'PU'
#define CFG_VERSION 1

enum RevMode { REV_FREE = 0, REV_FOLLOW = 1, REV_IGNORE = 2 };

struct Stored {
  uint16_t magic;
  uint8_t  version;
  PullerConfig c;
  uint8_t  encInvert;   // 1 = 엔코더 방향 반대
  uint8_t  motorInvert; // 1 = 모터 방향 반대
  uint8_t  revMode;     // RevMode
};

Stored cfg;
PullerRatio ratio;
ElectronicGear gearCore;

volatile int32_t  encCount = 0;     // 주축 카운트 누계 (속도 표시용)
volatile int32_t  stepCount = 0;    // 낸 스텝 누계
volatile bool     follow = false;   // false면 카운트는 세되 모터는 안 돌림
volatile bool     simActive = false;
volatile int8_t   lastDir = 0;

float   lastSimExpectMm = 0;
char    lineBuf[40];
uint8_t lineLen = 0;

// ---------- 핵심: 카운트 1개 처리 (ISR과 시뮬레이션 공용) ----------
static inline void handleCount(int8_t d) {
  encCount += d;
  if (!follow) return;
  if (cfg.revMode == REV_FOLLOW && REV_ACTIVE()) d = -d;
  int8_t s = gearCore.onCount(d);
  if (s == 0) return;
  bool fwd = (s > 0) ^ (cfg.motorInvert != 0);
  if (s != lastDir) { DIR_SET(fwd); lastDir = s; delayMicroseconds(DIR_SETUP_US); }
  STEP_HIGH();
  delayMicroseconds(STEP_PULSE_US);
  STEP_LOW();
  stepCount += s;
}

void encISR() {
  if (simActive) return;
  int8_t d = ENC_B_HIGH() ? -1 : 1;   // A 상승 시점에 B가 LOW면 정회전(배선 따라 반대일 수 있음 → 'inv')
  if (cfg.encInvert) d = -d;
  handleCount(d);
}

// ---------- 드라이버 ----------
void driverEnable(bool on) {
  digitalWrite(PIN_EN, (on ^ (EN_ACTIVE_LOW != 0)) ? HIGH : LOW);
}

// ---------- 설정 ----------
void setDefaults() {
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.c.stitchMm = 3.0f;
  cfg.c.overfeedPct = 2.0f;
  cfg.c.rollerDmm = 40.0f;
  cfg.c.gear = 3.0f;
  cfg.c.calib = 1.0f;
  cfg.c.motorSpr = 200;
  cfg.c.microstep = 8;
  cfg.c.encCpr = 200;
  cfg.encInvert = 0;
  cfg.motorInvert = 0;
  cfg.revMode = REV_FREE;
}

bool cfgSane(const PullerConfig &c) {
  return c.stitchMm > 0.1f && c.stitchMm < 12.0f &&
         c.overfeedPct > -20.0f && c.overfeedPct < 30.0f &&
         c.rollerDmm > 5.0f && c.rollerDmm < 200.0f &&
         c.gear >= 0.1f && c.gear <= 50.0f &&
         c.calib > 0.5f && c.calib < 1.5f &&
         c.motorSpr > 0 && c.microstep > 0 && c.encCpr > 0;
}

// 비율을 다시 계산해 적용. den(=CPR)이 같으면 num만 바꿔 운전 중에도 끊김 없음.
bool applyRatio() {
  PullerRatio r = pullerComputeRatio(cfg.c);
  if (!r.ok) {
    Serial.print(F("! 스텝/카운트 = ")); Serial.print(r.stepsPerCount, 3);
    Serial.println(F(" > 1. 엔코더 CPR을 올리거나 마이크로스텝·감속비를 낮출 것. 적용 안 함."));
    return false;
  }
  noInterrupts();
  if (r.den != gearCore.den) gearCore.init(r); else gearCore.setNum(r.num);
  interrupts();
  ratio = r;
  return true;
}

void loadCfg() {
  EEPROM.get(0, cfg);
  if (cfg.magic != CFG_MAGIC || cfg.version != CFG_VERSION || !cfgSane(cfg.c)) {
    setDefaults();
    Serial.println(F("# 저장된 설정 없음 → 기본값"));
  }
}

void saveCfg() { EEPROM.put(0, cfg); Serial.println(F("# 저장함")); }

// ---------- 상태 ----------
void printStatus() {
  Serial.println(F("---- 전자 종감속기 v0.1 ----"));
  Serial.print(F("땀 길이        s ")); Serial.print(cfg.c.stitchMm, 2); Serial.println(F(" mm"));
  Serial.print(F("선행률         o ")); Serial.print(cfg.c.overfeedPct, 2); Serial.println(F(" %"));
  Serial.print(F("롤러 외경      d ")); Serial.print(cfg.c.rollerDmm, 2); Serial.println(F(" mm"));
  Serial.print(F("감속비         g ")); Serial.println(cfg.c.gear, 3);
  Serial.print(F("보정계수     cal ")); Serial.println(cfg.c.calib, 4);
  Serial.print(F("풀스텝/회전   fs ")); Serial.println(cfg.c.motorSpr);
  Serial.print(F("마이크로스텝   m ")); Serial.println(cfg.c.microstep);
  Serial.print(F("엔코더 CPR     e ")); Serial.println(cfg.c.encCpr);
  Serial.print(F("역박음 모드    r ")); Serial.println(cfg.revMode == REV_FREE ? F("0 풀어줌") : cfg.revMode == REV_FOLLOW ? F("1 같이 후진") : F("2 무시"));
  Serial.print(F("방향 반전  inv/minv ")); Serial.print(cfg.encInvert); Serial.print('/'); Serial.println(cfg.motorInvert);
  Serial.print(F("=> 스텝/땀 ")); Serial.print(ratio.stepsPerStitch, 3);
  Serial.print(F(", 스텝/카운트 ")); Serial.print(ratio.stepsPerCount, 4);
  Serial.print(F(", 분해능 ")); Serial.print(ratio.mmPerStep * 1000.0f, 1); Serial.println(F(" um/스텝"));
}

void printHelp() {
  Serial.println(F("명령: p 상태 | s <mm> 땀길이 | o <%> 선행률 | d <mm> 롤러외경 | g <비> 감속비"));
  Serial.println(F("      fs <n> 풀스텝 | m <n> 마이크로스텝 | e <n> 엔코더CPR | r <0|1|2> 역박음"));
  Serial.println(F("      inv / minv 방향반전 | sim <spm> <땀수> 미싱 없이 시험 | cal <실측mm> 보정"));
  Serial.println(F("      w 저장 | x 기본값 | ! 시뮬레이션 중단"));
}

// ---------- 시뮬레이션: 미싱 없이 가상 엔코더 펄스로 돌린다 (보정에 사용) ----------
void runSim(long spm, long stitches) {
  if (spm <= 0 || stitches <= 0) { Serial.println(F("! sim <spm> <땀수>")); return; }
  long total = stitches * (long)cfg.c.encCpr;
  Serial.print(F("# 시뮬레이션 ")); Serial.print(spm); Serial.print(F(" spm, "));
  Serial.print(stitches); Serial.println(F(" 땀. '!'로 중단"));
  simActive = true;
  bool wasFollow = follow;
  follow = true;
  driverEnable(true);
  int32_t s0 = stepCount;
  unsigned long t0 = micros(), next = t0;
  for (long i = 0; i < total; i++) {
    // 0.3초 동안 선형 가속 (모터 탈조 방지)
    unsigned long el = micros() - t0;
    float k = el >= 300000UL ? 1.0f : 0.05f + 0.95f * (float)el / 300000.0f;
    unsigned long period = (unsigned long)(60000000.0f / ((float)spm * k * (float)cfg.c.encCpr));
    while ((long)(micros() - next) < 0) {}
    next += period;
    noInterrupts(); handleCount(1); interrupts();
    if ((i & 0xFF) == 0 && Serial.available() && Serial.peek() == '!') { Serial.read(); Serial.println(F("# 중단")); break; }
  }
  simActive = false;
  follow = wasFollow;
  long st = stepCount - s0;
  lastSimExpectMm = st * ratio.mmPerStep;   // 낸 스텝 기준의 이론 이송
  Serial.print(F("# 낸 스텝 ")); Serial.print(st);
  Serial.print(F(", 이론 이송 ")); Serial.print(lastSimExpectMm, 2);
  Serial.println(F(" mm. 실제 이동량을 재서 'cal <mm>' 입력"));
}

// 실측 보정: 이론 이송/실제 이송 비로 보정계수를 갱신 (롤러 외경 오차·TPU 눌림 등을 흡수)
void doCal(float measuredMm) {
  if (lastSimExpectMm <= 0 || measuredMm <= 0) { Serial.println(F("! 먼저 sim 을 돌린 뒤 실측값을 넣을 것")); return; }
  float nc = cfg.c.calib * lastSimExpectMm / measuredMm;
  PullerConfig t = cfg.c; t.calib = nc;
  if (!cfgSane(t)) { Serial.println(F("! 보정계수가 ±50%를 넘음 — 롤러 외경/감속비/마이크로스텝 설정부터 확인")); return; }
  cfg.c.calib = nc;
  applyRatio();
  Serial.print(F("# 보정계수 = ")); Serial.println(nc, 4);
  lastSimExpectMm = 0;
}

// ---------- 시리얼 명령 ----------
void setFloat(float &field, const char *arg) {
  float old = field;
  field = atof(arg);
  if (!cfgSane(cfg.c) || !applyRatio()) { field = old; applyRatio(); Serial.println(F("! 값 거부")); }
  else printStatus();
}

void setU16(uint16_t &field, const char *arg) {
  uint16_t old = field;
  field = (uint16_t)atol(arg);
  if (!cfgSane(cfg.c) || !applyRatio()) { field = old; applyRatio(); Serial.println(F("! 값 거부")); }
  else { printStatus(); Serial.println(F("# 드라이버 DIP/엔코더 실제 사양과 같은지 확인")); }
}

void handleLine(char *ln) {
  char *cmd = strtok(ln, " ");
  char *a1 = strtok(NULL, " ");
  char *a2 = strtok(NULL, " ");
  if (!cmd) return;
  if (!strcmp(cmd, "p")) printStatus();
  else if (!strcmp(cmd, "h")) printHelp();
  else if (!strcmp(cmd, "s") && a1) setFloat(cfg.c.stitchMm, a1);
  else if (!strcmp(cmd, "o") && a1) setFloat(cfg.c.overfeedPct, a1);
  else if (!strcmp(cmd, "d") && a1) setFloat(cfg.c.rollerDmm, a1);
  else if (!strcmp(cmd, "g") && a1) setFloat(cfg.c.gear, a1);
  else if (!strcmp(cmd, "fs") && a1) setU16(cfg.c.motorSpr, a1);
  else if (!strcmp(cmd, "m") && a1) setU16(cfg.c.microstep, a1);
  else if (!strcmp(cmd, "e") && a1) setU16(cfg.c.encCpr, a1);
  else if (!strcmp(cmd, "r") && a1) { int v = atoi(a1); if (v >= 0 && v <= 2) cfg.revMode = v; printStatus(); }
  else if (!strcmp(cmd, "inv")) { cfg.encInvert ^= 1; printStatus(); }
  else if (!strcmp(cmd, "minv")) { cfg.motorInvert ^= 1; lastDir = 0; printStatus(); }
  else if (!strcmp(cmd, "sim") && a1 && a2) runSim(atol(a1), atol(a2));
  else if (!strcmp(cmd, "cal") && a1) doCal(atof(a1));
  else if (!strcmp(cmd, "w")) saveCfg();
  else if (!strcmp(cmd, "x")) { setDefaults(); applyRatio(); printStatus(); }
  else { Serial.println(F("? 'h' = 도움말")); }
}

void pollSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') { lineBuf[lineLen] = 0; handleLine(lineBuf); lineLen = 0; }
    else if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = ch;
  }
}

// ---------- setup / loop ----------
void setup() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);   // 오픈컬렉터 엔코더용. 외부 풀업(1~4.7k)이 있으면 더 좋다
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_REV, INPUT_PULLUP);
  pinMode(PIN_RUN, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  driverEnable(false);
  Serial.begin(115200);
  loadCfg();
  if (!applyRatio()) { setDefaults(); applyRatio(); }
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encISR, RISING);
  printStatus();
  printHelp();
}

void loop() {
  pollSerial();

  // 풀러 ON/OFF 와 역박음 상태로 추종 여부와 드라이버 전원을 정한다
  bool runOn = digitalRead(PIN_RUN) == LOW;
  bool rev = digitalRead(PIN_REV) == LOW;
  bool want = runOn && !(rev && cfg.revMode == REV_FREE);
  if (want != follow) {
    noInterrupts(); follow = want; interrupts();
    driverEnable(want);          // OFF·역박음(풀어줌)이면 전원을 끊어 롤러가 원단을 따라 헛돈다
  }

  // 0.5초마다 속도 표시 (주축 spm)
  static unsigned long tPrev = 0;
  static int32_t cPrev = 0;
  unsigned long now = millis();
  if (now - tPrev >= 500) {
    noInterrupts(); int32_t c = encCount; interrupts();
    long spm = (long)((c - cPrev) * 120L / (long)cfg.c.encCpr);   // 0.5초 → ×120 = 분당
    cPrev = c; tPrev = now;
    digitalWrite(PIN_LED, follow ? (spm != 0 ? ((now / 100) & 1) : HIGH) : LOW);
    static long spmPrinted = 0;
    if (labs(spm - spmPrinted) >= 20 || (spm == 0 && spmPrinted != 0)) { Serial.print(F("spm ")); Serial.println(spm); spmPrinted = spm; }
  }
}
