/*
 *  Aircraft menu (click a callsign): commands for EuroScope and copy-and-paste EuroScope dot commands.
 */
import { writable } from "svelte/store";

const apiBase: string = (import.meta as any).env?.VITE_API_BASE ?? "";

export const menuCallsign = writable<string>("");   // "" = menu closed

export type Controller = { callsign: string; frequency: string; name: string; facility: number };

export async function loadControllers(): Promise<Controller[]> {
  try {
    const r = await fetch(apiBase + "/GetControllers", { credentials: "same-origin" });
    return r.ok ? await r.json() : [];
  } catch (e) { return []; }
}

// Copy text to the clipboard. Returns true if it worked.
export async function copyText(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch (e) {
    try {
      const ta = document.createElement("textarea");
      ta.value = text;
      ta.style.position = "fixed";
      ta.style.opacity = "0";
      document.body.appendChild(ta);
      ta.select();
      const ok = document.execCommand("copy");
      document.body.removeChild(ta);
      return ok;
    } catch (e2) { return false; }
  }
}

export const NO_CONTACT_TEXT = "This aircraft is within my controlling area but has not responded to contact me requests";

export function selcalCommand(code: string): string { return ".selcal " + code.toUpperCase().replace(/[^A-Z]/g, ""); }
// EuroScope's .contactme takes no callsign: it acts on the aircraft you click or have selected
export function contactMeCommand(_callsign: string): string { return ".contactme"; }
export function wallopCommand(callsign: string, text: string): string {
  const t = text.trim().replace(/"/g, "'");
  return ".wallop " + callsign + " " + (t.indexOf(" ") >= 0 ? '"' + t + '"' : t);
}
export function validSelcal(code: string): boolean { return /^[A-Z]{4}$/.test(code.toUpperCase().replace(/[-\s]/g, "")); }   // any four letters: VATSIM pilots are not limited to the real-world SELCAL letters
// Why a SELCAL code is not valid ("" if it is). Any four letters are fine: VATSIM pilots are not limited to the real-world SELCAL letters.
export function selcalProblem(code: string): string {
  const c = code.toUpperCase().replace(/[-\s]/g, "");
  return /^[A-Z]{4}$/.test(c) ? "" : "SELCAL must be exactly 4 letters, like ABCD";
}

// Send a command to the controller's EuroScope. Returns an error message, or the command id as a number.
export async function queueCommand(body: Record<string, string | number>): Promise<string | number> {
  try {
    const r = await fetch(apiBase + "/cmd/queue", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body), credentials: "same-origin" });
    const d = await r.json().catch(() => ({}));
    return r.ok ? d.id : (d.error ?? "could not send the command");
  } catch (e) { return "cannot reach the server"; }
}

// Wait for EuroScope's answer. state: ok | failed | waiting | pending | delivered
export async function commandState(id: number): Promise<{ state: string; msg: string }> {
  try {
    const r = await fetch(apiBase + "/cmd/status?id=" + id, { credentials: "same-origin" });
    if (!r.ok) return { state: "failed", msg: "lost track of the command" };
    return await r.json();
  } catch (e) { return { state: "pending", msg: "" }; }
}

export function natTrakCommand(callsign: string): string {
  return ".msg " + callsign + " Hello, you will soon be entering oceanic airspace. Please request oceanic clearance via nattrak.vatsim.net no later than 20 minutes before your oceanic entry point. Clearance via CPDLC is unavailable.";
}

// Remember (sent = true) or forget (false) that the natTrak reminder was sent. Returns an error message or "".
export async function markNatTrakSent(callsign: string, sent: boolean): Promise<string> {
  try {
    const r = await fetch(apiBase + "/NatTrakSent", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ callsign, sent }), credentials: "same-origin" });
    const d = await r.json().catch(() => ({}));
    return r.ok ? "" : (d.error ?? "could not save");
  } catch (e) { return "cannot reach the server"; }
}
