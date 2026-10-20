vNAAATS V2 plugin - BETA 1.3b4 (testers)
=========================================

INSTALL
1. Close EuroScope.
2. Put VatsimNAAATS.dll in your EuroScope Plugins folder (any folder is fine).
3. Copy vnaaats.example.cfg to the SAME folder as the DLL and rename it vnaaats.cfg.
4. Open vnaaats.cfg in Notepad and fill in:
      server=http://49.12.254.143
      key=<the key you were sent - keep it private>
      squawk=1
   (squawk=0 turns off automatic squawk 2000 on oceanic clearance)
5. Start EuroScope > Other SET > Plug-ins > Load, pick VatsimNAAATS.dll.
6. If it will not load, install the Microsoft Visual C++ 2015-2022 Redistributable (x86).

NEW IN 1.3b4
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
