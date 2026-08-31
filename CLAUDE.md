# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

홈패션(가정용 섬유) 원단 재단 계획을 계산하는 모바일 웹앱. 단일 사용자가 아이폰 13 미니(375px, Safari)에서 재단 현장에서 사용. GitHub Pages로 배포되고, GitHub Gist를 통해 기기 간 데이터를 동기화한다.

- Repo: `psw9715-debug/fabric-calc`
- Live: `https://psw9715-debug.github.io/fabric-calc`
- Entire app is one file: `index.html` (HTML + CSS + JS, no build step, no dependencies)
- `products.json` exists only as a Gist-sync artifact; it's not source data to edit by hand.

작업 환경은 **GitHub 저장소 하나뿐**이다. 로컬 PC 작업 폴더도, 예전에 있던 중첩 클론 `fabric-calc/fabric-calc/`도 더 이상 없다. 정본은 `main` 브랜치의 최상위 `index.html`이다. 사용자는 iPhone의 Claude 앱으로 작업하므로, 코드는 **부분 diff가 아니라 완성된 `index.html` 전체**로 전달한다 (자세한 내용은 `HANDOVER.md`).

`index.html`을 고치기 전에 `backup/index_YYYYMMDD-HHMM.html`(한국 시간)로 수정 직전 상태를 복사해 둔다. 되돌릴 때는 그 파일을 `index.html`로 덮어쓰면 된다.

## Commands

No build/lint/test tooling — it's a static HTML file. To develop:
- Open `index.html` directly in a browser, or serve the directory (e.g. `python -m http.server`) to test on a phone via LAN IP.
- Deploy = commit and push `index.html` to the repo; GitHub Pages serves it automatically.
- Test on a 375px viewport (iPhone 13 mini) as the primary target.

## Architecture

Everything lives in `index.html` as five `.screen` divs shown/hidden via `showScreen('screen-id')`:

| screen | role |
|---|---|
| `screen-main` | product list (home), grouped by company |
| `screen-sync` | GitHub Gist sync settings |
| `screen-manage` | company / fabric-list maintenance (⚙️ in main header) |
| `screen-register` | product create/edit (피스 치수만 — 원단 입력란 없음) |
| `screen-calc` | cutting-layout calculation input |
| `screen-result` | calculation results |

### Data model

- `localStorage['fabric_home_v4']` — array of products. Schema: `{ company, name, pieces: [{ name, w, h, qty, rotatable }] }`. 피스는 **원단 정보를 갖지 않는다** — 원단은 업체 단위(`fabric_vendors_v1`)로만 관리한다.
- `localStorage['fabric_sync_v1']` — `{ token, gistId, autoSync }` for Gist sync. **Never hardcode a GitHub token in code** — this is a public repo, GitHub auto-revokes committed tokens. Tokens are only ever entered by the user at runtime.
- `localStorage['fabric_vendors_v1']` — `{ [companyName]: [{ name, width }] }`. 업체별 원단 목록이자 원단 정보의 **유일한 저장소**. 실무에서 원단은 업체 단위로 고정되므로 제품마다 다시 입력하지 않는다. 제품 데이터와 완전히 분리돼 있어서, 이 맵이 없는 기기에서도 제품 계산은 정상 동작한다(계산 화면에서 폭을 직접 입력하면 된다). 관리 화면(`screen-manage`)에서 추가·수정·삭제하고, 계산 화면은 이 목록을 폭 빠른선택 버튼으로 쓴다. Gist 동기화와 파일 백업 payload는 `{ products, vendors }` 형태로, 원단 목록도 같이 싣는다(이전 형식인 배열/`{products}`만 있는 파일도 계속 읽힌다). 받아오기는 제품과 마찬가지로 원단 목록도 통째로 덮어쓴다.
- `localStorage['fabric_ui_v1']` — `{ [companyName]: collapsedBool }`, the main list's fold state.
- Gist filename: `fabric_data.json`; description: `홈패션 연단 계산기 데이터`.
- Product identity is **(company, name)**, not name — the same product name legitimately recurs across companies (`손수건` under two vendors). `findDuplicate()` scopes its check to one company; never dedupe on name alone.
- `migrateProducts()` runs on every read path (localStorage load, Gist pull, file restore). `company:''`를 백필하고, 피스에 붙어 있던 예전 원단 정보(`fabricName`/`fabricWidth`, 그리고 그 다음 세대의 `fabrics[]`)를 **그 제품 업체의 `fabric_vendors_v1` 목록으로 옮긴 뒤 피스에서 지운다**. 업체가 빈 문자열이면 `companyLabel()` 규칙대로 `'기타'`로 모인다. 이름+폭이 같으면 중복 등록하지 않고, 여러 번 돌려도 결과가 같다(멱등). 새 읽기 경로는 반드시 이 함수를 거치게 하고, 필드가 있다고 가정하지 말고 이 함수를 확장할 것.

