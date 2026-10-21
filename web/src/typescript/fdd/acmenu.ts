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
export function contactMeCommand(callsign: string): string { return ".contactme " + callsign; }
export function wallopCommand(callsign: string, text: string): string {
  const t = text.trim().replace(/"/g, "'");
  return ".wallop " + callsign + " " + (t.indexOf(" ") >= 0 ? '"' + t + '"' : t);
}
export function validSelcal(code: string): boolean { return /^[A-HJ-MP-S]{4}$/.test(code.toUpperCase()); }

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
