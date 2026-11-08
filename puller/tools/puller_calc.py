#!/usr/bin/env python3
"""종감속기(풀러) 설계 계산기 — 설계 문서(puller/README.md)의 숫자를 재현한다.

값을 바꿔 다시 돌리면 내 부품 기준의 숫자가 나온다.
  python3 puller/tools/puller_calc.py
"""
import math

# ---- 입력 (자기 장비 값으로 바꿀 것) ----
STITCH_MM   = 3.0      # 땀 길이 (mm/땀 = mm/주축1회전)
OVERFEED    = 0.02     # 풀러 선행률 (+2%)
ROLLER_D_MM = 40.0     # 구동 롤러 외경 (mm)
MOTOR_SPR   = 200      # 스텝모터 풀스텝/회전 (1.8도)
MICROSTEP   = 8        # 드라이버 마이크로스텝
GEAR        = 3.0      # 모터→롤러 감속비 (GT2 20T→60T = 3)
ENC_CPR     = 200      # 풀리 엔코더 카운트/회전 (A상 상승엣지만 = PPR)
SPM_LIST    = [500, 1000, 2000, 3000, 4000, 5000]
ROTOR_J     = 54e-7    # NEMA17(17HS4401급) 로터 관성 kg·m² (54 g·cm²) — 데이터시트 확인
ROLLER_J    = 2.0e-5   # 롤러+풀리 관성 kg·m² (Ø40 알루미늄+TPU 대략치) — 실측/CAD로 교체
FEED_WINDOW_DEG = 150  # 간헐(캠) 모드: 주축 한 바퀴 중 원단을 보내는 구간 각도

circ = math.pi * ROLLER_D_MM
roller_rev_per_stitch = STITCH_MM * (1 + OVERFEED) / circ
steps_per_roller_rev = MOTOR_SPR * MICROSTEP * GEAR
steps_per_stitch = roller_rev_per_stitch * steps_per_roller_rev
steps_per_count = steps_per_stitch / ENC_CPR
mm_per_step = circ / steps_per_roller_rev

print("== 기하 ==")
print(f"롤러 둘레           {circ:8.2f} mm")
print(f"롤러 회전/땀        {roller_rev_per_stitch:8.5f} rev  (주축:롤러 = 1 : {1/roller_rev_per_stitch:.1f})")
print(f"스텝/롤러1회전      {steps_per_roller_rev:8.0f}")
print(f"스텝/땀             {steps_per_stitch:8.2f}")
print(f"스텝/엔코더카운트   {steps_per_count:8.4f}  ({'OK ≤1' if steps_per_count <= 1 else '초과! CPR을 올리거나 마이크로스텝을 낮출 것'})")
print(f"원단 분해능         {mm_per_step*1000:8.1f} µm/스텝")
print()
print("== 연속 모드(전자기어) — 속도별 ==")
print(f"{'spm':>6} {'원단 mm/s':>10} {'롤러rpm':>8} {'모터rpm':>8} {'스텝Hz':>8} {'ISR/s':>8}")
for spm in SPM_LIST:
    f = spm / 60.0
    v = STITCH_MM * (1 + OVERFEED) * f
    rr = roller_rev_per_stitch * spm
    mr = rr * GEAR
    sh = steps_per_stitch * f
    print(f"{spm:6d} {v:10.1f} {rr:8.1f} {mr:8.1f} {sh:8.0f} {ENC_CPR*f:8.0f}")
print()
print("== 간헐(캠) 모드 — 삼각 속도 프로파일로 한 땀을 FEED_WINDOW 안에 보낼 때 ==")
J_motor_side = ROTOR_J + ROLLER_J / GEAR**2
print(f"모터축 환산 관성   {J_motor_side*1e7:.1f} g·cm²")
print(f"{'spm':>6} {'창(ms)':>7} {'최고속 mm/s':>11} {'가속 m/s²':>10} {'모터rpm최고':>11} {'가속토크 N·m':>12}")
for spm in SPM_LIST:
    T = 60.0 / spm * FEED_WINDOW_DEG / 360.0
    L = STITCH_MM * (1 + OVERFEED) / 1000.0  # m
    vpk = 2 * L / T
    a = 4 * L / T**2
    r = ROLLER_D_MM / 2000.0
    alpha_motor = a / r * GEAR
    rpm_pk = vpk / r * GEAR * 60 / (2 * math.pi)
    tq = J_motor_side * alpha_motor
    print(f"{spm:6d} {T*1000:7.2f} {vpk*1000:11.0f} {a:10.1f} {rpm_pk:11.0f} {tq:12.3f}")
print()
print("참고: 가속토크는 관성만의 값이다. 원단을 당기는 힘 F[N] × 롤러반경 / 감속비가 여기에 더해진다.")
print(f"예) F=20N → 모터축 {20*ROLLER_D_MM/2000/GEAR:.3f} N·m 추가")