### Calculation engine

The core logic is a set of pure-ish functions operating on `piece = {w, h, qty, rotatable}`:

- `getBestFit(piece, fabricWidth)` → `{w, h, perRow, rotated}` — picks the orientation (rotating if allowed) that fits more pieces per row.
- `calcPiecePlan(piece, fabricWidth, targetQty, guide, margin)` → single-piece layout plan (`perRow, totalPieces, totalRows, sheetLen, fullSheets, remRows, remSheetLen, totalLen, w, h, rotated`).
- `calcMixedLayout(pieces, fabricWidth, targetQties, margin, fabricLen)` — the multi-piece layout engine: places required pieces row by row, greedily backfilling leftover row width with other pieces, then (if `fabricLen` allows) adds bonus rows once required quantities are met. `targetQties: null` means "pack as many as possible." Returns `{rows, totalLen, cut, needs, pieces, fits}`.
- `buildBriefing(pieces, fabricW, fabricLen, targetQties, layout)` — generates the human-readable formula breakdown shown in Mode C results.
- `drawFabricCanvas(canvasId, fabricW, rows, margin, pieces)` — renders the layout to `<canvas>`, 1:1 scaled to fabric width, DPR-aware. No text is drawn inside pieces (avoids font-rendering breakage at small sizes) — a color legend below the canvas identifies pieces instead.

Three calculation modes exist in the UI, all built on `calcMixedLayout`/`calcPiecePlan`:
- **Mode A**: fabric width + length → max producible sets.
- **Mode B**: fabric width + target quantity → required fabric length.
- **Mode C**: fabric width + length + per-piece targets (or "max" toggle per piece) → feasibility + per-piece achievement + formula briefing + layout diagram.

Known tuning knob: mixed-row eligibility uses `jFit.h <= rowH * 1.5` — lowering this threshold prevents smaller pieces from sharing a row with much taller ones.

Per-piece participation (`pieceRoles[i]`) gates every mode: `'off'` (excluded from this job), `'qty'` (target quantity), `'max'` (fill leftover). Modes A/B only offer off/qty. `calculate()` filters to `activePieceIdx()` before doing anything, so results, legends and canvases only ever contain pieces being cut. In `calcMixedLayout()` stage 2, multiple `'max'` pieces are assigned rows **round-robin** — picking the shortest piece each time let one piece monopolise the fabric.

Row geometry: each row carries `leftoverW`/`leftover` (unused fabric width) and `usedW`. `calcPiecePlan()` returns `fabricW`/`leftoverW` too, so single-piece layouts report their leftover strip as well. `rowWidthTableHtml()` and `widthUsage()` render those; keep both key names on row objects — `drawFabricCanvas()` reads `leftover`.

### Conventions

- Vanilla JS only — no libraries, no CDN (app must work offline/on a plane at a job site).
- `var`, not `let`/`const` — this is the existing style, not an IE compatibility requirement; match it.
- Inline event handlers (`onclick="fn()"`), not addEventListener wiring.
- HTML is built via string concatenation (`html += '...'`) and injected, not templating.
- Always pass user-facing text through `esc()` before interpolating into HTML strings (XSS guard).
- Piece IDs use `piece-uid-N` so mid-list deletion doesn't reindex/collide.
- 1 yard = 91.44cm is the hardcoded conversion used for yard output.

### Mobile UI constraints (design target: iPhone 13 mini, 375px, Safari)

- Touch targets: buttons ≥44px, primary buttons ≥56px tall.
- Inputs use `font-size:18px` specifically to prevent iOS Safari's auto-zoom-on-focus.
- `touch-action: manipulation` to suppress double-tap zoom.
- Safe-area padding: `padding-top:52px` on headers, `padding-bottom:max(14px, env(safe-area-inset-bottom))` on bottom-fixed content.
- CSS custom properties in `:root` define the color system (`--bg`, `--surface`, `--accent`, `--danger`, `--warn`, etc.) — reuse these tokens rather than introducing new colors.
