/*
 *  Who is using the FDD, which position they are working, and sending edits to the server.
 */
import { writable } from "svelte/store";
import * as dataHandler from "./data";

const apiBase: string = (import.meta as any).env?.VITE_API_BASE ?? "";

export type Session = {
  editing: boolean;   // the server has some way to log in
  password: boolean;
  vatsim: boolean;
  loggedIn: boolean;
  name: string;
  cid: string;
  position: string;
  positions: string[];
};

export const emptySession: Session = { editing: false, password: false, vatsim: false, loggedIn: false, name: "", cid: "", position: "", positions: [] };
export const session = writable<Session>(emptySession);

export function canEdit(s: Session): boolean {
  return s.loggedIn && s.position !== "" && s.position !== "OBSERVER";
}

async function post(path: string, body: any): Promise<{ ok: boolean; data: any }> {
  try {
    const r = await fetch(apiBase + path, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body), credentials: "same-origin" });
    return { ok: r.ok, data: await r.json().catch(() => ({})) };
  } catch (e) {
    return { ok: false, data: { error: "cannot reach the server" } };
  }
}

export async function refreshSession(): Promise<void> {
  try {
    const r = await fetch(apiBase + "/auth/status", { credentials: "same-origin" });
    if (r.ok) session.set(await r.json());
  } catch (e) { /* offline: keep what we have */ }
}

// Returns an error message, or "" on success
export async function loginPassword(name: string, password: string): Promise<string> {
  const r = await post("/auth/password", { name, password });
  if (r.ok) { session.set(r.data); return ""; }
  return r.data.error ?? "login failed";
}

export async function choosePosition(position: string): Promise<string> {
  const r = await post("/auth/position", { position });
  if (r.ok) { session.set(r.data); return ""; }
  return r.data.error ?? "could not set the position";
}

export async function logout(): Promise<void> {
  await post("/auth/logout", {});
  await refreshSession();
}

export function vatsimLogin(): void {
  window.location.href = apiBase + "/auth/login";
}

// changes: any of level, mach, track, selcal. Returns an error message, or "" on success.
export async function editFlight(callsign: string, changes: Record<string, string | number>): Promise<string> {
  const r = await post("/EditFlight", Object.assign({ callsign }, changes));
  if (!r.ok) {
    if (r.data.error && /log in/.test(r.data.error)) await refreshSession();
    return r.data.error ?? "edit failed";
  }
  await dataHandler.populateAllAircraft();        // show it straight away
  dataHandler.reselect(callsign);
  return "";
}
