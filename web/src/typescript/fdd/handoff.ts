/*
 *  Handoff prompter. For aircraft YOU are tracking, the server says how far each is from its next boundary and who is online to take it
 *  (Aircraft.Handoff). At 30 nm a card pops up with a chime and the transfer controls; with nobody online it waits until the aircraft is
 *  at the boundary and then says it is leaving oceanic airspace (UNICOM). Dismissing a card keeps it away for that aircraft.
 */
import { writable, get } from "svelte/store";
import * as dataHandler from "./data";

export const NEAR_NM = 30;       // pop up with the controllers online
export const LEAVE_NM = 3;       // nobody online: pop up at the boundary
const CLEAR_NM = 45;             // an aircraft this far from the boundary again is forgotten (so a later approach can alert again)

export type HandoffCard = { callsign: string; kind: "near" | "leaving"; nm: number; fix: string; name: string; cpdlc: boolean; online: { callsign: string; code: string; frequency: string }[] };
export const handoffCards = writable<HandoffCard[]>([]);
export const handoffSoundOn = writable<boolean>((() => { try { return localStorage.getItem("vnaaats.handoffSound") !== "off"; } catch (e) { return true; } })());
export function setHandoffSound(on: boolean): void {
  handoffSoundOn.set(on);
  try { localStorage.setItem("vnaaats.handoffSound", on ? "on" : "off"); } catch (e) { /* not saved */ }
}

const done = new Set<string>();      // callsign|kind already shown (or dismissed)
let actx: AudioContext | null = null;
/** Two rising notes, different from the VACS ring and the CPDLC sound. */
export function chime(): void {
  if (!dataHandler.soundOn || !get(handoffSoundOn)) return;
  try {
    actx = actx ?? new (window.AudioContext || (window as any).webkitAudioContext)();
    if (actx.state === "suspended") void actx.resume();
    if (actx.state !== "running") return;
    const t0 = actx.currentTime + 0.02;
    [[660, 0], [880, 0.22]].forEach(([f, d]) => {
      const o = actx!.createOscillator(), g = actx!.createGain();
      o.type = "triangle"; o.frequency.value = f; o.connect(g); g.connect(actx!.destination);
      g.gain.setValueAtTime(0, t0 + d); g.gain.linearRampToValueAtTime(0.25, t0 + d + 0.02); g.gain.exponentialRampToValueAtTime(0.001, t0 + d + 0.5);
      o.start(t0 + d); o.stop(t0 + d + 0.52);
    });
  } catch (e) { /* no audio */ }
}

try { window.addEventListener("pointerdown", () => { try { actx = actx ?? new (window.AudioContext || (window as any).webkitAudioContext)(); void actx.resume(); } catch (e) { /* no audio */ } }, { once: true }); } catch (e) { /* no window */ }

export function dismiss(callsign: string): void {
  handoffCards.update((l) => l.filter((c) => c.callsign !== callsign));
}

/** Call every couple of seconds with your position callsign (e.g. CZQO_A_CTR). */
export function update(position: string): void {
  const me = (position ?? "").toUpperCase();
  const cards: HandoffCard[] = [];
  const seen = new Set<string>();
  if (me && me !== "OBSERVER") {
    dataHandler.networkAircraft.forEach((group) => group.forEach((ac: any) => {
      const h = ac.Handoff;
      if (!h || String(ac.TrackedBy ?? "").toUpperCase() !== me) return;
      seen.add(ac.Callsign);
      if (h.nm > CLEAR_NM) { done.delete(ac.Callsign + "|near"); done.delete(ac.Callsign + "|leaving"); return; }
      const kind = !h.unicom && h.nm <= NEAR_NM ? "near" : h.unicom && h.nm <= LEAVE_NM ? "leaving" : null;
      if (!kind) return;
      const key = ac.Callsign + "|" + kind;
      cards.push({ callsign: ac.Callsign, kind, nm: h.nm, fix: h.fix, name: h.name, cpdlc: !!ac.Cpdlc, online: h.online ?? [] });
      if (!done.has(key)) { done.add(key); chime(); }
    }));
  }
  const dismissed = new Set<string>(get(dismissedStore));
  const shown = cards.filter((c) => !dismissed.has(c.callsign + "|" + c.kind));
  handoffCards.set(shown);
  [...dismissed].forEach((k) => { if (!seen.has(k.split("|")[0])) dismissedStore.update((s) => s.filter((x) => x !== k)); });
}
const dismissedStore = writable<string[]>([]);
export function dismissCard(c: HandoffCard): void {
  dismissedStore.update((s) => [...s, c.callsign + "|" + c.kind]);
  dismiss(c.callsign);
}
