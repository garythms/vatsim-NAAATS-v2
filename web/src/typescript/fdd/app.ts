/*
 *  The vNAAATS app (a small Windows program that opens this display) can do things a web page cannot:
 *  find the plugin, replace an old DLL, save your key into vnaaats.cfg, start EuroScope and VACS.
 *  In a normal browser none of this exists and the display works as before.
 */
export type AppStatus = {
  app_version: string; root: string | null; euroscope: string | null; vacs: string | null;
  dll_found: boolean; dll_path: string | null; dll_sha: string | null;
  latest_version: string | null; latest_sha: string | null; up_to_date: boolean | null;
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
export const pickExe = (kind: "euroscope" | "vacs"): Promise<string | null> => call("pick_exe", { kind });
export const launch = (kind: "euroscope" | "vacs"): Promise<void> => call("launch", { kind });

export function errText(e: unknown): string { return typeof e === "string" ? e : (e as any)?.message ?? "something went wrong"; }
