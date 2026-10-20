# Flight data display changelog

Version shown in the header (`src/config.json`). Bump it whenever the display changes.

## 0.2.1 - 2026-10-20
- natTrak alerts can be switched on/off separately: Visual (flashing, tab title) and Audio.
- Choice of alert sound: Ping, Chime or Beep (click Tone to cycle; plays a preview).
- Clicking a pending request (box row or its flashing strip) opens natTrak in a small pop-up window on that track's pending list.

## 0.2.0 - 2026-10-20
- Runs against the own vNAAATS server (old api.vnaaats.net backend is gone).
- Fixed "No data on remote" never clearing, and the wrong "updated recently" check.
- Strip text scales with the window width (no more overlapping columns on narrow windows).
- Working HL / LL / ALL TK / RR filter buttons and Default reset.
- Working PST / FaD / WST / TERM checkboxes (interpretation shown as tooltips).
- Oceanic-bound flights from the public VATSIM data feed are shown alongside plugin flights.
- natTrak Information panel connected to the public natTrak API.
- natTrak request box (PENDING / CLEARED), flashing strips and a ping sound for new pending requests.

## 0.1 - original
- vNAAATS-web as published by the vNAAATS team (Apache-2.0).
