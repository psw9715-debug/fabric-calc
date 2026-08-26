# 홈패션 연단 계산기 — 클로드 코드 인수인계서

## 📋 프로젝트 개요

홈패션 원단 재단 작업을 위한 모바일 웹앱.  
아이폰 13 미니(375px)에서 사파리로 주로 사용.  
GitHub Pages로 호스팅, GitHub Gist로 다기기 데이터 동기화.

---

## 🌐 배포 정보

| 항목 | 값 |
|------|-----|
| GitHub 저장소 | `psw9715-debug/fabric-calc` |
| GitHub Pages URL | `https://psw9715-debug.github.io/fabric-calc` |
| 메인 파일 | `index.html` (단일 파일 앱) |
| 데이터 동기화 | GitHub Gist (사용자가 앱 내에서 토큰 입력) |
| 로컬 저장 | localStorage (`fabric_home_v4`) |

---

## 📁 파일 구조

```
fabric-calc/
├── index.html          ← 앱 전체 (HTML + CSS + JS 단일 파일)
├── products.json       ← Gist 동기화용 (앱이 자동 생성/관리)
└── README.md
```

---

## 🗂 현재 데이터 구조

### localStorage 키
- `fabric_home_v4` — 제품 배열
- `fabric_sync_v1` — GitHub Gist 동기화 설정

### 제품 데이터 구조 (현재)
```json
[
  {
    "name": "소창손수건(소)",
    "pieces": [
      {
        "name": "겉지",
        "w": 22,
        "h": 59,
        "qty": 2,
        "rotatable": false
      },
      {
        "name": "속지",
        "w": 19,
        "h": 22,
        "qty": 2,
        "rotatable": true
      }
    ]
  }
]
```

### 제품 데이터 구조 (다음 버전 — 추가 필요)
```json
[
  {
    "company": "혀니네홈패션",
    "name": "소창손수건(소)",
    "pieces": [
      {
        "name": "겉지",
        "w": 22,
        "h": 59,
        "qty": 2,
        "rotatable": false,
        "fabricName": "소창",
        "fabricWidth": 150
      },
      {
        "name": "속지",
        "w": 19,
        "h": 22,
        "qty": 2,
        "rotatable": true,
        "fabricName": "면혼방",
        "fabricWidth": 110
      }
    ]
  }
]
```

---

## 📱 화면 구성 (5개 screen)

| screen ID | 역할 |
|-----------|------|
| `screen-main` | 메인 — 제품 목록 |
| `screen-sync` | GitHub Gist 동기화 설정 |
| `screen-register` | 제품 등록/수정 |
| `screen-calc` | 재단 계산 입력 |
| `screen-result` | 계산 결과 |

화면 전환: `showScreen('screen-id')` 함수 호출

---

## ⚙️ 핵심 계산 함수

### `getBestFit(piece, fabricWidth)`
피스의 최적 배치 방향 계산 (회전가능 시 더 많이 들어가는 방향 선택)
```js
반환: { w, h, perRow, rotated }
```

### `calcPiecePlan(piece, fabricWidth, targetQty, guide, margin)`
단일 피스의 연단 계획 계산
```js
반환: { perRow, totalPieces, totalRows, sheetLen, fullSheets, remRows, remSheetLen, totalLen, w, h, rotated }
```

### `calcMixedLayout(pieces, fabricWidth, targetQties, margin, fabricLen)`
혼합 배치 핵심 엔진.
- `targetQties`: 피스별 목표수량 배열. `null` = 최대한 욱여넣기
- `fabricLen`: 원단 길이 (`null`이면 무제한)
- 1단계: 필수 피스 배치하면서 자투리 폭에 최대한 피스 즉시 끼워넣기
- 2단계: 필수 피스 완료 후 남은 원단에 최대한 피스 전용 줄 추가
```js
반환: { rows, totalLen, cut, needs, pieces, fits }
```

### `buildBriefing(pieces, fabricW, fabricLen, targetQties, layout)`
계산 과정 수식 브리핑 텍스트 생성 (모드C 결과에 표시)

### `drawFabricCanvas(canvasId, fabricW, rows, margin, pieces)`
Canvas로 배치 시각화. 원단 폭 = 화면 가로폭으로 1:1 비율 매핑.
피스 안 글씨 없음, 하단 범례로 색상 구분.

---

## 🧮 계산 모드 3가지

### 모드 A — 원단 길이 → 최대 수량
입력: 원단 폭 + 길이  
출력: 최대 생산 가능 세트 수 + 각 피스별 연단 계획  
피스 2개 이상: 혼합 배치도 계산해서 비교 표시

### 모드 B — 목표 수량 → 필요 길이
입력: 원단 폭 + 목표 수량  
출력: 필요한 원단 총 길이 (피스별 합산)  
피스 2개 이상: 혼합 배치 계산해서 비교

### 모드 C — 원단 + 피스별 목표 → 최적 배치
입력: 원단 폭 + 길이 + 피스별 목표수량(또는 "최대한" 체크박스)  
출력: 가능 여부 + 피스별 달성 현황 + 수식 브리핑 + 혼합 배치도  
핵심: 자투리 폭에 최대한 피스 끼워넣기 계산

---

## 🎨 디자인 토큰 (CSS 변수)

```css
--bg: #F5F3EE          /* 앱 배경 */
--surface: #FFFFFF      /* 카드 배경 */
--surface2: #F0EEEC     /* 보조 배경 */
--border: #E0DCD6       /* 테두리 */
--text: #1A1814         /* 기본 텍스트 */
--text2: #6B6660        /* 보조 텍스트 */
--text3: #A8A4A0        /* 힌트 텍스트 */
--accent: #2D5A3D       /* 주색 (초록) */
--accent2: #E4EDE6      /* 주색 연하게 */
--danger: #B03020       /* 위험 (빨강) */
--warn: #7A5C10         /* 경고 (갈색) */
--radius: 16px          /* 카드 모서리 */
--radius-sm: 12px       /* 작은 모서리 */
```

