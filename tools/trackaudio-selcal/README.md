# TrackAudio with SELCAL tone transmit (personal test build)

Two patches on top of TrackAudio (pierr3/TrackAudio, GPL-3.0) and its afv-native fork. They add a `kTransmitTones` WebSocket
message that sends SELCAL tones on a frequency that is set to transmit, in place of the microphone, so the virtual cable is not needed.

Send `{"type": "kTransmitTones", "value": {"frequency": 118300000, "selcal": "HPCD"}}` to TrackAudio's WebSocket.
Custom tones: `"tones": [{"f1": 645.7, "f2": 1083.9, "ms": 1000}, {"f1": 0, "ms": 200}, ...]`, optional `"gain"` 0 to 1. Maximum 10 seconds.

The workflow `.github/workflows/trackaudio-selcal.yml` builds Windows installers from a pinned TrackAudio commit with the patches applied.
Run it from the Actions tab and download the artifact. This is an unofficial build for testing: it is not TrackAudio and not for sharing.
