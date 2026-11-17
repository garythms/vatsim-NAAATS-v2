<script lang="ts">
  import { fade, fly } from "svelte/transition";
  import * as st from "../../typescript/fdd/settings";
  import * as sess from "../../typescript/fdd/session";
  import * as app from "../../typescript/fdd/app";
  import { whatsNewOpen } from "../../typescript/fdd/whatsnew";
  let setupAgain = app.inApp() && app.setupAgainGet();

  const { settingsOpen } = st;
  const { session } = sess;

  type Opt = { key: string; label: string; help: string; choices?: { v: string; t: string }[] };
  const yesNo = [{ v: "1", t: "On" }, { v: "0", t: "Off" }];
  const options: Opt[] = [
    { key: "autotype", label: "Auto-type into EuroScope", help: "The FDD buttons for SELCAL, Contact Me and the natTrak message type the command into EuroScope for you.", choices: yesNo },
    { key: "autoclick", label: "Click the aircraft after Contact Me", help: "After typing .contactme, the plugin also clicks the aircraft so the message is sent.", choices: yesNo },
    { key: "homecontact", label: "Contact Me with the Contact Me key", help: "The plugin empties the command line, rests the mouse on the callsign and presses the Contact Me key (Home unless you changed it, see below), which EuroScope answers by sending Contact Me. Off goes back to typing .contactme and clicking.", choices: yesNo },
    { key: "homefocus", label: "Press Home before typing", help: "Leave this Off. It is the old way of starting a typed command; it makes EuroScope write .contactme into the command line and can send a stray Contact Me. Turn it On only if typing stops working.", choices: yesNo },
    { key: "pastemessage", label: "Paste long messages instead of typing them", help: "Auto RCL and the natTrak message are pasted with Ctrl+V after the callsign, about 1 second instead of 6 to 8. Your copied text is put back afterwards. Turn it Off if the message does not appear in EuroScope.", choices: [{ v: "1", t: "On" }, { v: "0", t: "Off" }] },
    { key: "highlight", label: "Circle the aircraft when clicked here", help: "Clicking an aircraft on the FDD circles it in white in EuroScope for 3 seconds.", choices: yesNo },
    { key: "retrack", label: "Re-track aircraft automatically", help: "After a crash, a lost connection or a pilot reconnecting within 5 minutes, track the aircraft again.", choices: yesNo },
    { key: "remotelog", label: "Send problems to the vNAAATS server", help: "Warnings and errors go to the server so faults can be found quickly (never your key).", choices: [{ v: "1", t: "Warnings and errors" }, { v: "2", t: "Everything" }, { v: "0", t: "Off" }] },
  ];

  let values: Record<string, string> = {};
  let keyChoice: string = "home";      // "home" or "other"
  let recording: boolean = false;
  let firstTime: boolean = false;      // no Contact Me key has been saved yet
  const keyNames: Record<number, string> = { 8: "Backspace", 9: "Tab", 13: "Enter", 19: "Pause", 20: "Caps Lock", 27: "Esc", 32: "Space", 33: "Page Up", 34: "Page Down", 35: "End", 36: "Home", 37: "Left", 38: "Up", 39: "Right", 40: "Down", 45: "Insert", 46: "Delete", 144: "Num Lock", 145: "Scroll Lock" };
  function keyName(code: number): string {
    if (keyNames[code]) return keyNames[code];
    if (code >= 48 && code <= 57) return String.fromCharCode(code);
    if (code >= 65 && code <= 90) return String.fromCharCode(code);
    if (code >= 112 && code <= 135) return "F" + (code - 111);
    if (code >= 96 && code <= 105) return "Numpad " + (code - 96);
    return "key code " + code;
  }
  function onRecord(e: KeyboardEvent): void {
    if (!recording) return;
    e.preventDefault(); e.stopPropagation();
    const c = e.keyCode;
    if (c >= 1 && c <= 254) { values.contactkey = String(c); values = values; }
    recording = false;
  }
  function chooseKey(v: string): void {
    keyChoice = v;
    if (v === "home") values.contactkey = "36";
    else if (values.contactkey === "36") { values.contactkey = ""; recording = true; }
    else recording = true;
    values = values;
  }
  let loaded: boolean = false;
  let error: string = "";
  let saved: boolean = false;
  let busy: boolean = false;

  async function load(): Promise<void> {
    error = ""; saved = false; loaded = false;
    const r = await st.loadSettings();
    if (typeof r === "string") { error = r; return; }
    values = { autotype: "1", autoclick: "1", pastemessage: "1", homecontact: "1", contactkey: "36", homefocus: "0", highlight: "1", retrack: "1", remotelog: "1", ...(r.defaults ?? {}), ...r.settings };
    firstTime = r.settings?.contactkey === undefined;
    keyChoice = values.contactkey === "36" || !values.contactkey ? "home" : "other";
    recording = false;
    loaded = true;
  }
  $: if ($settingsOpen) load();

  // Until a controller has saved an answer to the Contact Me key question, open this window once per sign-in
  let askChecked: boolean = false;
  async function askIfUnanswered(): Promise<void> {
    askChecked = true;
    const r = await st.loadSettings();
    if (typeof r === "string" || r.settings?.contactkey !== undefined) return;
    if (app.inApp()) {     // FDD for Windows already asked and saved the answer in vnaaats.cfg: keep it on the server too, no need to ask again
      try { const a = await app.status(); if (a.cfg_contact_key) { await st.saveSettings({ contactkey: String(a.cfg_contact_key) }); return; } } catch (e) { /* ask below */ }
    }
    settingsOpen.set(true);
  }
  $: if ($session.loggedIn && !askChecked) askIfUnanswered();
  $: if (!$session.loggedIn) askChecked = false;

  async function save(): Promise<void> {
    busy = true; error = ""; saved = false;
    error = await st.saveSettings(values);
    busy = false;
    saved = error === "";
  }
