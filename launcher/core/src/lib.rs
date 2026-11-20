//! The file work of the vNAAATS app. Kept apart from the window code so it can be tested on any computer.
use sha2::{Digest, Sha256};
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};

pub const DLL_NAME: &str = "VatsimNAAATS.dll";
pub const CFG_NAME: &str = "vnaaats.cfg";
const PLACEHOLDER: &str = "PASTE_YOUR_API_KEY_HERE";
const DEFAULT_SERVER: &str = "https://vnaaats.com";

pub fn sha256_hex(bytes: &[u8]) -> String {
    let mut h = Sha256::new();
    h.update(bytes);
    h.finalize().iter().map(|b| format!("{:02x}", b)).collect()
}

/// Find VatsimNAAATS.dll under `root` (looking up to `max_depth` folders down). Returns the first match, shallowest first.
pub fn find_plugin_dll(root: &Path, max_depth: usize) -> Option<PathBuf> {
    let mut level = vec![root.to_path_buf()];
    for _ in 0..=max_depth {
        let mut next = Vec::new();
        for dir in &level {
            let Ok(rd) = fs::read_dir(dir) else { continue };
            let mut subs = Vec::new();
            for e in rd.flatten() {
                let p = e.path();
                if p.is_dir() {
                    subs.push(p);
                } else if p.file_name().map(|n| n.to_string_lossy().eq_ignore_ascii_case(DLL_NAME)).unwrap_or(false) {
                    return Some(p);
                }
            }
            next.extend(subs);
        }
        if next.is_empty() {
            break;
        }
        level = next;
    }
    None
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct CfgInfo {
    pub exists: bool,
    pub has_key: bool,
    pub server: String,
    pub contact_key: Option<u32>,   // contactkey= (Windows key code of the Contact Me key); None until the controller has answered
}

/// The key saved in vnaaats.cfg (None when there is no file, no key line, or it still holds the placeholder).
pub fn read_key(dir: &Path) -> Option<String> {
    let text = fs::read_to_string(dir.join(CFG_NAME)).ok()?;
    for line in text.lines() {
        let line = line.trim();
        if line.starts_with('#') {
            continue;
        }
        if let Some((k, v)) = line.split_once('=') {
            if k.trim() == "key" {
                let v = clean_key(v);
                return if v.is_empty() || v == PLACEHOLDER { None } else { Some(v) };
            }
        }
    }
    None
}

pub fn read_cfg(dir: &Path) -> CfgInfo {
    let mut info = CfgInfo { exists: false, has_key: false, server: DEFAULT_SERVER.to_string(), contact_key: None };
    let Ok(text) = fs::read_to_string(dir.join(CFG_NAME)) else { return info };
    info.exists = true;
    for line in text.lines() {
        let line = line.trim();
        if line.starts_with('#') {
            continue;
        }
        if let Some((k, v)) = line.split_once('=') {
            let (k, v) = (k.trim(), v.trim());
            if k == "key" && !v.is_empty() && v != PLACEHOLDER {
                info.has_key = true;
            } else if k == "server" && !v.is_empty() {
                info.server = v.to_string();
            } else if k == "contactkey" {
                info.contact_key = v.parse::<u32>().ok().filter(|n| (1..=254).contains(n));
            }
        }
    }
    info
}

/// A key looks like 1234567.abc... (a personal key) or a long code (the shared key): letters, numbers, dot, dash, underscore.
pub fn valid_key(key: &str) -> bool {
    (8..=120).contains(&key.len()) && key != PLACEHOLDER && key.chars().all(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '-' | '_'))
}

/// A key as people paste it: with spaces, quotes, or the "key=" label that the vnaaats.cfg line shows. Leaves just the key.
pub fn clean_key(raw: &str) -> String {
    let mut s = raw.trim().trim_matches(|c| c == '"' || c == '\'' || c == '`').trim();
    if s.len() >= 4 && s[..4].eq_ignore_ascii_case("key=") {
        s = s[4..].trim().trim_matches(|c| c == '"' || c == '\'' || c == '`').trim();
    }
    s.to_string()
}

/// Put `key=` into vnaaats.cfg (replacing an existing key line, or adding one; creating the file if it is not there).
/// Every other line is kept as it was, including the line endings.
pub fn set_cfg_key(dir: &Path, key: &str) -> Result<(), String> {
    let key = clean_key(key);
    let key = key.as_str();
    if !valid_key(key) {
        return Err("that does not look like a vNAAATS key (it is long, with letters, numbers and a dot)".into());
    }
    let path = dir.join(CFG_NAME);
    let old = fs::read_to_string(&path).unwrap_or_default();
    let eol = if old.contains("\r\n") || old.is_empty() { "\r\n" } else { "\n" };
    let mut out: Vec<String> = Vec::new();
    let mut done = false;
    for line in old.lines() {
        let t = line.trim_start();
        if !t.starts_with('#') && t.split_once('=').map(|(k, _)| k.trim() == "key").unwrap_or(false) {
            if !done {
                out.push(format!("key={}", key));
                done = true;
            }
        } else {
            out.push(line.to_string());
        }
    }
    if old.is_empty() {
        out.push(format!("server={}", DEFAULT_SERVER));
    }
    if !done {
        out.push(format!("key={}", key));
    }
    let text = out.join(eol) + eol;
    let tmp = dir.join("vnaaats.cfg.tmp");
    fs::write(&tmp, text).map_err(|e| format!("could not write vnaaats.cfg: {}", e))?;
    fs::rename(&tmp, &path).map_err(|e| format!("could not save vnaaats.cfg: {}", e))
}

