/*
 *  The vNAAATS app (a small Windows program that opens this display) can do things a web page cannot:
 *  find the plugin, replace an old DLL, save your key into vnaaats.cfg, start EuroScope and VACS.
 *  In a normal browser none of this exists and the display works as before.
 */
export type AppStatus = {
  app_version: string; root: string | null; euroscope: string | null; vacs: string | null; afv: string | null; trackaudio: string | null;
  dll_found: boolean; dll_path: string | null; dll_sha: string | null;
  latest_version: string | null; latest_sha: string | null; up_to_date: boolean | null;
  airlines_found: boolean; airlines_up_to_date: boolean | null;
  cfg_exists: boolean; cfg_has_key: boolean; error: string | null;
};

function tauri(): any { try { return (window as any).__TAURI__; } catch (e) { return null; } }
export function inApp(): boolean { return !!tauri()?.core?.invoke; }

async function call<T>(cmd: string, args: Record<string, unknown> = {}): Promise<T> {
  return await tauri().core.invoke(cmd, args) as T;
}

export const status = (): Promise<AppStatus> => call<AppStatus>("status");
export const pickFolder = (): Promise<string | null> => call("pick_folder");
export const updatePlugin = (): Promise<string> => call("update_plugin");
export const saveKey = (key: string): Promise<void> => call("save_key", { key });
export type ProgKind = "euroscope" | "vacs" | "afv" | "trackaudio";
export const pickExe = (kind: ProgKind): Promise<string | null> => call("pick_exe", { kind });
export const launch = (kind: ProgKind): Promise<void> => call("launch", { kind });

// Open a web page in the normal browser (pop-up windows do not work inside the app). Falls back to window.open outside the app.
// With strict set, a failure in the app is thrown so the caller can show it; otherwise it falls back to a normal window.
export async function openExternal(url: string, strict: boolean = false): Promise<void> {
  if (inApp()) {
    try { await call("open_url", { url }); return; } catch (e) { if (strict) throw e; }
  }
  window.open(url, "_blank");
}

// Settings for opening the programs automatically once a position is chosen (kept in this app's own storage)
export type AutoKind = ProgKind;
// Which audio client the controller uses: "afv" (Audio for VATSIM), "trackaudio" or "none"
export type AudioChoice = "afv" | "trackaudio" | "none";
export function audioGet(): AudioChoice { try { const v = localStorage.getItem("vnaaats.audio"); return v === "afv" || v === "trackaudio" ? v : "none"; } catch (e) { return "none"; } }
export function audioSet(c: AudioChoice): void { try { localStorage.setItem("vnaaats.audio", c); } catch (e) { /* not kept */ } }
export function autoGet(kind: AutoKind): boolean { try { return localStorage.getItem("vnaaats.auto." + kind) !== "0"; } catch (e) { return true; } }
export function autoSet(kind: AutoKind, on: boolean): void { try { localStorage.setItem("vnaaats.auto." + kind, on ? "1" : "0"); } catch (e) { /* not kept */ } }

/** Called after a position is chosen: starts EuroScope, VACS and the chosen audio client once per app session, if ticked and located. */
export async function autoLaunch(): Promise<void> {
  if (!inApp()) return;
  let st: AppStatus;
  try { st = await status(); } catch (e) { return; }
  const audio = audioGet();
  for (const k of ["euroscope", "vacs", ...(audio === "none" ? [] : [audio])] as AutoKind[]) {
    if (!st[k] || !autoGet(k)) continue;
    const flag = "vnaaats.launched." + k;
    try { if (sessionStorage.getItem(flag) === "1") continue; sessionStorage.setItem(flag, "1"); } catch (e) { /* carry on */ }
    try { await launch(k); } catch (e) { /* the setup screen shows launch errors; here we stay quiet */ }
  }
}

// The welcome animation tells the setup screen when it has finished
let welcomeResolve: () => void = () => {};
export const welcomeDone: Promise<void> = new Promise<void>((r) => { welcomeResolve = r; });
export function finishWelcome(): void { welcomeResolve(); }

// The saved sign-in (kept in the Windows Credential Manager)
export const credGet = (): Promise<{ name: string; password: string } | null> => call("cred_get");
export const credSave = (name: string, password: string): Promise<void> => call("cred_save", { name, password });
export const credForget = (): Promise<void> => call("cred_forget");

export function errText(e: unknown): string { return typeof e === "string" ? e : (e as any)?.message ?? "something went wrong"; }