피스 색상 배열: `['#2D5A3D', '#B03020', '#1A4A7A', '#7A5C10']`

---

## 📌 UI 원칙 (아이폰 13 미니 최적화)

- 최소 터치 영역: `min-height: 44px` (버튼), `min-height: 56px` (주요 버튼)
- 입력 폰트: `font-size: 18px` (아이폰 자동 줌 방지)
- 스크롤: `-webkit-overflow-scrolling: touch`
- 상단 safe area: `padding-top: 52px` (헤더)
- 하단 safe area: `padding-bottom: max(14px, env(safe-area-inset-bottom))`
- `touch-action: manipulation` (더블탭 줌 방지)
- 피스 안 글씨 없음 (Canvas 시각화에서 작은 폰트 깨짐 방지)

---

## ☁️ GitHub Gist 동기화

```js
// 설정 저장 위치
localStorage['fabric_sync_v1'] = {
  token: 'ghp_...',   // 사용자 입력 (코드에 하드코딩 절대 금지)
  gistId: '...',      // 첫 업로드 시 자동 생성
  autoSync: true/false
}

// Gist 파일명
'fabric_data.json'

// Gist description
'홈패션 연단 계산기 데이터'
```

**중요**: 토큰은 절대 코드에 하드코딩하지 말 것. 공개 저장소라서 GitHub이 자동 감지 후 토큰 무효화함.

---

## 🚧 다음 버전 작업 목록 (우선순위 순)

### 1. 업체 → 제품 계층 구조 추가
- 제품 등록 시 `company` 필드 추가
- 메인 화면에서 업체별 그룹핑 표시
- 업체 탭 또는 접기/펼치기 방식

### 2. 피스별 원단 정보 저장
각 피스에 `fabricName`(자유 입력)과 `fabricWidth` 추가:
```js
{
  name: '겉지',
  w: 22, h: 59, qty: 2, rotatable: false,
  fabricName: '소창',      // 사용자가 자유롭게 입력
  fabricWidth: 150         // cm
}
```

### 3. 계산 화면 원단 폭 빠른 선택 UI
현재: 숫자 직접 입력만 가능  
개선: 해당 제품 피스들의 저장된 원단을 버튼으로 표시
```
원단 폭 (cm)
[소창 150] [면혼방 110]  ← 탭하면 자동입력
[────────────────]
│    150    │  ← 직접 수정도 가능
```
- 버튼 탭 → 폭 자동입력 + 버튼 활성화
- 숫자 직접 수정 → 버튼 선택 해제
- 저장된 원단 없으면 버튼 없이 입력란만

### 4. 결과 화면 Canvas 시각화 개선
- 현재: 4번째 줄(대×2 + 소×5)에서 대 피스 높이가 소 피스보다 커서 시각적으로 어색함
- 개선: 혼합 줄에서 각 피스의 실제 높이를 표시하고, 높이 차이 있는 경우 피스가 줄 위쪽에 붙도록

---

## 🐛 알려진 버그 / 주의사항

1. **피스 높이 조건**: 현재 `jFit.h <= rowH * 1.5` 조건으로 혼합 배치 허용. 이 값보다 낮으면 소 피스(22cm)가 대 피스(59cm) 줄에 못 들어감.

2. **removePiece**: `piece-uid-N` 형식의 고유 ID 사용. 중간 삭제 시 ID 꼬임 없음.

3. **모드C 체크박스**: `modeC-max-{i}` 체크 시 해당 피스 input 비활성화. `toggleModeCMax(i)` 함수 참조.

4. **야드 변환**: 1야드 = 91.44cm

5. **Gist 토큰 노출 금지**: 공개 저장소 → 토큰 하드코딩 시 GitHub 자동 차단

---

## 💡 개발 패턴 / 컨벤션

- **바닐라 JS만 사용** (라이브러리 없음, CDN 없음)
- **var 사용** (IE 호환성 아닌 기존 패턴 유지)
- **인라인 이벤트**: `onclick="function()"` 방식
- **HTML 문자열 생성**: `html += '...'` 패턴으로 동적 렌더링
- **Canvas 시각화**: `window.devicePixelRatio` 적용으로 레티나 대응
- **XSS 방어**: `esc()` 함수로 모든 사용자 입력 이스케이프

---

## 📞 사용자 환경

| 항목 | 내용 |
|------|------|
| 주 사용 기기 | 아이폰 13 미니 (375px) |
| 브라우저 | 사파리 |
| 사용 장소 | 재단 작업장 (현장) |
| 공유 사용 | 배우자도 같은 앱 사용 (Gist 동기화로 공유) |
| PC | 가끔 제품 등록 시 사용 |

---

## 🔧 클로드 코드 작업 시 주의사항

1. **단일 파일 유지**: `index.html` 한 파일에 HTML/CSS/JS 모두 포함
2. **외부 라이브러리 사용 금지**: CDN 포함 일체 불가 (오프라인 고려)
3. **모바일 퍼스트**: 모든 UI 변경 시 375px 기준으로 설계
4. **터치 친화적**: 버튼 최소 44px, 입력 최소 52px
5. **GitHub 배포**: 수정 후 `index.html`을 저장소에 푸시하면 자동 배포
6. **데이터 마이그레이션**: 스토리지 키 변경 시 기존 `fabric_home_v4` 데이터 마이그레이션 코드 필요
7. **토큰 절대 하드코딩 금지**

