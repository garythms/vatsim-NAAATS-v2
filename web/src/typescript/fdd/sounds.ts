/*
 *  Volume for each of the FDD's alert sounds, and one master volume over all of them. Kept in this browser only (localStorage).
 *  0 mutes a sound. 100 is the level the sounds always had, so nothing changes until someone moves a slider.
 */
import { writable, get } from "svelte/store";

export type SoundKind = "natrak" | "cpdlc" | "rcl" | "handoff" | "selcal";
export const soundKinds: { kind: SoundKind; label: string; note: string }[] = [
  { kind: "natrak", label: "natTrak request", note: "A new oceanic clearance request arrives (the Tone setting picks the sound)" },
  { kind: "cpdlc", label: "CPDLC message", note: "A message from a pilot" },
  { kind: "rcl", label: "Clearance request", note: "A new RCL request on CPDLC" },
  { kind: "handoff", label: "Handoff prompt", note: "An aircraft you are tracking is near your boundary" },
  { kind: "selcal", label: "SELCAL", note: "The SELCAL tones you hear on this computer" },
];

const KEY = "vnaaats.soundVolumes";
type Saved = { master: number; levels: Record<SoundKind, number> };

function clamp(n: any): number { n = Number(n); return Number.isFinite(n) ? Math.min(100, Math.max(0, Math.round(n))) : 100; }
function load(): Saved {
  const d: Saved = { master: 100, levels: { natrak: 100, cpdlc: 100, rcl: 100, handoff: 100, selcal: 100 } };
  try {
    const s = JSON.parse(localStorage.getItem(KEY) ?? "null");
    if (s && typeof s === "object") {
      d.master = clamp(s.master ?? 100);
      for (const k of Object.keys(d.levels) as SoundKind[]) d.levels[k] = clamp(s.levels?.[k] ?? 100);
    }
  } catch (e) { /* defaults */ }
  return d;
}

export const soundVolumes = writable<Saved>(load());
soundVolumes.subscribe((v) => { try { localStorage.setItem(KEY, JSON.stringify(v)); } catch (e) { /* not saved */ } });

export function setMaster(n: number): void { soundVolumes.update((v) => ({ ...v, master: clamp(n) })); }
export function setLevel(kind: SoundKind, n: number): void { soundVolumes.update((v) => ({ ...v, levels: { ...v.levels, [kind]: clamp(n) } })); }
export function resetVolumes(): void { soundVolumes.set({ master: 100, levels: { natrak: 100, cpdlc: 100, rcl: 100, handoff: 100, selcal: 100 } }); }

/** 0 to 1: what to multiply a sound's own level by. 0 means this sound is muted. */
export function gain(kind: SoundKind): number {
  const v = get(soundVolumes);
  return (v.master / 100) * (v.levels[kind] / 100);
}