/// Put `contactkey=<code>` into vnaaats.cfg: the Windows key code of the key EuroScope uses for Contact Me (36 = Home).
/// Replaces an existing line or adds one; every other line is kept as it was.
pub fn set_cfg_contact_key(dir: &Path, code: u32) -> Result<(), String> {
    if !(1..=254).contains(&code) {
        return Err("that is not a key code the plugin can press".into());
    }
    let path = dir.join(CFG_NAME);
    let old = fs::read_to_string(&path).unwrap_or_default();
    let eol = if old.contains("\r\n") || old.is_empty() { "\r\n" } else { "\n" };
    let mut out: Vec<String> = Vec::new();
    let mut done = false;
    for line in old.lines() {
        let t = line.trim_start();
        if !t.starts_with('#') && t.split_once('=').map(|(k, _)| k.trim() == "contactkey").unwrap_or(false) {
            if !done {
                out.push(format!("contactkey={}", code));
                done = true;
            }
        } else {
            out.push(line.to_string());
        }
    }
    if !done {
        out.push(format!("contactkey={}", code));
    }
    let text = out.join(eol) + eol;
    let tmp = dir.join("vnaaats.cfg.tmp");
    fs::write(&tmp, text).map_err(|e| format!("could not write vnaaats.cfg: {}", e))?;
    fs::rename(&tmp, &path).map_err(|e| format!("could not save vnaaats.cfg: {}", e))
}

/// Replace the plugin DLL with `bytes` after checking its SHA-256. Fails with a plain message if EuroScope still has it open.
pub fn replace_dll(dll: &Path, bytes: &[u8], expected_sha: &str) -> Result<(), String> {
    if bytes.len() < 2 || &bytes[..2] != b"MZ" {
        return Err("the download is not a DLL".into());
    }
    if !expected_sha.eq_ignore_ascii_case(&sha256_hex(bytes)) {
        return Err("the download does not match its checksum - try again".into());
    }
    let tmp = dll.with_extension("dll.new");
    fs::write(&tmp, bytes).map_err(|e| format!("could not write the new plugin: {}", e))?;
    if let Err(e) = fs::rename(&tmp, dll) {
        let _ = fs::remove_file(&tmp);
        return Err(format!("could not replace the plugin - close EuroScope and try again ({})", e));
    }
    Ok(())
}

/// Replace a plain data file (such as ICAO_Airlines.txt) after checking its SHA-256.
pub fn replace_file(dest: &Path, bytes: &[u8], expected_sha: &str) -> Result<(), String> {
    if bytes.is_empty() {
        return Err("the download is empty".into());
    }
    if !expected_sha.eq_ignore_ascii_case(&sha256_hex(bytes)) {
        return Err("the download does not match its checksum - try again".into());
    }
    let tmp = dest.with_extension("txt.new");
    fs::write(&tmp, bytes).map_err(|e| format!("could not write {}: {}", dest.display(), e))?;
    if let Err(e) = fs::rename(&tmp, dest) {
        let _ = fs::remove_file(&tmp);
        return Err(format!("could not replace {}: {}", dest.display(), e));
    }
    Ok(())
}

/// Files that hold a controller's own choices (EuroScope profile with login details, screen layout, voice setup).
/// A sector pack installs them only when the controller does not have them yet; they are never overwritten.
fn is_personal(rel: &Path) -> bool {
    let name = rel.file_name().map(|n| n.to_string_lossy().to_ascii_lowercase()).unwrap_or_default();
    name.ends_with(".prf") || ["screen layout.txt", "vccs.txt", "voicechannels.txt"].contains(&name.as_str())
}

/// Point an existing EuroScope profile at the sector file named in the pack's own profile, leaving every other line as it was.
fn patch_prf_sector(existing: &Path, pack_prf: &str) -> Result<(), String> {
    let want = pack_prf.lines().find(|l| l.starts_with("Settings\tsector\t")).map(|l| l.trim_end().to_string());
    let Some(want) = want else { return Ok(()) };
    let old = fs::read(existing).map_err(|e| e.to_string())?;
    let text = String::from_utf8_lossy(&old).into_owned();
    let eol = if text.contains("\r\n") { "\r\n" } else { "\n" };
    let mut found = false;
    let lines: Vec<String> = text.lines().map(|l| if l.starts_with("Settings\tsector\t") { found = true; want.clone() } else { l.to_string() }).collect();
    if !found {
        return Ok(());
    }
    if lines.join(eol) + eol == text {
        return Ok(());
    }
    fs::write(existing, lines.join(eol) + eol).map_err(|e| format!("could not update your EuroScope profile: {}", e))
}

/// Gander Radio is trialled as CZQO_FSS until 1 January 2027. An existing VoiceChannels.txt is the controller's own file and is
/// not replaced, so rename the Gander positions in it (CZQO_CTR, CZQO_A_CTR ... CZQO_H_CTR) and leave every other line as it was.
fn migrate_voice_channels(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    let mut rest = text;
    while let Some(i) = rest.find("CZQO") {
        out.push_str(&rest[..i]);
        let after = &rest[i + 4..];
        let mid = match after.as_bytes() {
            [b'_', l, b'_', ..] if (b'A'..=b'H').contains(l) => 2,
            _ => 0,
        };
        if after[mid..].starts_with("_CTR") {
            out.push_str(&rest[i..i + 4 + mid]);
            out.push_str("_FSS");
            rest = &after[mid + 4..];
        } else {
            out.push_str("CZQO");
            rest = after;
        }
    }
    out.push_str(rest);
    out
}

fn patch_voice_channels(existing: &Path) -> Result<(), String> {
    let old = fs::read(existing).map_err(|e| e.to_string())?;
    let text = String::from_utf8_lossy(&old).into_owned();
    let new = migrate_voice_channels(&text);
    if new == text {
        return Ok(());
    }
    fs::write(existing, new).map_err(|e| format!("could not update your voice channel list: {}", e))
}

/// What the controller types into EuroScope's Connect dialog, kept in their EuroScope profile (.prf) under "LastSession".
#[derive(Debug, PartialEq, Eq, Clone, Default)]
pub struct ProfileDetails {
    pub realname: String,
    pub certificate: String,
    pub rating: Option<u32>,        // EuroScope's own number (Observer 0 ... Enroute Controller 4 ...)
    pub has_password: bool,         // the password itself is never read back
}

/// EuroScope's numbers for the ratings a controller can pick (Student 3 = "Senior Student" is 3, Enroute Controller is 4, and so on).
pub const RATINGS: &[u32] = &[0, 1, 2, 3, 4, 6, 7, 9, 10, 11];

