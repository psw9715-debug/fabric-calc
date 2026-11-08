// PC에서 puller_core.h 검증: g++ -std=c++11 -O2 puller/test/test_core.cpp -o /tmp/t && /tmp/t
#include "../firmware/puller_firmware/puller_core.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main() {
  PullerConfig c = {3.0f, 2.0f, 40.0f, 3.0f, 1.0f, 200, 8, 200};
  PullerRatio r = pullerComputeRatio(c);
  printf("stepsPerStitch=%.4f stepsPerCount=%.5f num=%ld den=%ld mm/step=%.4f ok=%d\n",
         r.stepsPerStitch, r.stepsPerCount, (long)r.num, (long)r.den, r.mmPerStep, r.ok);
  CHECK(r.ok, "ratio must be <= 1 step per count");
  CHECK(fabs(r.stepsPerStitch - 116.88f) < 0.05f, "stepsPerStitch %.3f", r.stepsPerStitch);

  // 1) 10만 땀 정회전: 스텝 수가 이론값과 1스텝 이내
  ElectronicGear g; g.init(r);
  long steps = 0;
  const long stitches = 100000;
  for (long i = 0; i < stitches * c.encCpr; i++) steps += g.onCount(1);
  double expect = (double)r.num / r.den * stitches * c.encCpr;
  printf("forward: steps=%ld expect=%.2f\n", steps, expect);
  CHECK(fabs(steps - expect) <= 1.0, "forward drift");
  // 이송 길이 오차 (mm): 이론 이송 vs 스텝×분해능
  double mm = steps * r.mmPerStep, mmExpect = stitches * 3.0 * 1.02;
  printf("feed: %.3f mm vs %.3f mm (err %.4f%%)\n", mm, mmExpect, (mm - mmExpect) / mmExpect * 100);
  CHECK(fabs(mm - mmExpect) / mmExpect < 1e-4, "feed length error too big");

  // 2) 같은 카운트만큼 역회전하면 정확히 0으로 돌아온다
  for (long i = 0; i < stitches * c.encCpr; i++) steps += g.onCount(-1);
  printf("after reverse: steps=%ld acc=%ld\n", steps, (long)g.acc);
  CHECK(steps == 0 && g.acc == 0, "reverse must return to origin");

  // 3) 앞뒤로 흔들어도(손으로 풀리 돌리기) 누적 오차 없음
  srand(1); long pos = 0; steps = 0;
  for (int i = 0; i < 2000000; i++) { int8_t d = (rand() % 3) ? 1 : -1; pos += d; steps += g.onCount(d); }
  double ex2 = (double)pos * r.num / r.den;
  printf("jitter: pos=%ld steps=%ld expect=%.2f\n", pos, steps, ex2);
  CHECK(fabs(steps - ex2) <= 1.0, "jitter drift");

  // 4) 한 카운트에 스텝은 최대 1개 (연속 출력 간격 확인)
  g.init(r); long lastStep = -1; long minGap = 1 << 30;
  for (long i = 0; i < 100000; i++) if (g.onCount(1)) { if (lastStep >= 0 && i - lastStep < minGap) minGap = i - lastStep; lastStep = i; }
  printf("min counts between steps = %ld\n", minGap);
  CHECK(minGap >= 1, "at most one step per count");

  // 5) 운전 중 비율 변경(num 교체): 튐 없이 이어짐
  g.init(r); steps = 0;
  for (int i = 0; i < 200 * 50; i++) steps += g.onCount(1);
  PullerConfig c2 = c; c2.stitchMm = 4.0f; PullerRatio r2 = pullerComputeRatio(c2);
  g.setNum(r2.num);
  for (int i = 0; i < 200 * 50; i++) steps += g.onCount(1);
  double ex5 = 50 * r.stepsPerStitch + 50 * r2.stepsPerStitch;
  printf("ratio change: steps=%ld expect=%.2f\n", steps, ex5);
  CHECK(fabs(steps - ex5) <= 1.5, "ratio change");

  // 6) 비율 초과 설정은 ok=false
  PullerConfig c3 = c; c3.microstep = 32; PullerRatio r3 = pullerComputeRatio(c3);
  printf("microstep 32 -> stepsPerCount=%.3f ok=%d\n", r3.stepsPerCount, r3.ok);
  CHECK(!r3.ok, "should reject >1 step/count");

  printf(fails ? "\n%d FAILED\n" : "\nALL PASS\n", fails);
  return fails ? 1 : 0;
}
