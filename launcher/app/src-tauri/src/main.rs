// The vNAAATS app: a window around https://vnaaats.com/fdd plus a few jobs a web page cannot do
// (find the plugin, check and replace the DLL, save the key into vnaaats.cfg, start EuroScope and VACS).
// Only pages from vnaaats.com may use these jobs.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};
use std::process::Command;
use tauri::{Manager, WebviewWindow};
use tauri_plugin_dialog::DialogExt;

const SITE: &str = "https://vnaaats.com";

#[derive(Serialize, Deserialize, Default, Clone)]
struct Saved {
    root: Option<String>,
    euroscope: Option<String>,
    vacs: Option<String>,
}

fn config_file(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let dir = app.path().app_config_dir().map_err(|e| e.to_string())?;
    std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    Ok(dir.join("settings.json"))
}

fn load(app: &tauri::AppHandle) -> Saved {
    config_file(app).ok().and_then(|f| std::fs::read_to_string(f).ok()).and_then(|t| serde_json::from_str(&t).ok()).unwrap_or_default()
}

fn store(app: &tauri::AppHandle, s: &Saved) -> Result<(), String> {
    std::fs::write(config_file(app)?, serde_json::to_string_pretty(s).map_err(|e| e.to_string())?).map_err(|e| e.to_string())
}

/// Only the vNAAATS website may ask this app to do anything.
fn check_origin(w: &WebviewWindow) -> Result<(), String> {
    let url = w.url().map_err(|e| e.to_string())?;
    if url.scheme() == "https" && url.host_str() == Some("vnaaats.com") {
        Ok(())
    } else {
        Err("not allowed from this page".into())
    }
}

fn http_get(path: &str, max: u64) -> Result<Vec<u8>, String> {
    use std::io::Read;
    let resp = ureq::get(&format!("{}{}", SITE, path)).timeout(std::time::Duration::from_secs(60)).call().map_err(|e| format!("could not reach vnaaats.com: {}", e))?;
    let mut buf = Vec::new();
    resp.into_reader().take(max).read_to_end(&mut buf).map_err(|e| e.to_string())?;
    Ok(buf)
}

#[derive(Serialize)]
struct Status {
    app_version: String,
    root: Option<String>,
    euroscope: Option<String>,
    vacs: Option<String>,
    dll_found: bool,
    dll_path: Option<String>,
    dll_sha: Option<String>,
    latest_version: Option<String>,
    latest_sha: Option<String>,
    up_to_date: Option<bool>,
    cfg_exists: bool,
    cfg_has_key: bool,
    error: Option<String>,
}

#[tauri::command]
fn status(window: WebviewWindow, app: tauri::AppHandle) -> Result<Status, String> {
    check_origin(&window)?;
    let saved = load(&app);
    let mut st = Status {
        app_version: env!("CARGO_PKG_VERSION").to_string(),
        root: saved.root.clone(),
        euroscope: saved.euroscope.clone(),
        vacs: saved.vacs.clone(),
        dll_found: false,
        dll_path: None,
        dll_sha: None,
        latest_version: None,
        latest_sha: None,
        up_to_date: None,
        cfg_exists: false,
        cfg_has_key: false,
        error: None,
    };
    let Some(root) = saved.root else { return Ok(st) };
    if let Some(dll) = vnaaats_core::find_plugin_dll(Path::new(&root), 6) {
        st.dll_found = true;
        st.dll_path = Some(dll.to_string_lossy().into_owned());
        if let Ok(bytes) = std::fs::read(&dll) {
            st.dll_sha = Some(vnaaats_core::sha256_hex(&bytes));
        }
        if let Some(dir) = dll.parent() {
            let c = vnaaats_core::read_cfg(dir);
            st.cfg_exists = c.exists;
            st.cfg_has_key = c.has_key;
        }
    }
    match http_get("/plugin/latest.json", 4096).and_then(|b| serde_json::from_slice::<serde_json::Value>(&b).map_err(|e| e.to_string())) {
        Ok(v) => {
            st.latest_version = v["version"].as_str().map(String::from);
            st.latest_sha = v["sha256"].as_str().map(String::from);
            if let (Some(a), Some(b)) = (&st.dll_sha, &st.latest_sha) {
                st.up_to_date = Some(a.eq_ignore_ascii_case(b));
            }
        }
        Err(e) => st.error = Some(e),
    }
    Ok(st)
}

#[tauri::command]
fn pick_folder(window: WebviewWindow, app: tauri::AppHandle) -> Result<Option<String>, String> {
    check_origin(&window)?;
    let Some(p) = app.dialog().file().set_title("Choose your EXOCO folder (the one holding Plugins)").blocking_pick_folder() else { return Ok(None) };
    let path = p.into_path().map_err(|e| e.to_string())?;
    let mut s = load(&app);
    s.root = Some(path.to_string_lossy().into_owned());
    store(&app, &s)?;
    Ok(s.root)
}