/// The EuroScope profiles that load the vNAAATS plugin. They sit next to the EXCXO folder: in `root`, or in its parent when `root` is the EXCXO folder itself.
pub fn find_vnaaats_profiles(root: &Path) -> Vec<PathBuf> {
    let mut dirs = vec![root.to_path_buf()];
    if root.file_name().map(|n| n.to_string_lossy().eq_ignore_ascii_case("EXCXO")).unwrap_or(false) {
        if let Some(parent) = root.parent() {
            dirs.insert(0, parent.to_path_buf());
        }
    }
    let mut found = Vec::new();
    for d in dirs {
        let Ok(rd) = fs::read_dir(&d) else { continue };
        for e in rd.flatten() {
            let p = e.path();
            if p.extension().map(|x| x.to_string_lossy().eq_ignore_ascii_case("prf")).unwrap_or(false) {
                if let Ok(b) = fs::read(&p) {
                    if String::from_utf8_lossy(&b).to_ascii_lowercase().contains(&DLL_NAME.to_ascii_lowercase()) {
                        found.push(p);
                    }
                }
            }
        }
    }
    found.sort();
    found.dedup();
    found
}

fn session_value<'a>(text: &'a str, key: &str) -> Option<&'a str> {
    let head = format!("LastSession\t{}\t", key);
    text.lines().find_map(|l| l.strip_prefix(head.as_str()))
}

pub fn read_profile_details(prf: &Path) -> Result<ProfileDetails, String> {
    let bytes = fs::read(prf).map_err(|e| e.to_string())?;
    let text = String::from_utf8_lossy(&bytes);
    Ok(ProfileDetails {
        realname: session_value(&text, "realname").unwrap_or("").trim().to_string(),
        certificate: session_value(&text, "certificate").unwrap_or("").trim().to_string(),
        rating: session_value(&text, "rating").and_then(|v| v.trim().parse().ok()),
        has_password: !session_value(&text, "password").unwrap_or("").trim().is_empty(),
    })
}

/// Check what a controller typed before it goes into a profile. EuroScope's file is tab-separated, one setting per line, so tabs and line breaks are refused.
pub fn check_profile_details(name: &str, cid: &str, rating: u32, password: &str) -> Result<(), String> {
    let plain = |s: &str| !s.chars().any(|c| c == '\t' || c == '\n' || c == '\r' || c.is_control());
    let name = name.trim();
    if name.is_empty() || name.chars().count() > 60 || !plain(name) {
        return Err("enter your real name (as it should show on VATSIM)".into());
    }
    if cid.is_empty() || cid.len() > 9 || !cid.chars().all(|c| c.is_ascii_digit()) {
        return Err("your CID is the number of your VATSIM account (digits only)".into());
    }
    if !RATINGS.contains(&rating) {
        return Err("choose your controller rating".into());
    }
    if password.chars().count() > 64 || !plain(password) {
        return Err("that password cannot be saved".into());
    }
    Ok(())
}

fn set_line(lines: &mut Vec<String>, head: &str, value: &str) {
    let full = format!("{}{}", head, value);
    match lines.iter().position(|l| l.starts_with(head)) {
        Some(i) => lines[i] = full,
        None => lines.push(full),
    }
}

/// Put the name, CID, rating and (when given) password into one profile, leaving every other line as it was.
/// An empty password leaves the saved one alone. The TeamSpeak nickname is set to the CID when it is still the pack's placeholder, empty, or a number.
/// Returns true when the file changed.
pub fn write_profile_details(prf: &Path, name: &str, cid: &str, rating: u32, password: &str) -> Result<bool, String> {
    check_profile_details(name, cid, rating, password)?;
    let old = fs::read(prf).map_err(|e| e.to_string())?;
    let text = String::from_utf8(old).map_err(|_| "this profile has characters the app cannot safely rewrite, so it was left alone".to_string())?;
    let eol = if text.contains("\r\n") { "\r\n" } else { "\n" };
    let mut lines: Vec<String> = text.lines().map(String::from).collect();
    set_line(&mut lines, "LastSession\trealname\t", name.trim());
    set_line(&mut lines, "LastSession\tcertificate\t", cid);
    set_line(&mut lines, "LastSession\trating\t", &rating.to_string());
    if !password.is_empty() {
        set_line(&mut lines, "LastSession\tpassword\t", password);
    }
    let nick_head = "TeamSpeakVccs\tTs3NickName\t";
    let nick = lines.iter().find_map(|l| l.strip_prefix(nick_head)).map(|v| v.trim().to_string());
    if nick.as_deref().map(|v| v.is_empty() || v.eq_ignore_ascii_case("PUT YOUR CID HERE") || v.chars().all(|c| c.is_ascii_digit())).unwrap_or(true) {
        set_line(&mut lines, nick_head, cid);
    }
    let new = lines.join(eol) + eol;
    if new == text {
        return Ok(false);
    }
    let mut tmp_name = prf.file_name().map(|n| n.to_os_string()).unwrap_or_default();
    tmp_name.push(".new");
    let tmp = prf.with_file_name(tmp_name);
    fs::write(&tmp, new).map_err(|e| format!("could not write your profile: {}", e))?;
    if let Err(e) = fs::rename(&tmp, prf) {
        let _ = fs::remove_file(&tmp);
        return Err(format!("could not update your profile - close EuroScope and try again ({})", e));
    }
    Ok(true)
}

/// Where a pack should be unpacked. The zip holds a top-level folder (such as EXCXO) next to its .sct, .ese and .prf files.
/// If the folder the controller chose IS that folder, the pack goes in its parent, so it does not end up as EXCXO/EXCXO.
pub fn pack_dest(zip_path: &Path, root: &Path) -> Result<PathBuf, String> {
    let file = fs::File::open(zip_path).map_err(|e| e.to_string())?;
    let mut zip = zip::ZipArchive::new(file).map_err(|_| "the download is not a valid zip file".to_string())?;
    let me = root.file_name().map(|n| n.to_string_lossy().to_ascii_lowercase()).unwrap_or_default();
    for i in 0..zip.len() {
        let f = zip.by_index(i).map_err(|e| e.to_string())?;
        if let Some(rel) = f.enclosed_name() {
            let mut parts = rel.components();
            if let (Some(first), Some(_)) = (parts.next(), parts.next()) {
                if first.as_os_str().to_string_lossy().to_ascii_lowercase() == me {
                    return root.parent().map(|p| p.to_path_buf()).ok_or_else(|| "that folder has no parent folder".to_string());
                }
            }
        }
    }
    Ok(root.to_path_buf())
}