</script>

<svelte:window on:keydown|capture={onRecord} />

{#if $settingsOpen}
  <div transition:fade={{ duration: 180 }} class="fixed inset-0 z-40 flex items-center justify-center bg-black bg-opacity-60" on:click|self={() => settingsOpen.set(false)}>
    <div transition:fly={{ y: 24, duration: 220 }} class="w-full max-w-xl max-h-full overflow-y-auto bg-blue-2 border-2 border-blue-10 text-white font-web">
      <div class="flex flex-row justify-between items-center px-3 py-1 bg-blue-3 font-bold">
        <span>Plugin settings (vnaaats.cfg)</span>
        {#if app.inApp()}<button class="ml-auto mr-2 px-3 py-0.5 text-sm font-bold border-2 border-blue-10 bg-blue-4 hover:bg-blue-10 hover:text-black" title="Open the FDD for Windows setup again (folder, key, EuroScope, VACS, audio client)" on:click={() => { settingsOpen.set(false); app.setupOpen.set(true); }}>App setup...</button>{/if}
        <button class="{app.inApp() ? '' : 'ml-auto'} mr-2 px-3 py-0.5 text-sm font-bold border-2 border-blue-10 bg-blue-4 hover:bg-blue-10 hover:text-black" title="What changed in the latest release" on:click={() => { settingsOpen.set(false); whatsNewOpen.set(true); }}>What's new</button>
        <button class="px-3 py-0.5 text-sm font-bold border-2 border-blue-10 bg-blue-4 hover:bg-blue-10 hover:text-black" on:click={() => settingsOpen.set(false)}>Close</button>
      </div>
      <div class="m-3 p-2 border-2 border-yellow-300 text-yellow-300 text-sm">
        <b>EuroScope must be closed and opened again for any change to take effect.</b>
        Your EuroScope saves the new choices into vnaaats.cfg within about 2 minutes of you pressing Save (it must be running with the plugin loaded).
      </div>
      {#if app.inApp()}
        <div class="mx-3 mb-3 p-2 border border-blue-6 bg-blue-3 text-sm">
          <div class="font-bold">Audio client: {app.audioGet() === "afv" ? "Audio for VATSIM" : app.audioGet() === "trackaudio" ? "TrackAudio" : "none chosen"}</div>
          <div class="text-xs opacity-80 mb-2">To switch between Audio for VATSIM and TrackAudio, have the setup screen shown again the next time FDD for Windows starts, and choose there.</div>
          {#if setupAgain}
            <span class="text-green-300">The setup screen will open the next time you start FDD for Windows.</span>
            <button class="ml-2 underline" on:click={() => { app.setupAgainSet(false); setupAgain = app.setupAgainGet(); }}>Cancel</button>
          {:else}
            <button class="px-3 py-1 font-bold bg-blue-10 text-black" on:click={() => { app.setupAgainSet(true); setupAgain = app.setupAgainGet(); }}>Change audio client at next start</button>
          {/if}
        </div>
      {/if}
      {#if error}<div class="mx-3 mb-2 text-yellow-300">{error}</div>{/if}
      {#if !$session.loggedIn}
        <div class="m-3 opacity-80">Sign in first.</div>
      {:else if loaded}
        <div class="mx-3 mb-3 p-2 border-2 {firstTime ? 'border-yellow-300' : 'border-blue-6'} bg-blue-3">
          <div class="font-bold">Contact Me key in EuroScope</div>
          <div class="text-xs opacity-80 mb-2">Contact Me works by pressing one key while the mouse rests on the callsign. EuroScope uses Home unless you changed it. Have you changed the Home key (Contact Me) in EuroScope?</div>
          <div class="flex flex-row flex-wrap items-center gap-3">
            <label><input type="radio" name="keychoice" checked={keyChoice === "home"} on:change={() => chooseKey("home")} /> No, it is still Home</label>
            <label><input type="radio" name="keychoice" checked={keyChoice === "other"} on:change={() => chooseKey("other")} /> Yes, I use another key</label>
          </div>
          {#if keyChoice === "other"}
            <div class="mt-2 flex flex-row items-center gap-3">
              <button class="px-3 py-1 font-bold bg-blue-10 text-black" on:click={() => (recording = true)}>{recording ? "Press the key now..." : "Record key"}</button>
              <span class="text-sm">{values.contactkey ? "Recorded: " + keyName(Number(values.contactkey)) : "No key recorded yet"}</span>
            </div>
          {/if}
          {#if firstTime}<div class="text-xs text-yellow-300 mt-2">Press Save to keep this choice.</div>{/if}
        </div>
        {#each options as o}
          <div class="mx-3 mb-3">
            <div class="flex flex-row justify-between items-center">
              <label class="font-bold" for={"set-" + o.key}>{o.label}</label>
              <select id={"set-" + o.key} class="text-black px-1" bind:value={values[o.key]}>
                {#each o.choices ?? [] as c}<option value={c.v}>{c.t}</option>{/each}
              </select>
            </div>
            <div class="text-xs opacity-80">{o.help}</div>
          </div>
        {/each}
        <div class="px-3 pb-3 flex flex-row items-center gap-3">
          <button class="px-4 py-1 font-bold bg-blue-10 text-black" disabled={busy} on:click={save}>{busy ? "Saving..." : "Save"}</button>
          {#if saved}<span class="text-green-300 text-sm">Saved. Close EuroScope and open it again once your EuroScope has updated vnaaats.cfg.</span>{/if}
        </div>
      {:else if !error}
        <div class="m-3 opacity-80">Loading...</div>
      {/if}
    </div>
  </div>
{/if}
