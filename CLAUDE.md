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
- **`index.html`을 고칠 때마다 스크립트 맨 위의 `APP_VERSION`을 올린다.** 관리 화면의 "최신 버전 가져오기"가 서버 파일에서 이 문자열을 정규식으로 다시 읽어 지금 떠 있는 화면과 비교하므로, 안 올리면 새 버전이 있어도 "이미 최신"이라고 나온다.
- Test on a 375px viewport (iPhone 13 mini) as the primary target.

## Architecture

Everything lives in `index.html` as five `.screen` divs shown/hidden via `showScreen('screen-id')`:

| screen | role |
|---|---|
| `screen-main` | product list (home), grouped by company |
| `screen-sync` | GitHub Gist sync settings |
| `screen-manage` | company / fabric-list maintenance + 앱 버전·업데이트 (⚙️ in main header) |
| `screen-register` | product create/edit (피스 치수만 — 원단 입력란 없음) |
| `screen-calc` | cutting-layout calculation input |
| `screen-result` | calculation results |

### Data model

- `localStorage['fabric_home_v4']` — array of products. Schema: `{ company, name, pieces: [{ name, w, h, qty, rotatable }] }`. 피스는 **원단 정보를 갖지 않는다** — 원단은 (업체 → 제품) 단위(`fabric_fabrics_v2`)로만 관리한다. 제품 식별자는 `(company, name)`이고, 원단 맵도 같은 키를 쓴다.
- `localStorage['fabric_sync_v1']` — `{ token, gistId, autoSync, repoSlug, repoPath, repoBranch, repoAuto, repoSha, repoSavedAt }`. `repo*`는 저장소 저장용(주 경로), `gistId`/`autoSync`는 예전 Gist 경로. **Never hardcode a GitHub token in code** — this is a public repo, GitHub auto-revokes committed tokens. Tokens are only ever entered by the user at runtime.
- `localStorage['fabric_fabrics_v2']` — `{ [companyName]: { [productName]: [{ name, width }] } }`. **업체 → 제품 → 원단** 3단이 원단 정보의 유일한 저장소다. 실무에서 `속지`/`겉지` 같은 이름은 제품마다 다른 원단을 가리키므로 업체 단위로 묶으면 구분이 안 된다(핀블랑 각티슈의 속지 ≠ 다른 업체 제품의 속지). 제품 데이터와 분리돼 있어서 이 맵이 없는 기기에서도 계산은 정상 동작한다(폭을 직접 입력하면 된다). 관리 화면(`screen-manage`)이 업체 → 제품 순으로 펼쳐 추가·수정·삭제하고, 계산 화면은 **그 제품의** 목록만 폭 빠른선택 버튼으로 쓴다.
  - 제품 이름·업체가 바뀌면 원단 목록도 따라가야 한다. `goToRegister()`/`duplicateProduct()`가 `registerOrigin`에 원래 위치를 기록해 두고, `saveProduct()`가 `moveProductFabrics()`로 옮긴다(복제는 복사). `deleteProduct()`는 같은 키를 쓰는 제품이 더 없을 때만 목록을 지운다.
  - 제품은 지웠는데 원단만 남은 항목은 관리 화면에 "지워진 제품"으로 표시하고 `dropGhostProduct()`로 정리한다.
  - 예전 업체 단위 목록(`fabric_vendors_v1`)은 v2 키가 없을 때 **한 번만** `promoteLegacyVendors()`로 승격된다(그 업체의 모든 제품에 복사). 승격 후 v2가 생기므로 다시 돌지 않아 지운 원단이 되살아나지 않는다.
  - 저장 payload(`syncPayloadObject()`)는 `{ products, fabrics, vendors }` — `fabrics`가 정본이고 `vendors`는 예전 버전 기기를 위한 업체 단위 평탄화본이다. 읽을 때는 `applyIncomingFabrics()`가 `fabrics` → 없으면 `vendors` 승격 순으로 처리한다.
