# Flight data display changelog

Version shown in the header (`src/config.json`). Bump it whenever the display changes.

## 0.3.0 - 2026-10-20
- Controller login (password now, VATSIM login when switched on) and a position screen (CZQO_CTR, EGGX_CTR ...). "Just watch" still works with no login.
- Level, Mach, track and SELCAL can be edited in the flight plan panel; the change is saved on the server and picked up by the plugin, including the open flight plan window.
- "Last change HHMMz by POSITION (name)" in the flight plan panel and a pencil on strips changed in the last 10 minutes.
- Warnings: same track + level entering within 10 minutes, or a level that is not on the track.
- Search box, group by track or entry fix, sort by ETA / callsign / level / last changed.
- Header shows both the display version and the server version.

## 0.2.2 - 2026-10-20
- Every route waypoint is shown as four digits, 52/50, whatever format the flight plan used (52N050W, 5230N05000W, 4545N ...); minutes are dropped (4730N05000W shows as 47/50). Random-route strips are now a single line.

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
