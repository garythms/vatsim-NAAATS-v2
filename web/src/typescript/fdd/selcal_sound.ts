/*
 *  The SELCAL sound: played here when you press Send on a SELCAL, and sent on your frequency by TrackAudio for Oceanic so the pilot hears it too.
 *  A SELCAL is four characters sent as two pairs (AB-CD): both tones of a pair together for about 1 second, a pause of 0.2 seconds, then the second pair.
 *  The 16 original tones are letters A-S (no I, N or O). SELCAL 32 adds 16 more tones: T-Z and the digits 1-9.
 *  Frequencies are the ones in the ICAO SELCAL 32 proposal (ICAO SAM/IG/23 NE/12) and the ASRI SELCAL guide. They are only used here to make a matching sound.
 */
import { writable, get } from "svelte/store";
import * as sounds from "./sounds";
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

/** Who hears the SELCAL tones when you press Send: "me" = only the controller, on this computer; "all" = the controller, and the pilots on your frequency
 *  (TrackAudio for Oceanic sends them); "off" = nobody. "all" is the default. It is changed on the Settings page. */
export type SelcalMode = "off" | "me" | "all";
function loadMode(): SelcalMode {
  try {
    const v = localStorage.getItem("fdd.selcalMode");
    if (v === "off" || v === "me" || v === "all") return v;
    if (localStorage.getItem("fdd.selcalSound") === "off") return "off";      // an older choice: the tones were switched off
  } catch (e) { /* default */ }
  return "all";
}
export const selcalMode = writable<SelcalMode>(loadMode());
export function setSelcalMode(m: SelcalMode): void {
  selcalMode.set(m);
  try { localStorage.setItem("fdd.selcalMode", m); } catch (e) { /* not saved */ }
}

/** What happened the last time the tones were sent on your frequency, for the SELCAL tab ("" = nothing to say). */
export const selcalNote = writable<string>("");
/** True when TrackAudio answered nothing to the tone request: it is the normal TrackAudio, not TrackAudio for Oceanic. The display then offers the install. */
export const selcalNeedsOceanic = writable<boolean>(false);
/** The last SELCAL the controller sent from here (for the Discord card): the code and when (ms). */
export const lastSelcalSent = writable<{ code: string; at: number }>({ code: "", at: 0 });

/** Ask TrackAudio (on this computer, ws://127.0.0.1:49080/ws) to send the SELCAL on the frequency that is set to transmit.
 *  TrackAudio for Oceanic answers every request with kTransmitTonesResult { ok, reason }; no answer means an ordinary TrackAudio. Returns "" when it was sent, or why not. */
async function sendViaTrackAudio(code: string): Promise<string> {
  const c = String(code || "").toUpperCase().replace(/[-\s]/g, "");
  let ws: WebSocket | null = null;
  selcalNeedsOceanic.set(false);
  try {
    ws = new WebSocket("ws://127.0.0.1:49080/ws");
    await new Promise<void>((ok, bad) => { ws!.onopen = () => ok(); ws!.onerror = () => bad(new Error("no TrackAudio")); setTimeout(() => bad(new Error("timeout")), 2000); });
    const freq: number = await new Promise<number>((ok) => {
      const t = setTimeout(() => ok(0), 2000);
      ws!.onmessage = (e) => {
        try {
          const m = JSON.parse(String(e.data));
          if (m?.type !== "kStationStates") return;
          const st = (m.value?.stations ?? []).map((s: any) => s?.value ?? s).find((s: any) => s?.tx === true && Number(s?.frequency) > 0);
          clearTimeout(t); ok(st ? Number(st.frequency) : 0);
        } catch (err) { /* not for us */ }
      };
      ws!.send(JSON.stringify({ type: "kGetStationStates" }));
    });
    if (!freq) return "None of your TrackAudio frequencies is set to transmit, so no SELCAL was sent on your frequency. Turn TX on for the frequency you want.";
    const answer: { ok: boolean; reason: string } | null = await new Promise((done) => {
      const t = setTimeout(() => done(null), 2500);
      ws!.onmessage = (e) => {
        try {
          const m = JSON.parse(String(e.data));
          if (m?.type !== "kTransmitTonesResult") return;
          clearTimeout(t); done({ ok: m.value?.ok === true, reason: String(m.value?.reason ?? "") });
        } catch (err) { /* not for us */ }
      };
      ws!.send(JSON.stringify({ type: "kTransmitTones", value: { frequency: freq, selcal: c } }));
    });
    if (!answer) { selcalNeedsOceanic.set(true); return "Your TrackAudio cannot send SELCAL tones on the frequency. Install TrackAudio for Oceanic (Settings)."; }
    return answer.ok ? "" : (answer.reason || "TrackAudio did not send the SELCAL.");
  } catch (e) {
    return "TrackAudio is not running on this computer, so no SELCAL was sent on your frequency.";
  } finally { try { ws?.close(); } catch (e) { /* closed */ } }
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
/** A made-up SELCAL: four different tones picked at random, for a code that has no tones of its own (a repeated letter, or I, N, O). */
function randomPulses(): [[number, number], [number, number]] {
  const keys = Object.keys(TONES);
  for (let i = keys.length - 1; i > 0; i--) { const j = Math.floor(Math.random() * (i + 1)); [keys[i], keys[j]] = [keys[j], keys[i]]; }
  const f = keys.slice(0, 4).map((k) => TONES[k]);
  return [[f[0], f[1]], [f[2], f[3]]];
}

/** Play the SELCAL tones for a code, as the setting says (off / only the controller / the controller and the pilots). A code that has no tones plays a random one.
 *  test = true plays them here only, whatever the setting is (the "Hear the tones" button). */
export function playSelcal(code: string, test: boolean = false): void {
  const mode = test ? "me" : get(selcalMode);
  if (!test) lastSelcalSent.set({ code: String(code || "").toUpperCase().replace(/[-\s]/g, ""), at: Date.now() });
  if (mode === "off") return;
  const pulses = selcalPulses(code) ?? randomPulses();
  try {
    actx = actx ?? new (window.AudioContext || (window as any).webkitAudioContext)();
    if (actx.state === "suspended") void actx.resume();
    const vol: number = sounds.gain("selcal");
    if (vol > 0) schedule(actx, pulses, 0.12 * vol);
  } catch (e) { /* no audio here */ }
  if (mode === "all" && !test) {
    selcalNote.set("");
    void sendViaTrackAudio(code).then((why) => selcalNote.set(why ? why : "SELCAL sent on your frequency by TrackAudio."));
  }
}
