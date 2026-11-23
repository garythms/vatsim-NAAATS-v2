/*
 *  Discord Rich Presence for FDD for Windows: the "Playing vNAAATS" card on the controller's own Discord profile.
 *  First line: position and frequency. Second line, most recent first: Sending SELCAL, TX on the frequency, RX from a callsign,
 *  otherwise aircraft tracked (n of total). Two buttons: Live Map and vNAAATS (set in the app).
 *  Only in FDD for Windows, only while signed in with a position, on by default and switched off in Settings.
 *  The card is built by the Discord app on this computer; nothing is sent to any other server.
 */
import { get, writable } from "svelte/store";
import * as app from "./app";
import * as sess from "./session";
import * as dataHandler from "./data";
import * as cp from "./cpdlc";
import { ta } from "./trackaudio";
import { lastSelcalSent } from "./selcal_sound";
import { loadControllers } from "./acmenu";

const KEY = "fdd.discordPresence";
function load(): boolean { try { return localStorage.getItem(KEY) !== "off"; } catch (e) { return true; } }
export const presenceOn = writable<boolean>(load());
export function setPresenceOn(on: boolean): void {
  presenceOn.set(on);
  try { localStorage.setItem(KEY, on ? "on" : "off"); } catch (e) { /* not saved */ }
  if (!on) { lastSent = ""; startedFor = ""; void app.presenceClear().catch(() => { /* nothing to clear */ }); }
  else schedule();
}

const MIN_GAP = 4500;            // Discord allows about 5 updates in 20 seconds
const HOLD_SELCAL = 8000;        // how long "Sending SELCAL" stays up
const HOLD_RX = 4000;            // an RX line stays up this long after the call ends, so a short call can still be read
const HOLD_TX = 3000;
let startedAt: number = 0;       // unix seconds: when this position was taken, for the elapsed timer
let startedFor: string = "";
let freqFor: string = "";
let freq: string = "";
let lastSent: string = "";
let lastAt: number = 0;
let pending: ReturnType<typeof setTimeout> | null = null;
let rxHold: { who: string; until: number } | null = null;
let txHold: number = 0;
let refresh: ReturnType<typeof setInterval> | null = null;

function mine(pos: string): { mine: number; cpdlc: number } {
  let m = 0, c = 0;
  dataHandler.networkAircraft.forEach((group) => {
    group.forEach((ac) => { if (String(ac.TrackedBy ?? "").toUpperCase() === pos.toUpperCase()) { m++; if (cp.isConnected(ac.Callsign)) c++; } });
  });
  return { mine: m, cpdlc: c };
}

/** The second line: what is happening on the radio right now, else the aircraft count. */
function secondLine(pos: string): string {
  const now = Date.now();
  const sc = get(lastSelcalSent);
  if (sc.code && now - sc.at < HOLD_SELCAL) return "Sending SELCAL " + sc.code;
  const t = get(ta);
  if (t.tx) txHold = now + HOLD_TX;
  if (t.tx || now < txHold) return "TX on " + (freq ? freq.replace(/MHz$/, "") : "frequency");
  if (t.rx) rxHold = { who: t.rx.who, until: now + HOLD_RX };
  if (t.rx || (rxHold && now < rxHold.until)) return "RX from " + (t.rx?.who ?? rxHold?.who ?? "");
  const n = mine(pos);
  return "Aircraft tracked (" + n.mine + " of " + dataHandler.relevantAircraftCount + ")" + (n.cpdlc ? " · " + n.cpdlc + " on CPDLC" : "");
}

async function update(): Promise<void> {
  pending = null;
  if (!app.inApp()) return;
  const s = get(sess.session);
  const pos: string = s.position ?? "";
  if (!get(presenceOn) || !s.loggedIn || !pos || pos === "ADMIN" || pos === "OBSERVER") {
    if (startedFor) { startedFor = ""; lastSent = ""; void app.presenceClear().catch(() => { /* ignore */ }); }
    return;
  }
  if (startedFor !== pos) { startedFor = pos; startedAt = Math.floor(Date.now() / 1000); }
  if (freqFor !== pos) {
    freqFor = pos; freq = "";
    try {
      const c = (await loadControllers()).find((x) => String(x.callsign).toUpperCase() === pos.toUpperCase());
      const f = c ? Number(c.frequency) : 0;
      if (f > 100 && f < 137) freq = f.toFixed(3) + "MHz";
    } catch (e) { /* no frequency */ }
  }
  const details = pos + (freq ? " " + freq : "");
  const state = secondLine(pos);
  const key = details + "|" + state;
  if (key === lastSent) return;
  lastSent = key; lastAt = Date.now();
  try { await app.presenceSet(details, state, startedAt); } catch (e) { /* the app is older than this feature */ }
  // a held line (RX, TX, SELCAL) must be replaced when its time is up
  setTimeout(schedule, HOLD_SELCAL + 200);
}

/** Ask for an update, no sooner than MIN_GAP after the last one. */
function schedule(): void {
  if (!app.inApp() || pending) return;
  const wait = Math.max(0, MIN_GAP - (Date.now() - lastAt));
  pending = setTimeout(() => { void update(); }, wait);
}

/** Start the card: it follows the radio and your tracked aircraft, and refreshes every 15 seconds. Safe to call more than once. */
export function startPresence(): void {
  if (!app.inApp() || refresh) return;
  refresh = setInterval(schedule, 15000);
  ta.subscribe(() => schedule());
  lastSelcalSent.subscribe(() => schedule());
  schedule();
}
