<script lang="ts">
  import { onMount } from "svelte";
  import * as app from "../../typescript/fdd/app";

  let show: boolean = false;
  let st: app.AppStatus | null = null;
  let busy: boolean = false;
  let msg: string = "";
  let key: string = "";
  let autoES: boolean = true;
  let autoVacs: boolean = true;
  let audio: app.AudioChoice = "none";
  let autoAudio: boolean = true;

  const SEEN = "vnaaats.appSetupSeen";
  function seen(): boolean { try { return sessionStorage.getItem(SEEN) === "1"; } catch (e) { return false; } }

  let problem: string = "";                         // why the app did not answer
  async function refresh(): Promise<void> {
    try {
      st = await Promise.race([app.status(), new Promise<never>((_, no) => setTimeout(() => no("the app did not answer within 15 seconds"), 15000))]);
      problem = "";
    } catch (e) { problem = app.errText(e); console.error("vNAAATS app:", e); }
  }
  onMount(async () => {
    if (!app.inApp() || seen()) return;
    await app.welcomeDone;
    await refresh();
    autoES = app.autoGet("euroscope"); autoVacs = app.autoGet("vacs");
    audio = app.audioGet(); autoAudio = app.autoGet(audio === "none" ? "afv" : audio);
    show = true;
  });

  $: listOk = !!st && st.airlines_up_to_date !== false;
  $: ready = !!st && st.root !== null && st.dll_found && st.up_to_date === true && listOk && st.cfg_has_key;

  async function run(f: () => Promise<unknown>, ok: string = ""): Promise<void> {
    busy = true; msg = "";
    try { await f(); if (ok) msg = ok; } catch (e) { msg = "✖ " + app.errText(e); }
    await refresh();
    busy = false;
  }
  const folder = () => run(() => app.pickFolder());
  const update = () => run(async () => { const v = await app.updatePlugin(); msg = "Updated: " + v + "."; });
  const saveKey = () => run(async () => { await app.saveKey(key); key = ""; }, "Key saved.");
  const VACS_URL = "https://vacs.network/";
  const pick = (k: app.ProgKind) => run(async () => {
    msg = "A file window should open (it can appear behind this one)...";
    const r = await app.pickExe(k);
    msg = r ? "" : "Nothing was chosen.";
  });
  const AUDIO_URL: Record<string, string> = { afv: "https://audio.vatsim.net/", trackaudio: "https://github.com/pierr3/TrackAudio/releases" };
  const AUDIO_NAME: Record<string, string> = { afv: "Audio for VATSIM", trackaudio: "TrackAudio" };
  function chooseAudio(c: app.AudioChoice): void { audio = c; app.audioSet(c); if (c !== "none") autoAudio = app.autoGet(c); }
  const getAudio = () => run(() => app.openExternal(AUDIO_URL[audio], true), "Opening the download page in your browser...");
  const getVacs = () => run(() => app.openExternal(VACS_URL, true), "Opening the VACS page in your browser...");
  function done(): void {
    try { sessionStorage.setItem(SEEN, "1"); } catch (e) { /* shown again next time */ }
    show = false;
  }
  const STEP = "mb-3 p-2 border border-blue-6 bg-blue-4";
  const BTN = "px-3 py-1 font-bold bg-blue-10 text-black";
</script>

