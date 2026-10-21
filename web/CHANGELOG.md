# Flight data display changelog

Version shown in the header (`src/config.json`). Bump it whenever the display changes.

## 0.7.1 - 2026-10-21
- Traffic window: by default the strip list only shows flights within 60 minutes of their oceanic entry estimate, until 5 minutes after their exit estimate. A "Show" box in the top bar switches to all flights (remembered in your browser) and a count shows how many are hidden. Flights with no times, the selected flight, CPDLC-connected flights and pending natTrak requests always show.

## 0.7.0 - 2026-10-21
- Click a callsign for the aircraft menu: Track (level, Mach, SELCAL), Untrack (asks to confirm), Transfer (online controllers), SELCAL, Contact me, Supervisor request (with a "No Contact" quick text), CPDLC, and Position reports (Concorde only).
- Track, Untrack and Transfer are sent to your EuroScope (plugin 1.5b1 or newer) and the result is shown in the menu.
- SELCAL test, Contact me and Supervisor copy the EuroScope command (.selcal / .contactme / .wallop) to paste into the command line, because plugins cannot run those themselves.

## 0.6.0 - 2026-10-21
- Concorde (type CONC) position reports: a "Position reports" button in the flight plan panel opens a table of every route fix with Calc, Est and Actual times (four digits, typed in by the controller; "now" fills the current time). Typed times replace the calculated times on the strip.
- A Concorde flashes (strip, flight plan button, red "overdue" count in the top bar, and the row in the table) when the next fix's time passed more than 3 minutes ago with no reported time.
- Mach is shown with three digits (M084, M200) so Concorde's Mach 2.00 displays correctly; the server now accepts Mach up to 2.99.

## 0.5.0 - 2026-10-21
- CPDLC sound for messages from pilots (your own sound file), instead of the beep.
- "Standard messages..." list in the CPDLC drawer (the TopSky list) and a UNICOM button (asks to confirm): sends "THERE IS NO FURTHER ATC AVAILABLE. SERVICE TERMINATED. MONITOR ADVISORY ON 122.800", logs the pilot off when they answer WILCO/ROGER and releases the tag in EuroScope.

## 0.4.0 - 2026-10-21
- New page /beta: always links the newest plugin beta download, with install steps. Homepage has a "Download plugin beta" button.
- CPDLC: aircraft connected to CPDLC show their callsign in brackets, e.g. [BAW123], on the strip and in the flight plan panel.
- New CPDLC drawer (button in the info bar, with unread count): connected aircraft, message thread, quick replies (UNABLE / STANDBY / ROGER), templates, send. Sound alert on new messages. Needs a controller position to send.
- Each controller signs in to CPDLC with their own Hoppie code (typed in the drawer, kept in server memory only). Stations: CZQO = CZQX, EGGX = EGGX, NAT_FSS = NATX.

## 0.3.1 - 2026-10-21
- Homepage: "Version 2 - Modernised by Gary Thomas" credit, copyright line updated (original (c) Andrew Ogden 2020-2021 kept, Version 2 (c) Gary Thomas 2026), Discord button removed, plugin box links to the Version 2 project, new "Dedicated CPDLC Interaction - coming soon" box.

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
