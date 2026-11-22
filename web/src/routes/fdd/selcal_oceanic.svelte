<script lang="ts">
  // TrackAudio for Oceanic and the SELCAL setting.
  // offer = true: the one-time question in the start-up window (shown only in FDD for Windows, only until it has been answered or installed).
  // offer = false: the same thing as a settings block: the SELCAL setting, the install status and the install / update button.
  import { onMount } from "svelte";
  import * as app from "../../typescript/fdd/app";
  import { selcalMode, setSelcalMode, selcalNeedsOceanic } from "../../typescript/fdd/selcal_sound";

  export let offer: boolean = false;

  const ASK = "fdd.oceanicAsk";                       // "" not answered, "later" = not now
  function asked(): string { try { return localStorage.getItem(ASK) ?? ""; } catch (e) { return ""; } }
  function remember(v: string): void { try { localStorage.setItem(ASK, v); } catch (e) { /* asked again next time */ } }

  let st: app.OceanicStatus | null = null;
  let busy = false;
  let msg = "";
  let hidden = offer && asked() !== "";
  const inApp = app.inApp();

  async function refresh(): Promise<void> {
    if (!inApp) return;
    try { st = await app.oceanicStatus(); } catch (e) { st = null; }
  }
  onMount(refresh);

  async function install(): Promise<void> {
    busy = true; msg = "Downloading TrackAudio for Oceanic...";
    try {
      msg = "✔ " + (await app.oceanicInstall());
      remember("done");
      selcalNeedsOceanic.set(false);
      await refresh();
    } catch (e) { msg = "✖ " + String(e); }
    busy = false;
  }
  function notNow(): void { remember("later"); hidden = true; }
  const BTN = "px-3 py-1 font-bold bg-blue-10 text-black";
</script>

{#if offer}
  {#if inApp && !hidden && st && !st.installed}
    <div class="mb-3 p-2 border border-blue-6 bg-blue-4 text-sm">
      <div class="font-bold">TrackAudio for Oceanic: SELCAL tones on your frequency</div>
      <div class="mt-1 opacity-90">The FDD and TrackAudio can now send the SELCAL tones out on your frequency, so the pilot hears them as well as getting the written alert. It needs a special build of TrackAudio.</div>
      <div class="mt-1 opacity-90">This is a custom-made <b>beta</b> version of TrackAudio for oceanic. Everything else in TrackAudio works as it does now, and it can still be used as a domestic ATC client. It will <b>replace any version of TrackAudio already installed</b> on this computer, and keeps your TrackAudio settings.</div>
      <div class="mt-1 opacity-90">If you are happy to go ahead, TrackAudio for Oceanic will download and install on this computer.</div>
      <div class="mt-1 text-xs opacity-70">Based on TrackAudio (GPL-3.0), modified by vNAAATS. It is not made or supported by the TrackAudio author, so please ask us, not them. <a class="underline" href="https://github.com/garythms/vatsim-NAAATS-v2/tree/trackaudio-selcal-build/tools/trackaudio-selcal" target="_blank" rel="noopener">Source for this modified version</a>.</div>
      {#if msg}<div class="mt-1 text-yellow-300">{msg}</div>{/if}
      <div class="mt-2">
        <button class="mr-2 {BTN}" disabled={busy} on:click={install}>{busy ? "Installing..." : "Yes, install it"}</button>
        <button class="px-3 py-1 border border-blue-6" disabled={busy} on:click={notNow}>Not now</button>
      </div>
      <div class="mt-1 text-xs opacity-70">TrackAudio is open source. Close TrackAudio first if it is running. You can install it later from Settings. <button class="underline" on:click={() => app.openExternal(app.OCEANIC_PAGE)}>Details and source</button></div>
    </div>
  {:else if msg.startsWith("✔")}
    <div class="mb-3 p-2 border border-blue-6 bg-blue-4 text-sm text-green-300">{msg}</div>
  {/if}
{:else}
  <div class="mx-3 mb-3 p-2 border border-blue-6 bg-blue-3 text-sm">
    <div class="font-bold">SELCAL tones</div>
    <label class="flex flex-row flex-wrap items-center gap-2 mt-1">When I send a SELCAL:
      <select class="px-1 py-0.5 text-black" value={$selcalMode} on:change={(e) => setSelcalMode(e.currentTarget.value === "off" ? "off" : e.currentTarget.value === "me" ? "me" : "all")}>
        <option value="all">Play the tones here and send them on my frequency</option>
        <option value="me">Play the tones here only</option>
        <option value="off">No tones</option>
      </select>
    </label>
    <div class="text-xs opacity-80 mt-1">Sending on your frequency uses the frequency that is set to transmit in TrackAudio, and needs TrackAudio for Oceanic. The written SELCAL alert is always sent as normal.</div>
    {#if $selcalNeedsOceanic}<div class="mt-1 text-yellow-300">Your TrackAudio cannot send SELCAL tones on the frequency. Install TrackAudio for Oceanic below.</div>{/if}
    <div class="mt-2 font-bold">TrackAudio for Oceanic</div>
    {#if inApp}
      <div class="text-xs opacity-80">{st ? (st.installed ? (st.update_available ? "Installed. A newer build is available." : "Installed and up to date.") : "Not installed from here yet.") : "Checking..."} It is a custom beta version of TrackAudio, still usable as a domestic ATC client, and replaces any TrackAudio already on this computer.</div>
      {#if st?.trackaudio_running}<div class="text-yellow-300 text-xs">TrackAudio is running: close it before installing.</div>{/if}
      {#if msg}<div class="mt-1 {msg.startsWith('✖') ? 'text-yellow-300' : ''}">{msg}</div>{/if}
      <button class="mt-1 px-3 py-1 {BTN}" disabled={busy} on:click={install}>{busy ? "Installing..." : st?.installed ? (st.update_available ? "Update TrackAudio for Oceanic" : "Reinstall TrackAudio for Oceanic") : "Install TrackAudio for Oceanic"}</button>
    {:else}
      <div class="text-xs opacity-80">FDD for Windows installs it for you. Otherwise <button class="underline" on:click={() => app.openExternal(app.OCEANIC_PAGE)}>download it here</button>.</div>
    {/if}
  </div>
{/if}