/// After a pack is in, remove the earlier builds of its sector and extension files (same name up to the first "_", older stamp).
fn remove_old_sector_files(zip_path: &Path, dest: &Path) {
    let Ok(file) = fs::File::open(zip_path) else { return };
    let Ok(mut zip) = zip::ZipArchive::new(file) else { return };
    let mut keep: Vec<String> = Vec::new();
    for i in 0..zip.len() {
        if let Ok(f) = zip.by_index(i) {
            if let Some(rel) = f.enclosed_name() {
                if rel.components().count() == 1 {
                    let n = rel.to_string_lossy().to_ascii_lowercase();
                    if n.ends_with(".sct") || n.ends_with(".ese") {
                        keep.push(n);
                    }
                }
            }
        }
    }
    let prefixes: Vec<String> = keep.iter().filter_map(|n| n.split_once('_').map(|(a, _)| format!("{}_", a))).collect();
    let Ok(rd) = fs::read_dir(dest) else { return };
    for e in rd.flatten() {
        let name = e.file_name().to_string_lossy().to_ascii_lowercase();
        if (name.ends_with(".sct") || name.ends_with(".ese")) && !keep.contains(&name) && prefixes.iter().any(|p| name.starts_with(p)) {
            let _ = fs::remove_file(e.path());
        }
    }
}