#[tauri::command]
fn update_plugin(window: WebviewWindow, app: tauri::AppHandle) -> Result<String, String> {
    check_origin(&window)?;
    let root = load(&app).root.ok_or("choose your EXOCO folder first")?;
    let dll = vnaaats_core::find_plugin_dll(Path::new(&root), 6).ok_or("VatsimNAAATS.dll was not found in that folder")?;
    let info: serde_json::Value = serde_json::from_slice(&http_get("/plugin/latest.json", 4096)?).map_err(|e| e.to_string())?;
    let sha = info["sha256"].as_str().ok_or("the server did not say which plugin is the latest")?.to_string();
    let bytes = http_get("/plugin/VatsimNAAATS.dll", 20 * 1024 * 1024)?;
    vnaaats_core::replace_dll(&dll, &bytes, &sha)?;
    Ok(info["version"].as_str().unwrap_or("").to_string())
}

#[tauri::command]
fn save_key(window: WebviewWindow, app: tauri::AppHandle, key: String) -> Result<(), String> {
    check_origin(&window)?;
    let root = load(&app).root.ok_or("choose your EXOCO folder first")?;
    let dll = vnaaats_core::find_plugin_dll(Path::new(&root), 6).ok_or("VatsimNAAATS.dll was not found in that folder")?;
    vnaaats_core::set_cfg_key(dll.parent().ok_or("bad folder")?, &key)
}

fn kind_ok(kind: &str) -> Result<(), String> {
    if kind == "euroscope" || kind == "vacs" { Ok(()) } else { Err("unknown program".into()) }
}

#[tauri::command]
fn pick_exe(window: WebviewWindow, app: tauri::AppHandle, kind: String) -> Result<Option<String>, String> {
    check_origin(&window)?;
    kind_ok(&kind)?;
    let title = if kind == "euroscope" { "Choose EuroScope.exe" } else { "Choose the VACS program" };
    let Some(p) = app.dialog().file().set_title(title).add_filter("Program", &["exe"]).blocking_pick_file() else { return Ok(None) };
    let path = p.into_path().map_err(|e| e.to_string())?;
    if !vnaaats_core::is_exe(&path) {
        return Err("that is not a program (.exe)".into());
    }
    let mut s = load(&app);
    let text = path.to_string_lossy().into_owned();
    if kind == "euroscope" { s.euroscope = Some(text.clone()) } else { s.vacs = Some(text.clone()) }
    store(&app, &s)?;
    Ok(Some(text))
}

#[tauri::command]
fn launch(window: WebviewWindow, app: tauri::AppHandle, kind: String) -> Result<(), String> {
    check_origin(&window)?;
    kind_ok(&kind)?;
    let s = load(&app);
    let path = if kind == "euroscope" { s.euroscope } else { s.vacs }.ok_or("choose the program first")?;
    let p = PathBuf::from(&path);
    if !vnaaats_core::is_exe(&p) {
        return Err("that program is no longer there - choose it again".into());
    }
    let mut cmd = Command::new(&p);
    if let Some(dir) = p.parent() {
        cmd.current_dir(dir);
    }
    cmd.spawn().map(|_| ()).map_err(|e| format!("could not start it: {}", e))
}

// The saved sign-in lives in the Windows Credential Manager (where Windows keeps its own saved passwords), never in a plain file.
#[derive(Serialize, Deserialize)]
struct Login {
    name: String,
    password: String,
}

fn cred_entry() -> Result<keyring::Entry, String> {
    keyring::Entry::new("vNAAATS", "flight-data-display").map_err(|e| e.to_string())
}

#[tauri::command]
fn cred_get(window: WebviewWindow) -> Result<Option<Login>, String> {
    check_origin(&window)?;
    match cred_entry()?.get_password() {
        Ok(text) => Ok(serde_json::from_str(&text).ok()),
        Err(keyring::Error::NoEntry) => Ok(None),
        Err(e) => Err(e.to_string()),
    }
}

#[tauri::command]
fn cred_save(window: WebviewWindow, name: String, password: String) -> Result<(), String> {
    check_origin(&window)?;
    if name.trim().is_empty() || password.is_empty() || name.len() > 100 || password.len() > 200 {
        return Err("nothing to save".into());
    }
    let text = serde_json::to_string(&Login { name: name.trim().to_string(), password }).map_err(|e| e.to_string())?;
    cred_entry()?.set_password(&text).map_err(|e| e.to_string())
}

#[tauri::command]
fn cred_forget(window: WebviewWindow) -> Result<(), String> {
    check_origin(&window)?;
    match cred_entry()?.delete_credential() {
        Ok(()) | Err(keyring::Error::NoEntry) => Ok(()),
        Err(e) => Err(e.to_string()),
    }
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .invoke_handler(tauri::generate_handler![status, pick_folder, update_plugin, save_key, pick_exe, launch, cred_get, cred_save, cred_forget])
        .run(tauri::generate_context!())
        .expect("the vNAAATS app could not start");
}
