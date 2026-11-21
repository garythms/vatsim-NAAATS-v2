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

// ---- Sending the tones on your frequency (optional). TrackAudio cannot play sound itself, so the tones are played into a virtual audio cable
// that TrackAudio uses as its microphone, and the display presses and releases TrackAudio's push-to-talk around them (ws://127.0.0.1:49080/ws on this computer).
const LS_FREQ = "fdd.selcalFreq", LS_DEV = "fdd.selcalDevice", LS_DEVNAME = "fdd.selcalDeviceName";
function lsGet(k: string): string { try { return localStorage.getItem(k) ?? ""; } catch (e) { return ""; } }
function lsSet(k: string, v: string): void { try { if (v) localStorage.setItem(k, v); else localStorage.removeItem(k); } catch (e) { /* not saved */ } }
export const selcalFreqOn = writable<boolean>(lsGet(LS_FREQ) === "on");
export const selcalDevice = writable<string>(lsGet(LS_DEV));
export function setSelcalFreq(on: boolean): void { selcalFreqOn.set(on); lsSet(LS_FREQ, on ? "on" : ""); }
export function setSelcalDevice(id: string, name: string): void { selcalDevice.set(id); lsSet(LS_DEV, id); lsSet(LS_DEVNAME, name); }

export type OutDevice = { id: string; label: string };
/** The sound outputs on this computer. Windows hides the names until the page has been allowed to use the microphone once, so that is asked for here
 *  (and the microphone is released again straight away: nothing is recorded). Returns the list and, if one looks like a virtual cable, its id. */
export async function listOutputs(): Promise<{ devices: OutDevice[]; cable: string; named: boolean }> {
  try {
    try { const st = await navigator.mediaDevices.getUserMedia({ audio: true }); st.getTracks().forEach((t) => t.stop()); } catch (e) { /* names may stay hidden */ }
    const all = await navigator.mediaDevices.enumerateDevices();
    const devices = all.filter((d) => d.kind === "audiooutput" && d.deviceId !== "default" && d.deviceId !== "communications").map((d) => ({ id: d.deviceId, label: d.label }));
    const named = devices.some((d) => d.label);
    const cable = devices.find((d) => /cable input|vb-audio|virtual/i.test(d.label))?.id ?? "";
    return { devices, cable, named };
  } catch (e) { return { devices: [], cable: "", named: false }; }
}

let pttBusy = false;
/** Press TrackAudio's push-to-talk, run play(), then release it again. The release is always sent, also after an error. */
async function withPtt(play: () => void, totalMs: number): Promise<void> {
  if (pttBusy) return;                                // never two at once
  pttBusy = true;
  let ws: WebSocket | null = null;
  const release = () => { try { ws?.send(JSON.stringify({ type: "kPttReleased" })); } catch (e) { /* link gone */ } try { ws?.close(); } catch (e) { /* closed */ } ws = null; pttBusy = false; };
  try {
    ws = new WebSocket("ws://127.0.0.1:49080/ws");
    await new Promise<void>((ok, bad) => { ws!.onopen = () => ok(); ws!.onerror = () => bad(new Error("no TrackAudio")); setTimeout(() => bad(new Error("timeout")), 2000); });
    ws.send(JSON.stringify({ type: "kPttPressed" }));
    await new Promise((r) => setTimeout(r, 350));       // let the transmission open before the first tone
    play();
    setTimeout(release, totalMs + 350);
  } catch (e) { release(); }
}

function schedule(ctx: AudioContext, pulses: [[number, number], [number, number]], level: number): void {
  const t0 = ctx.currentTime + 0.05;
  pulses.forEach((pair, i) => {
    const start = t0 + i * (PULSE_S + GAP_S);
    pair.forEach((hz) => {
      const o = ctx.createOscillator(), g = ctx.createGain();
      o.type = "sine"; o.frequency.value = hz; o.connect(g); g.connect(ctx.destination);
      g.gain.setValueAtTime(0, start);
      g.gain.linearRampToValueAtTime(level, start + 0.02);
      g.gain.setValueAtTime(level, start + PULSE_S - 0.03);
      g.gain.linearRampToValueAtTime(0, start + PULSE_S);
      o.start(start); o.stop(start + PULSE_S + 0.02);
    });
  });
}

let actx: AudioContext | null = null;
/** Play the chime for a code (on this computer's speakers, if the FDD sound and the SELCAL sound are on) and, if switched on, send it on your frequency.
 *  Returns "" when the chime was played here, or the reason it was not (shown to the controller, so a silent Send is never a mystery). */
export function playSelcal(code: string): string {
  const pulses = selcalPulses(code);
  let why = "";
  if (!pulses) why = "the code " + String(code || "").toUpperCase() + " cannot be played as tones (it needs four different letters or numbers from the 32 SELCAL tones: A-H, J-M, P-Z and 1-9, so no I, N or O)";
  else if (!dataHandler.soundOn) why = "the FDD's sound alerts are switched off (the sound switch at the top of the display)";
  else if (!get(selcalSoundOn)) why = "\"Play the SELCAL tones when I send\" is not ticked";
  else {
    try {
      actx = actx ?? new (window.AudioContext || (window as any).webkitAudioContext)();
      if (actx.state === "suspended") void actx.resume();
      schedule(actx, pulses, 0.12);
    } catch (e) { why = "this window could not play sound"; }
  }
  if (!pulses) return why;
  const dev = get(selcalDevice);
  if (get(selcalFreqOn) && dev) {
    const total = (2 * PULSE_S + GAP_S) * 1000;
    void withPtt(() => {
      try {
        const ctx: AudioContext = new (window.AudioContext || (window as any).webkitAudioContext)({ sinkId: dev } as any);
        const go = () => { schedule(ctx, pulses, 0.5); setTimeout(() => { try { void ctx.close(); } catch (e) { /* closed */ } }, total + 600); };
        if ((ctx as any).setSinkId && (ctx as any).sinkId !== dev) (ctx as any).setSinkId(dev).then(go, go); else go();
      } catch (e) { /* the device is gone: nothing is sent */ }
    }, total);
  }
  return why;
}
