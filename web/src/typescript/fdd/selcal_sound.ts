/*
 *  The sound a pilot hears when a controller calls them with SELCAL, played in the display when you press Send on a SELCAL.
 *  A SELCAL is four characters sent as two pairs (AB-CD): both tones of a pair together for about 1 second, a pause of 0.2 seconds, then the second pair.
 *  The 16 original tones are letters A-S (no I, N or O). SELCAL 32 adds 16 more tones: T-Z and the digits 1-9.
 *  Frequencies are the ones in the ICAO SELCAL 32 proposal (ICAO SAM/IG/23 NE/12) and the ASRI SELCAL guide. They are only used here to make a matching sound.
 */
import { writable, get } from "svelte/store";
import * as dataHandler from "./data";

export const TONES: Record<string, number> = {
  A: 312.6, B: 346.7, C: 384.6, D: 426.6, E: 473.2, F: 524.8, G: 582.1, H: 645.7,
  J: 716.1, K: 794.3, L: 881.0, M: 977.2, P: 1083.9, Q: 1202.3, R: 1333.5, S: 1479.1,
  T: 329.2, U: 365.2, V: 405.0, W: 449.3, X: 498.3, Y: 552.7, Z: 613.1,
  "1": 680.0, "2": 754.2, "3": 836.6, "4": 927.9, "5": 1029.2, "6": 1141.6, "7": 1266.2, "8": 1404.4, "9": 1557.8,
};
export const PULSE_S = 1.0;
export const GAP_S = 0.2;

/** The two pulses for a code: [[f1, f2], [f3, f4]] in Hz, or null if it is not a valid SELCAL (four different characters from the 32 tones). */
export function selcalPulses(code: string): [[number, number], [number, number]] | null {
  const c = String(code || "").toUpperCase().replace(/[-\s]/g, "");
  if (c.length !== 4 || new Set(c).size !== 4) return null;
  const f = [...c].map((ch) => TONES[ch]);
  if (f.some((x) => !x)) return null;
  return [[f[0], f[1]], [f[2], f[3]]];
}

export const selcalSoundOn = writable<boolean>((() => { try { return localStorage.getItem("fdd.selcalSound") !== "off"; } catch (e) { return true; } })());
export function setSelcalSound(on: boolean): void {
  selcalSoundOn.set(on);
  try { localStorage.setItem("fdd.selcalSound", on ? "on" : "off"); } catch (e) { /* not saved */ }
}

let actx: AudioContext | null = null;
/** Play the chime for a code. Respects the FDD sound switch and its own switch. Does nothing for a code with a repeated or unknown character. */
export function playSelcal(code: string): void {
  if (!dataHandler.soundOn || !get(selcalSoundOn)) return;
  const pulses = selcalPulses(code);
  if (!pulses) return;
  try {
    actx = actx ?? new (window.AudioContext || (window as any).webkitAudioContext)();
    if (actx.state === "suspended") void actx.resume();
    const t0 = actx.currentTime + 0.05;
    pulses.forEach((pair, i) => {
      const start = t0 + i * (PULSE_S + GAP_S);
      pair.forEach((hz) => {
        const o = actx!.createOscillator(), g = actx!.createGain();
        o.type = "sine"; o.frequency.value = hz; o.connect(g); g.connect(actx!.destination);
        g.gain.setValueAtTime(0, start);
        g.gain.linearRampToValueAtTime(0.12, start + 0.02);
        g.gain.setValueAtTime(0.12, start + PULSE_S - 0.03);
        g.gain.linearRampToValueAtTime(0, start + PULSE_S);
        o.start(start); o.stop(start + PULSE_S + 0.02);
      });
    });
  } catch (e) { /* no audio */ }
}
