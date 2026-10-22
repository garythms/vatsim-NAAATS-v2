# Flight data display changelog

Version shown in the header (`src/config.json`). Bump it whenever the display changes.

## 0.11.3 - 2026-10-22
- New install guide at /guide (linked from the download page), with the AeroNav Gander package link and a download button for the newest plugin pack.

## 0.11.2 - 2026-10-22
- The aircraft menu "NatTrak" tab is now "RCL REQ" (the clearance-request reminder message).

## 0.11.1 - 2026-10-22
- Flight plan panel: ACID and Datalink boxes narrower so the ATC box is not cut off.

## 0.11.0 - 2026-10-22
- "Remove" is now "Hide": it hides the strip for you only (this browser, 12 hours). Nobody else's display changes. Bring a strip back from the "Hidden" list in the top bar or by typing its callsign in Add callsign.
- Sector and State boxes merged into one "ATC" box: the callsign of the controller tracking the flight (green), or who the pilot is tuned to on VATSIM (oceanic green, domestic pale green), red UNICOM, or red NONE.

## 0.10.2 - 2026-10-22
- The "Entry time" box is now labelled "Entry".

## 0.10.1 - 2026-10-22
- Flight plan panel: Dest now sits next to Depart; "Etd" replaced by the editable "Entry time" (time at the oceanic entry fix); SELCAL moved to the Route row beside the route box.
- State box now shows CTL when an ATC position is tracking the flight, "--" otherwise (it used to show a fixed "UC").

## 0.10.0 - 2026-10-22
- Remove a strip: aircraft menu > Remove, with a warning first. The strip stays hidden for 12 hours unless someone adds it again.
- Add a strip: type a callsign in "Add callsign" (top bar) and press Add. The flight is looked up on VATSIM and stays on the display.
- Entry time: edit the "Entry" box in the flight plan panel and all the other fix times move by the same amount.

## 0.9.10 - 2026-10-22
- Flight plan panel: removed the blue callsign box and the blue route/times list (the Route (edit) box now uses the space).
- "Datalink" now means CPDLC: green "CPDLC on" when the pilot is logged on to the vNAAATS CPDLC station, red "No CPDLC" otherwise (it used to say Connected when a natTrak request existed).

## 0.9.9 - 2026-10-21
- A flight tracked by an oceanic controller (CZQO, EGGX, NAT_FSS ...) is never hidden by the traffic window: its strip stays until it is untracked and for 5 minutes afterwards. (The 5 minutes is counted by the open browser page, so a page refresh restarts the count.)

## 0.9.8 - 2026-10-21
- Headset icon: aircraft that are online but not on any controller or UNICOM now show a red crossed-out headset ("Not tracked by any controller").
- Transfer list only offers the stations in the TopSky list (topsky_cpdlc.txt), not every controller online. Needs server 0.11.1. With none online the Transfer tab says "No adjacent ATC - Send to UNICOM".

## 0.9.7 - 2026-10-21
- Headset icon now shows who the pilot is talking to, taken from the VATSIM radio frequency they have tuned: bold green = oceanic controller, faded green = domestic controller, orange = UNICOM 122.800. Hover shows e.g. "Tracked by NAT_FSS (133.500)". If VATSIM has no radio info it falls back to who is tracking the aircraft in EuroScope. Needs server 0.11.0.

## 0.9.6 - 2026-10-21
- "Group" now also has "by flight level": flights are grouped under FL400, FL380, ... highest first, each heading showing how many aircraft.

## 0.9.5 - 2026-10-21
- Tracks panel: the validity time is shown once on the heading ("Westbound 1130-1900z") instead of on every track. Each track is two lines: its name and fixes, then its flight levels. (Also fixed minutes 09 being shown as "9".)

## 0.9.4 - 2026-10-21
- Strip list rebuilt on one grid shared by every strip and header row: all fix times (including the exit fix) always show and line up under their fix names, at any window width. The columns scale with the list text, and the dead space between callsign / route / level is gone.

## 0.9.3 - 2026-10-21
- The tracking controller is now a headset icon (hover: "Tracked by NAT_FSS"). Flights cleared on natTrak show a clipboard-tick icon instead of the word NATTRAK (hover: "RCL processed at 1613z (natTrak)"; natTrak does not currently say which controller processed it, so the name only shows if natTrak starts sending it).
- Aircraft type and the tracking icon have their own space, so they no longer overlap; long rows are clipped instead of running into the right-hand column.

## 0.9.2 - 2026-10-21
- SELCAL is checked against the 16 SELCAL letters (A-H, J-M, P-S). A red warning in the menu says which letters are not allowed, e.g. "SELCAL cannot use W".

## 0.9.0 - 2026-10-21
- "Send" buttons for SELCAL test, Contact me and the natTrak message: the plugin (1.5b4, with `autotype=1` in vnaaats.cfg) brings EuroScope to the front and types the command into its command line. If that is switched off or Windows refuses, the command is copied instead and the menu says so. Supervisor stays copy-and-paste.

## 0.8.0 - 2026-10-21
- NatTrak tab in the aircraft menu: copies the EuroScope message (.msg CALLSIGN Hello, you will soon be entering oceanic airspace...) and "I have sent it - mark with red *" puts a red * next to the callsign on the strip (and, with plugin 1.5b3, on the EuroScope tag). The * clears by itself when the pilot requests clearance on natTrak or an oceanic controller (CZQO, CZQX, EGGX, NAT, BIRD, KZWY, LPPO, ENOB) tracks the flight. "Remove mark" clears it by hand.

## 0.7.5 - 2026-10-21
- Strip columns now have fixed widths (departure/arrival, level, Mach, each ETA) and the track header row uses the same widths, so level/Mach and the fix names no longer run into each other and the times line up under their fixes. The tracking controller (e.g. NAT_FSS) is no longer cut off.

## 0.7.4 - 2026-10-21
- Flights whose pilot is no longer on the VATSIM network are hidden (when the VATSIM feed is working). The selected flight, CPDLC-connected flights and pending natTrak requests still show. If the feed is unavailable, nothing is hidden for this reason.

## 0.7.3 - 2026-10-21
- Flights with no times at all are now judged by where the pilot is (live VATSIM position): they only show when within 200 nm of a fix on their route. If the pilot's position is not known they still show. (Server 0.9.1 sends the distance.)

## 0.7.2 - 2026-10-21
- Route can be amended in the flight plan panel ("Route (edit)"). Latitude/longitude fixes must be two numbers / two numbers (51/20); 51N20W and similar are refused with a red warning as you type. Fixes you keep keep their times; new fixes have none. The change is on the display and server only - EuroScope's own flight plan is not changed.

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
