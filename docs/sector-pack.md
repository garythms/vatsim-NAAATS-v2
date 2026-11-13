# Sector pack auto-update (FDD for Windows)

After each AeroNav push, publish the same vNAAATS zip here so the app can install it for controllers (AeroNav downloads need a login, so the app cannot fetch from there):

1. GitHub repo > Releases > open the release tagged `sector-pack` (create it once: tag `sector-pack`, mark it **pre-release** so it is never "latest", because the server reads the latest release for the plugin DLL).
2. Delete the old zip, then attach the new one (the `EXCXO-vNAAATS` zip from AeroNav). Replacing the file is what tells the app there is an update.
3. Next time a controller opens the FDD for Windows, it offers "A newer sector pack". Update now unpacks the zip over their EXOCO folder, only while EuroScope is closed, and never touches `vnaaats.cfg`.

The zip must have the same folder layout the controllers already unzip into EXOCO by hand.
