/*
 *  Follows the aircraft the controller clicks in EuroScope: the plugin tells the server, and this page asks every 2 seconds.
 *  The matching strip is outlined, scrolled into view and becomes the selected aircraft (flight plan panel).
 */
import { writable } from "svelte/store";
import * as dataHandler from "./data";

const apiBase: string = (import.meta as any).env?.VITE_API_BASE ?? "";

export const espicked = writable<string>("");
let lastSeq: number = -1;                 // -1 until the first answer, so an old pick does not scroll the list on page load
let timer: any = null;

async function poll(): Promise<void> {
  try {
    const r = await fetch(apiBase + "/asel", { credentials: "same-origin" });
    if (!r.ok) return;
    const d: { callsign: string; seq: number } = await r.json();
    if (lastSeq < 0) { lastSeq = d.seq; espicked.set(d.callsign || ""); return; }
    if (d.seq === lastSeq) return;
    lastSeq = d.seq;
    espicked.set(d.callsign || "");
    if (d.callsign) {
      dataHandler.setAsel(d.callsign);
      setTimeout(() => {
        const el = document.getElementById("strip-" + d.callsign);
        if (el) el.scrollIntoView({ block: "center", behavior: "smooth" });
      }, 200);
    }
  } catch (e) { /* offline: try again next time */ }
}

export function startEsSelect(): void {
  if (timer) return;
  poll();
  timer = setInterval(poll, 2000);
}

export function stopEsSelect(): void {
  if (timer) clearInterval(timer);
  timer = null;
  lastSeq = -1;
}
