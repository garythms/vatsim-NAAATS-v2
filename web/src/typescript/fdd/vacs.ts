/*
 *  VACS call alert. VACS (vacs.network) has a remote-control WebSocket on this computer (default ws://localhost:9600/ws, switched off
 *  until the controller turns it on in VACS). We only LISTEN for call events and, if asked, accept a ringing call. Nothing is sent to our server.
 *  The VACS API has no stability promise, so everything here fails quietly: on any problem the indicator simply goes grey.
 */
import { writable, get } from "svelte/store";
import * as app from "./app";

export type VacsState = {
  enabled: boolean;
  link: "off" | "connecting" | "up";        // our WebSocket to VACS
  onPosition: boolean;                       // VACS itself is connected to a position
  ringing: { callId: string; from: string; prio: boolean } | null;
  call: { callId: string; with: string } | null;
  dismissed: string;                         // callId whose pop-up was dismissed
  error: string;
};

const KEY = "vnaaats.vacs";
const PORT = "vnaaats.vacsPort";
function lsGet(k: string): string | null { try { return localStorage.getItem(k); } catch (e) { return null; } }
function lsSet(k: string, v: string): void { try { localStorage.setItem(k, v); } catch (e) { /* ignore */ } }

export function isEnabled(): boolean { const v = lsGet(KEY); return v === null ? app.inApp() : v === "1"; }
export function getPort(): number { const n = parseInt(lsGet(PORT) ?? "", 10); return n > 0 && n < 65536 ? n : 9600; }

export const vacs = writable<VacsState>({ enabled: false, link: "off", onPosition: false, ringing: null, call: null, dismissed: "", error: "" });
const upd = (f: (s: VacsState) => void) => vacs.update((s) => { f(s); return { ...s }; });

let ws: WebSocket | null = null;
let timer: any = null;
let ping: any = null;
let wanted = false;
let delay = 5000;
let nextId = 1;
let me = "";                                 // our own VATSIM CID, to tell "ringing for me" from calls we placed
const calls = new Map<string, string>();     // callId -> label of the other party

const EVENTS = ["signaling:call-invitation", "signaling:call-update", "signaling:call-end", "signaling:force-call-end", "signaling:accept-incoming-call",
  "signaling:outgoing-call", "signaling:connected", "signaling:disconnected", "webrtc:call-connected", "webrtc:call-disconnected", "auth:authenticated"];

function label(src: any): string {
  return (src?.positionId || src?.stationId || src?.clientId || "unknown").toString();
}
function targetLabel(t: any): string {
  if (!t) return "";
  return (t.station || t.position || t.client || "").toString();
}

function handle(name: string, p: any): void {
  switch (name) {
    case "signaling:call-invitation": {
      if (!p?.callId) return;
      const from = label(p.source);
      calls.set(p.callId, from);
      upd((s) => { s.ringing = { callId: p.callId, from, prio: !!p.prio }; });
      break;
    }
    case "signaling:accept-incoming-call":
      upd((s) => { if (s.ringing && s.ringing.callId === p) { s.call = { callId: p, with: s.ringing.from }; s.ringing = null; } });
      break;
    case "signaling:outgoing-call": {
      if (!p?.callId) return;
      const to = (p.targets ?? []).map(targetLabel).filter((x: string) => x).join(", ");
      calls.set(p.callId, to);
      upd((s) => { s.call = { callId: p.callId, with: to }; });
      break;
    }
    case "webrtc:call-connected":
      upd((s) => {
        if (s.ringing && s.ringing.callId === p?.callId) { s.call = { callId: p.callId, with: s.ringing.from }; s.ringing = null; }
        else if (!s.call && p?.callId) s.call = { callId: p.callId, with: calls.get(p.callId) ?? "" };
      });
      break;
    case "signaling:call-end":
    case "signaling:force-call-end":
      calls.delete(p);
      upd((s) => { if (s.ringing?.callId === p) s.ringing = null; if (s.call?.callId === p) s.call = null; });
      break;
    case "webrtc:call-disconnected":
      break;                                  // one leg of a call; the call itself ends with signaling:call-end
    case "signaling:connected": upd((s) => { s.onPosition = true; }); break;
    case "signaling:disconnected": upd((s) => { s.onPosition = false; s.ringing = null; s.call = null; }); break;
    case "auth:authenticated": me = String(p ?? ""); break;
  }
}

function send(o: any): void { try { if (ws && ws.readyState === 1) ws.send(JSON.stringify(o)); } catch (e) { /* ignore */ } }

function open(): void {
  if (!wanted) return;
  upd((s) => { s.link = "connecting"; });
  let sock: WebSocket;
  try { sock = new WebSocket("ws://127.0.0.1:" + getPort() + "/ws"); } catch (e) { upd((s) => { s.link = "off"; s.error = "blocked"; }); again(); return; }
  ws = sock;
  sock.onopen = () => {
    delay = 5000;
    upd((s) => { s.link = "up"; s.error = ""; });
    for (const e of EVENTS) send({ type: "subscribe", event: e });
    send({ type: "invoke", id: "ready", cmd: "app_frontend_ready", args: {} });
    send({ type: "invoke", id: "state", cmd: "remote_get_session_state", args: {} });
    send({ type: "invoke", id: "sync", cmd: "remote_request_store_sync", args: {} });
    clearInterval(ping);
    ping = setInterval(() => send({ type: "ping" }), 20000);
  };
  sock.onmessage = (m) => {
    let d: any;
    try { d = JSON.parse(m.data as string); } catch (e) { return; }
    if (d.type === "event") handle(d.name, d.payload);
    else if (d.type === "response" && d.id === "state" && d.ok) {
      upd((s) => { s.onPosition = d.data?.connectionState === "connected"; });
      if (d.data?.clientId) me = String(d.data.clientId);
    }
  };
  sock.onclose = () => { clearInterval(ping); if (ws === sock) ws = null; upd((s) => { s.link = "off"; s.onPosition = false; s.ringing = null; s.call = null; }); again(); };
  sock.onerror = () => { /* onclose follows */ };
}

function again(): void {
  if (!wanted) return;
  clearTimeout(timer);
  timer = setTimeout(open, delay);
  delay = Math.min(delay * 2, 30000);
}

export function start(): void {
  wanted = isEnabled();
  upd((s) => { s.enabled = wanted; });
  if (!wanted || ws) return;
  open();
}
export function stop(): void {
  wanted = false;
  clearTimeout(timer); clearInterval(ping);
  try { ws?.close(); } catch (e) { /* ignore */ }
  ws = null;
  upd((s) => { s.link = "off"; s.onPosition = false; s.ringing = null; s.call = null; });
}
export function configure(enabled: boolean, port: number): void {
  lsSet(KEY, enabled ? "1" : "0");
  lsSet(PORT, String(port));
  stop();
  delay = 5000;
  start();
}
export function accept(): void {
  const r = get(vacs).ringing;
  if (r) send({ type: "invoke", id: "n" + nextId++, cmd: "signaling_accept_call", args: { callId: r.callId } });
}
export function dismiss(): void { upd((s) => { s.dismissed = s.ringing?.callId ?? ""; }); }
