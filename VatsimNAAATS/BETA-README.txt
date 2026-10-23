vNAAATS V2 plugin - BETA 1.4b3 (testers)
=========================================

INSTALL
1. Close EuroScope.
2. Put VatsimNAAATS.dll in your EuroScope Plugins folder (any folder is fine).
3. Copy vnaaats.example.cfg to the SAME folder as the DLL and rename it vnaaats.cfg.
4. Open vnaaats.cfg in Notepad and fill in:
      server=https://vnaaats.com
      key=<the key you were sent - keep it private>
      squawk=1
   (squawk=0 turns off automatic squawk 2000 on oceanic clearance)
5. Start EuroScope > Other SET > Plug-ins > Load, pick VatsimNAAATS.dll.
6. If it will not load, install the Microsoft Visual C++ 2015-2022 Redistributable (x86).

NEW IN 1.4b3
- CPDLC handoff: when you hand an aircraft that is on CPDLC to another controller (flight plan window > Co-ordination / Active Authorities > Transfer), the pilot is sent "CONTACT <station name> ON <frequency>, BYE." and is logged off CPDLC when they reply WILCO/ROGER.
- UNICOM (button in the FDD CPDLC drawer): the pilot is told there is no further ATC, logged off when they reply, and the tag is released in EuroScope.

ALSO IN 1.4b2
- Radar targets are now aircraft icons (pointing along the heading) instead of stars and diamonds. Filled = cleared, outline = not cleared yet.

ALSO IN 1.4b1
- CPDLC: an aircraft connected to CPDLC shows its callsign in brackets on the radar tag, e.g. [BAW123].
- The server is now https://vnaaats.com (secure). The FDD is at https://vnaaats.com/fdd. If you used the old IP address in vnaaats.cfg, change it.
- SELCAL box in the flight plan window is editable (4 letters, A-H J-M P-S, e.g. ASCM or AS-CM).
  Clear the box to go back to the SEL/ code in the remarks.
- FDD button opens your own server's flight data display (<server>/fdd).
- Beta/update links point to the GitHub project.
- Your callsign is sent with every change, so the FDD shows who last changed a flight.
- If a controller changes level, Mach, track or SELCAL on the web FDD, your flight plan window updates by itself within a few seconds.

WHAT TO TEST
- Track list loads, flight plans open, Clear/Unclear works.
- Squawk 2000 assigned on oceanic clearance.
- Edit SELCAL, close and reopen the flight plan - the value should stay.
- The FDD (menu bar button) shows your aircraft.

REPORTING BUGS
https://github.com/garythms/vatsim-naaats-v2/issues
Include: callsign, what you clicked, what you expected, what happened, plugin version (shown when it loads),
and the log file (vnaaats log in the DLL folder or EuroScope folder) if you can find it.

NEW IN 1.5b1: FDD commands
Click a callsign on the web Flight Data Display for the aircraft menu. Track, Untrack and Transfer are carried out by this plugin in EuroScope
(it checks the server every 2 seconds, so keep EuroScope connected as your position). SELCAL test, Contact me and Supervisor copy the
EuroScope command for you to paste into the command line.

NEW IN 1.5b3: a red * beside the callsign on the radar tag means a natTrak reminder was sent to that pilot from the FDD (aircraft menu > NatTrak).
It disappears when the pilot requests clearance or an oceanic controller tracks the aircraft.

NEW IN 1.5b16: no feature changes - this build tests the automatic Discord update notice. Handoff accept via Track needs 1.5b15 or newer.

NEW IN 1.5b15: fixes a crash in 1.5b14 when new aircraft appeared (use 1.5b15, not 1.5b14).

NEW IN 1.5b14:
- Note / scratchpad: the note box on the FDD flight plan is the EuroScope scratchpad. It shows under the tag and goes with the aircraft to the next controller; notes typed in EuroScope show on the FDD.
- Track on the FDD now accepts a handoff that is being offered to you.
- Contact Me / SELCAL from the FDD: EuroScope's .contactme and .selcal act on the SELECTED aircraft and take no callsign. The plugin now selects the aircraft in EuroScope first.

NEW IN 1.5b13: typing into EuroScope (SELCAL / contact me / RCL REQ message) now works step by step on its own thread, clearing the command line first, so old text no longer gets mixed into the new command.

NEW IN 1.5b12: the "not equipped" * is now worked out again from the EuroScope flight plan each time, so an aircraft whose PBN/ remarks include B1 or B2 no longer keeps a stale star.

NEW IN 1.5b11: fixed a possible crash when reading an aircraft's equipment (PBN remarks at the end of a flight plan). A red-free * under the callsign on a tag means the aircraft is NOT AGCS equipped.

NEW IN 1.5b10: the update pop-up now opens the vNAAATS download page and says to replace only VatsimNAAATS.dll and keep vnaaats.cfg. The flight plan window "Datalink" box now shows CPDLC ON / CPDLC OFF (whether the pilot is logged on to the vNAAATS CPDLC station).

NEW IN 1.5b9: Send buttons (SELCAL, Contact, NatTrak message) now type one command at a time and replace anything already in the EuroScope command line, so two commands are no longer joined into one line.

NEW IN 1.5b8: conflict alerts no longer flash for aircraft one flight level apart (a 200 ft allowance is applied to the vertical separation, because real altitudes are never exactly on the level).

NEW IN 1.5b7 (and 1.5b6): fixed a crash on opening when the plugin could not reach the server (it now shows an error message instead, with a code).

NEW IN 1.5b5: start-up logging and protection (vNAAATS.log shows each start-up step). If EuroScope still crashes on opening, send vNAAATS.log.

vNAAATS.asr (in the zip)
This is the vNAAATS radar screen layout. Its sector-file lines are left blank, like the AeroNav ASR files, and it has no personal settings in it, so it should open with whatever Gander sector file you have loaded. Put it in your EuroScope ASR folder, or open it from Open SCT/ASR. If EuroScope still shows a "different sector file" warning, pick your latest Gander (EXCXO) sector file, then press Save once.
