// puller_core.h — 전자 종감속기의 계산 핵심 (하드웨어 독립, PC에서 테스트 가능)
//
// 기계식 종감속기는 "주축 1회전 = 롤러 일정 각도"라는 고정된 기어비를 가진다.
// 여기서는 그 기어비를 정수 분수 num/den(스텝/엔코더카운트)로 표현하고,
// 엔코더 카운트가 들어올 때마다 누산기에 num을 더해 den을 넘으면 스텝 1개를 낸다
// (Bresenham/DDA). 부동소수 오차가 쌓이지 않고, 역회전하면 정확히 되돌아간다.
#ifndef PULLER_CORE_H
#define PULLER_CORE_H

#include <stdint.h>
#include <math.h>

#define PULLER_SCALE 10000L   // 스텝/땀 을 1/10000 스텝 단위로 표현

struct PullerConfig {
  float    stitchMm;     // 땀 길이 mm (미싱 다이얼 값과 같게)
  float    overfeedPct;  // 풀러 선행률 % (+면 롤러가 톱니보다 약간 더 보냄)
  float    rollerDmm;    // 구동 롤러 외경 mm (공칭)
  float    gear;         // 모터→롤러 감속비 (20T→60T 이면 3.0)
  float    calib;        // 실측 보정계수 (1.0 = 보정 없음)
  uint16_t motorSpr;     // 모터 풀스텝/회전 (1.8도 = 200)
  uint16_t microstep;    // 드라이버 마이크로스텝
  uint16_t encCpr;       // 엔코더 카운트/주축1회전 (A상 상승엣지만 세면 = PPR)
};

struct PullerRatio {
  int32_t num;            // 카운트 1개당 더할 값
  int32_t den;            // 이 값을 넘으면 스텝 1개
  float   stepsPerStitch; // 주축 1회전(=1땀)당 스텝
  float   stepsPerCount;  // num/den
  float   mmPerStep;      // 원단 이송 분해능
  bool    ok;             // stepsPerCount <= 1 이어야 ISR 1회에 스텝 최대 1개
};

static inline PullerRatio pullerComputeRatio(const PullerConfig &c) {
  PullerRatio r;
  float circ = (float)M_PI * c.rollerDmm;
  float stepsPerRollerRev = (float)c.motorSpr * (float)c.microstep * c.gear;
  float rollerRevPerStitch = c.stitchMm * (1.0f + c.overfeedPct / 100.0f) / circ;
  r.stepsPerStitch = rollerRevPerStitch * stepsPerRollerRev * c.calib;
  r.den = (int32_t)c.encCpr * PULLER_SCALE;
  r.num = (int32_t)(r.stepsPerStitch * (float)PULLER_SCALE + 0.5f);  // 양수만 들어온다
  r.stepsPerCount = (float)r.num / (float)r.den;
  r.mmPerStep = circ / (stepsPerRollerRev * c.calib);
  r.ok = (c.encCpr > 0) && (r.num > 0) && (r.num <= r.den);
  return r;
}

// 엔코더 카운트 → 스텝 변환기. ISR 안에서 호출된다(가볍게 유지).
struct ElectronicGear {
  int32_t acc;   // 0 <= acc < den
  int32_t num;
  int32_t den;

  void init(const PullerRatio &r) { acc = 0; num = r.num; den = r.den; }
  // den(=CPR×SCALE)은 바뀌지 않으므로 num만 갈아끼우면 운전 중에도 튀지 않는다.
  void setNum(int32_t n) { num = n; }

  // dir: +1(정회전 카운트) / -1(역회전 카운트). 반환: +1/-1 스텝, 0 = 이번엔 없음
  inline int8_t onCount(int8_t dir) {
    if (dir > 0) {
      acc += num;
      if (acc >= den) { acc -= den; return 1; }
    } else {
      acc -= num;
      if (acc < 0) { acc += den; return -1; }
    }
    return 0;
  }
};

#endif