{#if show}
  <div class="fixed inset-0 z-[60] flex items-center justify-center overflow-y-auto" style="background: #0b1b30;">
    <div class="w-[34rem] max-w-full max-h-full overflow-y-auto bg-blue-3 border-2 border-blue-10 text-white font-regular p-4">
      <div class="text-xl font-bold mb-1">vNAAATS setup</div>
      <div class="mb-3 text-sm opacity-80">A quick check that this computer is ready{st ? " (app " + st.app_version + ")" : ""}.</div>
      {#if !st && problem}
        <div class="mb-2 text-yellow-300 text-sm break-words">The app could not check this computer: {problem}</div>
        <div class="flex gap-2">
          <button class={BTN} on:click={refresh}>Try again</button>
          <button class="px-3 py-1 border border-blue-6" on:click={done}>Continue anyway</button>
        </div>
      {:else if !st}
        <div>Checking...</div>
      {:else}
        <div class={STEP}>
          <b>1. Your EXOCO folder</b>
          {#if st.root}<div class="text-sm opacity-80 break-all">{st.root}</div>{/if}
          <button class="{BTN} mt-1" disabled={busy} on:click={folder}>{st.root ? "Choose a different folder" : "Choose your EXOCO folder"}</button>
        </div>
        {#if st.root}
          <div class={STEP}>
            <b>2. The vNAAATS plugin</b>
            {#if !st.dll_found}
              <div class="text-yellow-300 text-sm">VatsimNAAATS.dll was not found in that folder. Put it in EXOCO/Plugins/vNAAATS, or choose another folder.</div>
            {:else if st.up_to_date === true && st.airlines_up_to_date !== false}
              <div class="text-green-300 text-sm">✔ Plugin{st.latest_version ? " " + st.latest_version : ""} and airline list are up to date</div>
            {:else if st.up_to_date === false || st.airlines_up_to_date === false}
              <div class="text-yellow-300 text-sm">
                {#if st.up_to_date === false}A newer plugin{st.latest_version ? " (" + st.latest_version + ")" : ""} is available.{/if}
                {#if st.airlines_up_to_date === false}{st.airlines_found ? "A newer" : "No"} airline list (ICAO_Airlines.txt){st.airlines_found ? " is available" : " is installed"}.{/if}
                Close EuroScope first, then update.
              </div>
              <button class="{BTN} mt-1" disabled={busy} on:click={update}>Update now</button>
            {:else}
              <div class="text-yellow-300 text-sm">Could not check for the latest plugin{st.error ? ": " + st.error : ""}.</div>
            {/if}
          </div>
          {#if st.dll_found}
            <div class={STEP}>
              <b>3. Your key</b>
              {#if st.cfg_has_key}
                <div class="text-green-300 text-sm">✔ A key is saved in vnaaats.cfg</div>
              {:else}
                <div class="text-yellow-300 text-sm">No key in vnaaats.cfg yet. Paste the key from your Gander Oceanic vNAAATS page.</div>
                <form class="flex gap-2 mt-1" on:submit|preventDefault={saveKey}>
                  <input class="flex-1 px-2 py-1 text-black" placeholder="1234567.abc..." bind:value={key} autocomplete="off" />
                  <button class={BTN} disabled={busy || !key}>Save</button>
                </form>
              {/if}
            </div>
          {/if}
        {/if}
        <div class={STEP}>
          <b>{st.root ? "4" : "2"}. EuroScope</b>
          {#if st.euroscope}<div class="text-sm opacity-80 break-all">{st.euroscope}</div>{/if}
          <div class="flex gap-2 mt-1">
            <button class="px-3 py-1 border border-blue-6" disabled={busy} on:click={() => pick("euroscope")}>{st.euroscope ? "Change" : "Find EuroScope.exe"}</button>
          </div>
          <label class="flex items-center gap-2 mt-2 text-sm"><input type="checkbox" bind:checked={autoES} on:change={() => app.autoSet("euroscope", autoES)} /> Open EuroScope for me when I choose my position</label>
        </div>
        <div class={STEP}>
          <b>{st.root ? "5" : "3"}. VACS (voice calls between controllers, optional)</b>
          {#if st.vacs}<div class="text-sm opacity-80 break-all">{st.vacs}</div>{/if}
          <div class="flex gap-2 mt-1">
            <button class="px-3 py-1 border border-blue-6" disabled={busy} on:click={() => pick("vacs")}>{st.vacs ? "Change" : "Find the VACS program"}</button>
            {#if !st.vacs}<button class="px-3 py-1 underline" disabled={busy} on:click={getVacs}>Get VACS</button>{/if}
          </div>
          {#if !st.vacs}<div class="text-xs opacity-70 mt-1 break-all">Or open this address yourself: {VACS_URL}</div>{/if}
          <label class="flex items-center gap-2 mt-2 text-sm"><input type="checkbox" bind:checked={autoVacs} on:change={() => app.autoSet("vacs", autoVacs)} /> Open VACS for me when I choose my position</label>
        </div>
        <div class={STEP}>
          <b>{st.root ? "6" : "4"}. Audio client (voice with pilots)</b>
          <div class="flex flex-wrap gap-x-4 gap-y-1 mt-1 text-sm">
            <label><input type="radio" name="audioc" checked={audio === "afv"} on:change={() => chooseAudio("afv")} /> Audio for VATSIM</label>
            <label><input type="radio" name="audioc" checked={audio === "trackaudio"} on:change={() => chooseAudio("trackaudio")} /> TrackAudio</label>
            <label><input type="radio" name="audioc" checked={audio === "none"} on:change={() => chooseAudio("none")} /> Neither</label>
          </div>
          {#if audio !== "none"}
            {#if st[audio]}<div class="text-sm opacity-80 break-all mt-1">{st[audio]}</div>{/if}
            <div class="flex gap-2 mt-1">
              <button class="px-3 py-1 border border-blue-6" disabled={busy} on:click={() => pick(audio)}>{st[audio] ? "Change" : "Find " + AUDIO_NAME[audio]}</button>
              {#if !st[audio]}<button class="px-3 py-1 underline" disabled={busy} on:click={getAudio}>Get {AUDIO_NAME[audio]}</button>{/if}
            </div>
            {#if !st[audio]}<div class="text-xs opacity-70 mt-1 break-all">Or open this address yourself: {AUDIO_URL[audio]}</div>{/if}
            <label class="flex items-center gap-2 mt-2 text-sm"><input type="checkbox" bind:checked={autoAudio} on:change={() => app.autoSet(audio === "none" ? "afv" : audio, autoAudio)} /> Open {AUDIO_NAME[audio]} for me when I choose my position</label>
          {/if}
        </div>
        {#if msg}<div class="mb-2 text-sm text-yellow-300">{msg}</div>{/if}
        <button class="w-full py-2 font-bold {ready ? 'bg-blue-10 text-black' : 'border border-blue-6'}" on:click={done}>{ready ? "Continue" : "Continue anyway"}</button>
      {/if}
    </div>
  </div>
{/if}
