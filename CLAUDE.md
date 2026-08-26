# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

홈패션(가정용 섬유) 원단 재단 계획을 계산하는 모바일 웹앱. 단일 사용자가 아이폰 13 미니(375px, Safari)에서 재단 현장에서 사용. GitHub Pages로 배포되고, GitHub Gist를 통해 기기 간 데이터를 동기화한다.

- Repo: `psw9715-debug/fabric-calc`
- Live: `https://psw9715-debug.github.io/fabric-calc`
- Entire app is one file: `index.html` (HTML + CSS + JS, no build step, no dependencies)
- `products.json` exists only as a Gist-sync artifact; it's not source data to edit by hand.

There is a duplicate nested clone of this same repo at `fabric-calc/fabric-calc/` (its own `.git`, tracking `main` instead of `master`, otherwise identical `index.html`). Treat the top-level `index.html` as the one to edit; don't let edits diverge between the two checkouts.

## Commands

No build/lint/test tooling — it's a static HTML file. To develop:
- Open `index.html` directly in a browser, or serve the directory (e.g. `python -m http.server`) to test on a phone via LAN IP.
- Deploy = commit and push `index.html` to the repo; GitHub Pages serves it automatically.
- Test on a 375px viewport (iPhone 13 mini) as the primary target.

## Architecture

Everything lives in `index.html` as five `.screen` divs shown/hidden via `showScreen('screen-id')`:

| screen | role |
|---|---|
| `screen-main` | product list (home) |
| `screen-sync` | GitHub Gist sync settings |
| `screen-register` | product create/edit |
| `screen-calc` | cutting-layout calculation input |
| `screen-result` | calculation results |

### Data model

- `localStorage['fabric_home_v4']` — array of products, each `{ name, pieces: [{ name, w, h, qty, rotatable }] }`.
- `localStorage['fabric_sync_v1']` — `{ token, gistId, autoSync }` for Gist sync. **Never hardcode a GitHub token in code** — this is a public repo, GitHub auto-revokes committed tokens. Tokens are only ever entered by the user at runtime.
- Gist filename: `fabric_data.json`; description: `홈패션 연단 계산기 데이터`.
- Product schema: `{ company, name, pieces: [{ name, w, h, qty, rotatable, fabricName, fabricWidth }] }`. `migrateProducts()` backfills `company:''`, `fabricName:''`, `fabricWidth:0` on every read path (localStorage load, Gist pull, file restore) — route any new read path through it, and extend it rather than assuming new fields exist.

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