/// Unpack a sector pack zip into `dest`, replacing files of the same name. Returns how many files were written.
/// Paths that try to leave `dest` ("../x", absolute paths) are refused, and `vnaaats.cfg` (the controller's own key and settings) is never touched. Personal files (see `is_personal`) are not overwritten (an existing .prf is only pointed at the new sector file), and the plugin DLL is left to the plugin updater.
/// Each file is written beside its target and renamed into place, so a locked file stops the update with a plain message instead of leaving half a file.
pub fn extract_pack(zip_path: &Path, dest: &Path) -> Result<usize, String> {
    let file = fs::File::open(zip_path).map_err(|e| format!("could not open the download: {}", e))?;
    let mut zip = zip::ZipArchive::new(file).map_err(|_| "the download is not a valid zip file".to_string())?;
    // check every name first so a bad zip changes nothing
    for i in 0..zip.len() {
        let f = zip.by_index(i).map_err(|e| e.to_string())?;
        if f.enclosed_name().is_none() {
            return Err("the zip contains an unsafe file path, so it was not installed".into());
        }
    }
    let mut written = 0;
    for i in 0..zip.len() {
        let mut f = zip.by_index(i).map_err(|e| e.to_string())?;
        let rel = f.enclosed_name().ok_or("unsafe path")?;
        let target = dest.join(&rel);
        if f.is_dir() {
            fs::create_dir_all(&target).map_err(|e| e.to_string())?;
            continue;
        }
        if rel.file_name().map(|n| n.to_string_lossy().eq_ignore_ascii_case(CFG_NAME)).unwrap_or(false) {
            continue;
        }
        if rel.file_name().map(|n| n.to_string_lossy().eq_ignore_ascii_case(DLL_NAME)).unwrap_or(false) && target.exists() {
            continue;                           // an installed plugin is updated by its own check (the pack's copy may be older than the latest release)
        }                                       // (with no plugin at all, the pack's copy goes in; the plugin check then brings it to the latest)
        if target.exists() && is_personal(&rel) {
            // an existing profile keeps the controller's own details, but must point at the new sector file
            if rel.extension().map(|e| e.to_string_lossy().eq_ignore_ascii_case("prf")).unwrap_or(false) {
                let mut pack_text = String::new();
                f.read_to_string(&mut pack_text).ok();
                patch_prf_sector(&target, &pack_text)?;
            }
            if rel.file_name().map(|n| n.to_string_lossy().eq_ignore_ascii_case("voicechannels.txt")).unwrap_or(false) {
                patch_voice_channels(&target)?;
            }
            continue;                           // the controller's own settings are only installed when they have none
        }
        if let Some(parent) = target.parent() {
            fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        let mut tmp_name = target.file_name().map(|n| n.to_os_string()).unwrap_or_default();
        tmp_name.push(".new");
        let tmp = target.with_file_name(tmp_name);
        {
            let mut out = fs::File::create(&tmp).map_err(|e| format!("could not write {}: {}", rel.display(), e))?;
            std::io::copy(&mut f, &mut out).map_err(|e| format!("could not write {}: {}", rel.display(), e))?;
        }
        if let Err(e) = fs::rename(&tmp, &target) {
            let _ = fs::remove_file(&tmp);
            return Err(format!("could not replace {} - close EuroScope and try again ({})", rel.display(), e));
        }
        written += 1;
    }
    remove_old_sector_files(zip_path, dest);
    Ok(written)
}

pub fn is_exe(path: &Path) -> bool {
    path.is_file() && path.extension().map(|e| e.to_string_lossy().eq_ignore_ascii_case("exe")).unwrap_or(false)
}

/// Options the plugin no longer has. A refresh drops these lines instead of keeping them at the end of the file.
const RETIRED_OPTIONS: &[&str] = &["squawk"];

/// Bring vnaaats.cfg up to date with the template on the server: the comments and the list of options are replaced by the template's,
/// every value the controller already has (key, server, switches, settingsstamp ...) is kept, options the file does not have are
/// left out (the plugin's own defaults apply, so behaviour does not change), and lines the template does not know are kept at the end.
/// Returns true when the file was rewritten. A file that is not there is left alone (the key step creates it).
pub fn refresh_cfg(dir: &Path, template: &str) -> Result<bool, String> {
    let path = dir.join(CFG_NAME);
    let Ok(old) = fs::read_to_string(&path) else { return Ok(false) };
    let template = template.trim_start_matches('\u{feff}');
    if template.len() > 20000 || !template.starts_with('#') || !template.lines().any(|l| l.starts_with("server=")) {
        return Err("the server did not send a valid vnaaats.example.cfg".into());
    }
    let active = |l: &str| -> Option<(String, String)> {
        let t = l.trim();
        if t.is_empty() || t.starts_with('#') { return None; }
        t.split_once('=').map(|(k, v)| (k.trim().to_string(), v.trim().to_string()))
    };
    let mut have: Vec<(String, String)> = Vec::new();
    for l in old.lines() {
        if let Some((k, v)) = active(l) {
            if !have.iter().any(|(hk, _)| *hk == k) { have.push((k, v)); }
        }
    }
    let eol = if old.contains("\r\n") || old.is_empty() { "\r\n" } else { "\n" };
    let mut out: Vec<String> = Vec::new();
    let mut used: Vec<String> = Vec::new();
    for l in template.lines() {
        match active(l) {
            None => out.push(l.trim_end().to_string()),
            Some((k, _)) => {
                if let Some((_, v)) = have.iter().find(|(hk, _)| *hk == k) {
                    out.push(format!("{}={}", k, v));
                    used.push(k);
                }
            }
        }
    }
    let extras: Vec<&(String, String)> = have.iter().filter(|(k, _)| !used.contains(k) && !RETIRED_OPTIONS.contains(&k.as_str())).collect();
    if !extras.is_empty() {
        out.push(String::new());
        for (k, v) in extras { out.push(format!("{}={}", k, v)); }
    }
    while out.last().map(|l| l.is_empty()).unwrap_or(false) { out.pop(); }
    let text = out.join(eol) + eol;
    if text == old { return Ok(false); }
    let tmp = dir.join("vnaaats.cfg.tmp");
    fs::write(&tmp, text).map_err(|e| format!("could not write vnaaats.cfg: {}", e))?;
    fs::rename(&tmp, &path).map_err(|e| format!("could not save vnaaats.cfg: {}", e))?;
    Ok(true)
}

/// Does `tasklist /FO CSV /NH` output list a process called `exe_name` (case-insensitive)? Lines look like "EuroScope.exe","1234",...
pub fn tasklist_has(output: &str, exe_name: &str) -> bool {
    output.lines().any(|l| {
        let first = l.trim().trim_start_matches('"').split('"').next().unwrap_or("");
        first.eq_ignore_ascii_case(exe_name)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn refresh_cfg_keeps_values_and_updates_comments() {
        let d = tempfile::tempdir().unwrap();
        let tpl = "# new header\nserver=https://vnaaats.com\nkey=PASTE\n\n# autotype help\nautotype=1\n# highlight help\nhighlight=1\n";
        assert_eq!(refresh_cfg(d.path(), tpl), Ok(false));          // no file: left alone
        fs::write(d.path().join(CFG_NAME), "# old header\r\nserver=https://x.example\r\nkey=1234567.abc\r\nautotype=0\r\nsquawk=0\r\nsettingsstamp=5\r\n").unwrap();
        assert_eq!(refresh_cfg(d.path(), tpl), Ok(true));
        let t = fs::read_to_string(d.path().join(CFG_NAME)).unwrap();
        assert_eq!(t, "# new header\r\nserver=https://x.example\r\nkey=1234567.abc\r\n\r\n# autotype help\r\nautotype=0\r\n# highlight help\r\n\r\nsettingsstamp=5\r\n");
        assert!(!t.contains("highlight=") && !t.contains("PASTE"));  // not in their file: not added
        assert_eq!(refresh_cfg(d.path(), tpl), Ok(false));          // second time: nothing to do
        assert!(refresh_cfg(d.path(), "<html>nope").is_err());
    }

    #[test]
    fn finds_running_process() {
        let out = "\"System\",\"4\",\"Services\",\"0\",\"1 K\"\r\n\"EuroScope.exe\",\"4242\",\"Console\",\"1\",\"90,000 K\"\r\n";
        assert!(tasklist_has(out, "euroscope.exe"));
        assert!(!tasklist_has(out, "vacs.exe"));
        assert!(!tasklist_has("INFO: No tasks are running which match the specified criteria.", "vacs.exe"));
        assert!(!tasklist_has(out, "Euro"));
    }

    #[test]
    fn finds_dll_in_nested_folder() {
        let d = tempfile::tempdir().unwrap();
        let p = d.path().join("EXOCO/Plugins/vNAAATS");
        fs::create_dir_all(&p).unwrap();
        fs::write(p.join("vatsimnaaats.dll"), b"MZ").unwrap();           // upper/lower case does not matter
        assert_eq!(find_plugin_dll(d.path(), 5).unwrap(), p.join("vatsimnaaats.dll"));
        assert!(find_plugin_dll(d.path(), 1).is_none());                 // too deep for that limit
    }

    #[test]
    fn cfg_key_detection() {
        let d = tempfile::tempdir().unwrap();
        assert!(!read_cfg(d.path()).exists);
        fs::write(d.path().join(CFG_NAME), "server=https://x.example\nkey=PASTE_YOUR_API_KEY_HERE\n").unwrap();
        let i = read_cfg(d.path());
        assert!(i.exists && !i.has_key);
        assert_eq!(i.server, "https://x.example");
        fs::write(d.path().join(CFG_NAME), "# key=abc\nkey=1234567.abcdefghij\n").unwrap();
        assert!(read_cfg(d.path()).has_key);
    }

    #[test]
    fn set_key_keeps_other_lines_and_endings() {
        let d = tempfile::tempdir().unwrap();
        fs::write(d.path().join(CFG_NAME), "server=https://vnaaats.com\r\nkey=PASTE_YOUR_API_KEY_HERE\r\n# autotype=1 ok\r\nautotype=1\r\n").unwrap();
        set_cfg_key(d.path(), " 1234567.abcdefghijkl ").unwrap();
        let t = fs::read_to_string(d.path().join(CFG_NAME)).unwrap();
        assert_eq!(t, "server=https://vnaaats.com\r\nkey=1234567.abcdefghijkl\r\n# autotype=1 ok\r\nautotype=1\r\n");
    }

    #[test]
    fn set_key_creates_or_appends() {
        let d = tempfile::tempdir().unwrap();
        set_cfg_key(d.path(), "1234567.abcdefghijkl").unwrap();
        let i = read_cfg(d.path());
        assert!(i.exists && i.has_key && i.server == "https://vnaaats.com");
        let d2 = tempfile::tempdir().unwrap();
        fs::write(d2.path().join(CFG_NAME), "autotype=1\n").unwrap();
        set_cfg_key(d2.path(), "1234567.abcdefghijkl").unwrap();
        assert_eq!(fs::read_to_string(d2.path().join(CFG_NAME)).unwrap(), "autotype=1\nkey=1234567.abcdefghijkl\n");
    }

    #[test]
    fn a_pasted_key_label_is_dropped() {
        let d = tempfile::tempdir().unwrap();
        for pasted in ["key=1234567.abcdefghijkl", "KEY=1234567.abcdefghijkl", " \"key=1234567.abcdefghijkl\" ", "1234567.abcdefghijkl"] {
            set_cfg_key(d.path(), pasted).unwrap();
            let t = fs::read_to_string(d.path().join(CFG_NAME)).unwrap();
            let keys: Vec<&str> = t.lines().filter(|l| l.starts_with("key=")).collect();
            assert_eq!(keys, vec!["key=1234567.abcdefghijkl"], "{}", pasted);
        }
    }

    #[test]
    fn rejects_bad_keys() {
        let d = tempfile::tempdir().unwrap();
        for bad in ["", "short", "has space in it ok", "PASTE_YOUR_API_KEY_HERE", "a=b\nserver=evil"] {
            assert!(set_cfg_key(d.path(), bad).is_err(), "{}", bad);
        }
        assert!(!d.path().join(CFG_NAME).exists());
    }

    #[test]
    fn replace_file_checks_sha() {
        let d = tempfile::tempdir().unwrap();
        let f = d.path().join("ICAO_Airlines.txt");
        fs::write(&f, b"old").unwrap();
        assert!(replace_file(&f, b"new list", "deadbeef").is_err());
        assert!(replace_file(&f, b"", &sha256_hex(b"")).is_err());
        assert_eq!(fs::read(&f).unwrap(), b"old");
        replace_file(&f, b"new list", &sha256_hex(b"new list")).unwrap();
        assert_eq!(fs::read(&f).unwrap(), b"new list");
        assert!(!d.path().join("ICAO_Airlines.txt.new").exists());
    }

    #[test]
    fn replace_dll_checks_sha_and_header() {
        let d = tempfile::tempdir().unwrap();
        let dll = d.path().join(DLL_NAME);
        fs::write(&dll, b"MZold").unwrap();
        let new = b"MZnew-plugin".to_vec();
        assert!(replace_dll(&dll, &new, "deadbeef").is_err());
        assert!(replace_dll(&dll, b"<html>", &sha256_hex(b"<html>")).is_err());
        assert_eq!(fs::read(&dll).unwrap(), b"MZold");                  // untouched on failure
        replace_dll(&dll, &new, &sha256_hex(&new).to_uppercase()).unwrap();
        assert_eq!(fs::read(&dll).unwrap(), new);
        assert!(!d.path().join("VatsimNAAATS.dll.new").exists());
    }

    fn make_zip(dir: &Path, entries: &[(&str, &str)]) -> PathBuf {
        use std::io::Write;
        let p = dir.join("pack.zip");
        let mut w = zip::ZipWriter::new(fs::File::create(&p).unwrap());
        let opt = zip::write::SimpleFileOptions::default();
        for (n, c) in entries {
            w.start_file(*n, opt).unwrap();
            w.write_all(c.as_bytes()).unwrap();
        }
        w.finish().unwrap();
        p
    }

    #[test]
    fn extract_pack_replaces_files_and_keeps_cfg() {
        let t = tempfile::tempdir().unwrap();
        let dest = t.path().join("sector");
        fs::create_dir_all(dest.join("Plugins")).unwrap();
        fs::write(dest.join("Plugins").join(CFG_NAME), "key=mine").unwrap();
        fs::write(dest.join("EXCXO.sct"), "old").unwrap();
        let z = make_zip(t.path(), &[("EXCXO.sct", "new"), ("Plugins/vnaaats.cfg", "key=theirs"), ("Plugins/Sub/a.txt", "a")]);
        assert_eq!(extract_pack(&z, &dest).unwrap(), 2);
        assert_eq!(fs::read_to_string(dest.join("EXCXO.sct")).unwrap(), "new");
        assert_eq!(fs::read_to_string(dest.join("Plugins").join(CFG_NAME)).unwrap(), "key=mine");
        assert_eq!(fs::read_to_string(dest.join("Plugins/Sub/a.txt")).unwrap(), "a");
        assert!(!dest.join("EXCXO.sct.new").exists());
    }

    #[test]
    fn extract_pack_keeps_personal_settings_but_installs_them_when_missing() {
        let t = tempfile::tempdir().unwrap();
        let dest = t.path().join("sector");
        fs::create_dir_all(dest.join("EXCXO/Settings")).unwrap();
        fs::write(dest.join("EXCXO/Settings/Screen Layout.txt"), "mine").unwrap();
        fs::write(dest.join("EXCXO.prf"), "my login").unwrap();
        let z = make_zip(t.path(), &[("EXCXO/Settings/Screen Layout.txt", "pack"), ("EXCXO.prf", "pack"), ("EXCXO/Settings/General.txt", "pack"), ("EXCXO/Settings/VCCS.txt", "pack")]);
        extract_pack(&z, &dest).unwrap();
        assert_eq!(fs::read_to_string(dest.join("EXCXO/Settings/Screen Layout.txt")).unwrap(), "mine");
        assert_eq!(fs::read_to_string(dest.join("EXCXO.prf")).unwrap(), "my login");
        assert_eq!(fs::read_to_string(dest.join("EXCXO/Settings/General.txt")).unwrap(), "pack");
        assert_eq!(fs::read_to_string(dest.join("EXCXO/Settings/VCCS.txt")).unwrap(), "pack");   // missing, so installed
    }

    #[test]
    fn existing_voice_channels_get_the_fss_names_and_keep_everything_else() {
        let before = "VOICE\r\nAG:CZQO_CTR Gander Radio:131.700:Gander:CZQO_CTR\r\nAG:CZQO_H_CTR Gander Radio:131.075:novoice:CZQO_H_CTR\r\nAG:CZQM_CTR Moncton:132.200:novoice:CZQM_CTR\r\nAG:CZQO_DEL Gander Oceanic:128.700:novoice:CZQO_DEL\r\nAG:CZQO_X_CTR other:1:x:CZQO_X_CTR\r\n";
        let after = migrate_voice_channels(before);
        assert!(after.contains("AG:CZQO_FSS Gander Radio:131.700:Gander:CZQO_FSS\r\n"));
        assert!(after.contains("AG:CZQO_H_FSS Gander Radio:131.075:novoice:CZQO_H_FSS\r\n"));
        assert!(after.contains("AG:CZQM_CTR Moncton:132.200:novoice:CZQM_CTR"));
        assert!(after.contains("AG:CZQO_DEL Gander Oceanic"));
        assert!(after.contains("CZQO_X_CTR"));                  // not a Gander position
        assert_eq!(migrate_voice_channels(&after), after);      // nothing left to change
    }

    #[test]
    fn extract_pack_renames_gander_in_an_existing_voice_channel_list_but_keeps_the_rest() {
        let t = tempfile::tempdir().unwrap();
        let dest = t.path().join("sector");
        fs::create_dir_all(dest.join("EXCXO/Settings")).unwrap();
        fs::write(dest.join("EXCXO/Settings/VoiceChannels.txt"), "AG:CZQO_CTR Gander Radio:131.700:mine:CZQO_CTR\nAG:EGGX_CTR Shanwick:1:mine:EGGX_CTR\n").unwrap();
        let z = make_zip(t.path(), &[("EXCXO/Settings/VoiceChannels.txt", "pack")]);
        extract_pack(&z, &dest).unwrap();
        assert_eq!(fs::read_to_string(dest.join("EXCXO/Settings/VoiceChannels.txt")).unwrap(), "AG:CZQO_FSS Gander Radio:131.700:mine:CZQO_FSS\nAG:EGGX_CTR Shanwick:1:mine:EGGX_CTR\n");
    }

    #[test]
    fn profile_details_are_written_into_the_connect_dialog_lines_and_nothing_else() {
        let t = tempfile::tempdir().unwrap();
        let prf = t.path().join("OCA vNATTS.prf");
        fs::write(&prf, "Plugins\tPlugin1\t\\EXCXO\\Plugins\\vNAAATS V2\\VatsimNAAATS.dll\r\nTeamSpeakVccs\tTs3NickName\tPUT YOUR CID HERE\r\nLastSession\tcallsign\tCZQO_FSS\r\nLastSession\trating\t0\r\nLastSession\tpassword\tkeepme\r\n").unwrap();
        assert!(write_profile_details(&prf, " Gary Thomas ", "1248145", 4, "").unwrap());
        let text = fs::read_to_string(&prf).unwrap();
        assert!(text.contains("LastSession\trealname\tGary Thomas\r\n"));
        assert!(text.contains("LastSession\tcertificate\t1248145\r\n"));
        assert!(text.contains("LastSession\trating\t4\r\n"));
        assert!(text.contains("LastSession\tpassword\tkeepme\r\n"));            // an empty password leaves the saved one
        assert!(text.contains("TeamSpeakVccs\tTs3NickName\t1248145\r\n"));
        assert!(text.contains("LastSession\tcallsign\tCZQO_FSS\r\n"));
        assert!(!write_profile_details(&prf, "Gary Thomas", "1248145", 4, "").unwrap());   // nothing to change the second time
        assert!(write_profile_details(&prf, "Gary Thomas", "1248145", 4, "newpw").unwrap());
        assert!(fs::read_to_string(&prf).unwrap().contains("LastSession\tpassword\tnewpw\r\n"));
        let d = read_profile_details(&prf).unwrap();
        assert_eq!((d.realname.as_str(), d.certificate.as_str(), d.rating, d.has_password), ("Gary Thomas", "1248145", Some(4), true));
    }

    #[test]
    fn profile_details_refuse_odd_input_and_keep_a_custom_teamspeak_nickname() {
        assert!(check_profile_details("", "1", 4, "").is_err());
        assert!(check_profile_details("A\tB", "1", 4, "").is_err());
        assert!(check_profile_details("A B", "12x", 4, "").is_err());
        assert!(check_profile_details("A B", "1", 5, "").is_err());           // 5 is not a rating EuroScope offers
        assert!(check_profile_details("A B", "1", 4, "pa\nss").is_err());
        let t = tempfile::tempdir().unwrap();
        let prf = t.path().join("a.prf");
        fs::write(&prf, "TeamSpeakVccs\tTs3NickName\tG Thomas\nLastSession\tfacility\t1\n").unwrap();
        write_profile_details(&prf, "Gary", "1", 4, "").unwrap();
        let text = fs::read_to_string(&prf).unwrap();
        assert!(text.contains("Ts3NickName\tG Thomas\n"));
        assert!(text.contains("LastSession\tfacility\t1\n"));
        assert!(!text.contains('\r'));
    }

    #[test]
    fn only_profiles_that_load_the_vnaaats_plugin_are_found() {
        let t = tempfile::tempdir().unwrap();
        fs::create_dir_all(t.path().join("EXCXO")).unwrap();
        fs::write(t.path().join("mine.prf"), "Plugins\tPlugin1\t\\EXCXO\\Plugins\\vNAAATS V2\\VatsimNAAATS.dll\n").unwrap();
        fs::write(t.path().join("topsky.prf"), "Plugins\tPlugin1\tTopSky.dll\n").unwrap();
        assert_eq!(find_vnaaats_profiles(t.path()), vec![t.path().join("mine.prf")]);
        assert_eq!(find_vnaaats_profiles(&t.path().join("EXCXO")), vec![t.path().join("mine.prf")]);   // the EXCXO folder itself was chosen
    }

    #[test]
    fn extract_pack_skips_the_dll_repoints_the_profile_and_removes_old_sector_files() {
        let t = tempfile::tempdir().unwrap();
        let dest = t.path().join("data");
        fs::create_dir_all(dest.join("EXCXO/Plugins/vNAAATS V2")).unwrap();
        fs::write(dest.join("EXCXO/Plugins/vNAAATS V2").join(DLL_NAME), "latest").unwrap();
        fs::write(dest.join("P.prf"), "Settings\tname\tmine\r\nSettings\tsector\t\\old_1.sct\r\nSounds\tx\t1\r\n").unwrap();
        fs::write(dest.join("pack_1.sct"), "a").unwrap();
        fs::write(dest.join("pack_1.ese"), "a").unwrap();
        fs::write(dest.join("mine.sct"), "keep").unwrap();
        let z = make_zip(t.path(), &[("pack_2.sct", "b"), ("pack_2.ese", "b"), ("P.prf", "Settings\tsector\t\\pack_2.sct\r\nSettings\tname\tpack\r\n"), ("EXCXO/Plugins/vNAAATS V2/VatsimNAAATS.dll", "older")]);
        extract_pack(&z, &dest).unwrap();
        assert_eq!(fs::read_to_string(dest.join("EXCXO/Plugins/vNAAATS V2").join(DLL_NAME)).unwrap(), "latest");
        assert_eq!(fs::read_to_string(dest.join("P.prf")).unwrap(), "Settings\tname\tmine\r\nSettings\tsector\t\\pack_2.sct\r\nSounds\tx\t1\r\n");
        assert!(dest.join("pack_2.sct").exists() && dest.join("pack_2.ese").exists());
        assert!(!dest.join("pack_1.sct").exists() && !dest.join("pack_1.ese").exists());
        assert!(dest.join("mine.sct").exists());
    }

    #[test]
    fn pack_dest_steps_up_when_the_chosen_folder_is_the_packs_own_folder() {
        let t = tempfile::tempdir().unwrap();
        let z = make_zip(t.path(), &[("pack_1.sct", "a"), ("EXCXO/Settings/General.txt", "g")]);
        let inside = t.path().join("data").join("EXCXO");
        assert_eq!(pack_dest(&z, &inside).unwrap(), t.path().join("data"));
        let parent = t.path().join("data");
        assert_eq!(pack_dest(&z, &parent).unwrap(), parent);
    }

    #[test]
    fn reads_the_saved_key() {
        let d = tempfile::tempdir().unwrap();
        assert_eq!(read_key(d.path()), None);
        fs::write(d.path().join(CFG_NAME), "# key=ignored\r\nserver=x\r\nkey=PASTE_YOUR_API_KEY_HERE\r\n").unwrap();
        assert_eq!(read_key(d.path()), None);
        fs::write(d.path().join(CFG_NAME), "# key=ignored\r\nserver=x\r\nkey = 1234567.abc \r\n").unwrap();
        assert_eq!(read_key(d.path()), Some("1234567.abc".to_string()));
    }

    #[test]
    fn refresh_cfg_drops_retired_options_but_keeps_unknown_ones() {
        let d = tempfile::tempdir().unwrap();
        fs::write(d.path().join(CFG_NAME), "# old\r\nserver=https://x.example\r\nkey=1234567.abc\r\nsquawk=1\r\nmyoption=5\r\n").unwrap();
        assert!(refresh_cfg(d.path(), "# new\r\nserver=https://vnaaats.com\r\nkey=PASTE_YOUR_API_KEY_HERE\r\n").unwrap());
        let t = fs::read_to_string(d.path().join(CFG_NAME)).unwrap();
        assert_eq!(t, "# new\r\nserver=https://x.example\r\nkey=1234567.abc\r\n\r\nmyoption=5\r\n");
    }

    #[test]
    fn extract_pack_installs_the_dll_when_there_is_none() {
        let t = tempfile::tempdir().unwrap();
        let dest = t.path().join("dest");
        fs::create_dir_all(&dest).unwrap();
        let z = make_zip(t.path(), &[("EXCXO/Plugins/vNAAATS V2/VatsimNAAATS.dll", "pack copy")]);
        extract_pack(&z, &dest).unwrap();
        assert_eq!(fs::read_to_string(dest.join("EXCXO/Plugins/vNAAATS V2").join(DLL_NAME)).unwrap(), "pack copy");
    }

    #[test]
    fn extract_pack_refuses_paths_outside_the_folder() {
        let t = tempfile::tempdir().unwrap();
        let dest = t.path().join("sector");
        fs::create_dir_all(&dest).unwrap();
        let z = make_zip(t.path(), &[("ok.txt", "x"), ("../evil.txt", "x")]);
        assert!(extract_pack(&z, &dest).is_err());
        assert!(!dest.join("ok.txt").exists());          // nothing was written
        assert!(!t.path().join("evil.txt").exists());
    }

    #[test]
    fn extract_pack_rejects_a_non_zip() {
        let t = tempfile::tempdir().unwrap();
        let p = t.path().join("x.zip");
        fs::write(&p, "not a zip").unwrap();
        assert!(extract_pack(&p, t.path()).is_err());
    }

    #[test]
    fn contact_key_is_saved_read_and_replaced() {
        let d = tempfile::tempdir().unwrap();
        assert_eq!(read_cfg(d.path()).contact_key, None);
        std::fs::write(d.path().join(CFG_NAME), "key=1234567.abcdefghijkl\r\n# contactkey=99\r\nautotype=1\r\n").unwrap();
        assert_eq!(read_cfg(d.path()).contact_key, None);
        set_cfg_contact_key(d.path(), 36).unwrap();
        assert_eq!(read_cfg(d.path()).contact_key, Some(36));
        set_cfg_contact_key(d.path(), 46).unwrap();
        let text = std::fs::read_to_string(d.path().join(CFG_NAME)).unwrap();
        assert_eq!(text.matches("contactkey=46").count(), 1);
        assert!(text.contains("# contactkey=99") && text.contains("autotype=1") && text.contains("key=1234567.abcdefghijkl"));
        assert!(text.contains("\r\n"));
        assert!(set_cfg_contact_key(d.path(), 0).is_err());
        assert!(set_cfg_contact_key(d.path(), 255).is_err());
    }
}