- `localStorage['fabric_ui_v1']` — `{ [companyName]: collapsedBool }`, the main list's fold state.
- Gist filename: `fabric_data.json`; description: `홈패션 연단 계산기 데이터`.
- Product identity is **(company, name)**, not name — the same product name legitimately recurs across companies (`손수건` under two vendors). `findDuplicate()` scopes its check to one company; never dedupe on name alone.
- `migrateProducts()` runs on every read path (localStorage load, Gist pull, file restore). `company:''`를 백필하고, 피스에 붙어 있던 예전 원단 정보(`fabricName`/`fabricWidth`, 그리고 그 다음 세대의 `fabrics[]`)를 **그 제품의 `fabric_fabrics_v2` 목록((업체, 제품) 키)으로 옮긴 뒤 피스에서 지운다**. 업체가 빈 문자열이면 `companyLabel()` 규칙대로 `'기타'`로 모인다. 이름+폭이 같으면 중복 등록하지 않고, 여러 번 돌려도 결과가 같다(멱등). 새 읽기 경로는 반드시 이 함수를 거치게 하고, 필드가 있다고 가정하지 말고 이 함수를 확장할 것.

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

### GitHub 저장소 저장 (주 동기화 경로)

Gist·파일 대신 사용자의 저장소 안 JSON 파일 하나(`repoSlug` + `repoPath`, 기본 `psw9715-debug/fabric-calc` · `data/fabric_data.json`)에 `{app, version, updatedAt, vendors, products}`를 통째로 넣는다. Contents API(`GET/PUT /repos/{owner}/{repo}/contents/{path}`)만 쓴다.

- `repoSave(silent)` — 먼저 현재 파일을 읽어 `sha`를 얻고 PUT. 원격 `sha`가 저장해 둔 `repoSha`와 다르면 **다른 기기가 먼저 저장한 것**이므로 덮어쓰기 전에 확인을 받는다(자동 저장 중에는 조용히 건너뛴다 — 사용자 확인 없이 남의 저장을 지우지 않는다).
- `repoLoad()` — 받아서 `applyIncomingFabrics()`로 원단을 먼저 넣고 `migrateProducts()`를 태운 뒤 제품을 덮어쓴다. 받아온 `sha`를 `repoSha`에 저장해 다음 저장이 충돌로 잡히지 않게 한다.
- `checkRepo()` — 저장소 메타를 읽어 `default_branch`를 잡고, **공개 저장소면 경고를 띄운다**(업체·원단 이름이 인터넷에 공개되므로).
- 한글이 들어가므로 base64는 `TextEncoder`/`TextDecoder` 경유(`b64encode`/`b64decode`). `btoa(str)` 직접 호출 금지.
- `autoSyncIfEnabled()`가 저장소 저장과 Gist 업로드를 모두 태운다. 제품 저장·삭제뿐 아니라 **원단 추가·수정·삭제에서도 호출**해야 한다(원단이 제품에 안 붙어 있어서 제품 저장만으로는 안 올라간다).
- 토큰은 런타임 입력 → localStorage. 공개 저장소이므로 **코드에 절대 넣지 않는다**.

### 앱 업데이트 (사파리 캐시 우회)

사파리가 GitHub Pages의 `index.html`을 오래 붙잡고 있어서, 새로 배포해도 폰에서는 옛 화면이 뜬다. 관리 화면 아래 "앱 버전" 카드가 이걸 해결한다.

- `checkForUpdate()` — `location.pathname`을 `?_=타임스탬프` + `cache:'no-store'`로 다시 받아 `APP_VERSION`을 정규식으로 뽑아 비교한다. 다르면 바로 `forceReload()`, 같으면 "이미 최신", 실패하면 오프라인 안내(앱은 그대로 쓸 수 있다).
- `forceReload()` — Cache Storage를 비우고 `location.replace(origin+pathname+'?v='+Date.now())`. 쿼리를 매번 새로 붙이므로 `?v=`가 중첩되지 않는다. `#force-reload-btn`이 직접 호출하는 최후 수단이기도 하다.
- localStorage는 쿼리스트링이 달라져도 같은 오리진이라 그대로 남는다 — 업데이트해도 제품·원단 데이터는 유실되지 않는다.
- 정규식이 `APP_VERSION` **정의부**를 먼저 만나야 하므로, 이 변수는 항상 스크립트 최상단에 둔다.

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
