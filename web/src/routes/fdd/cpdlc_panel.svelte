<script lang="ts">
  import { onDestroy, onMount } from "svelte";
  import * as cp from "../../typescript/fdd/cpdlc";
  import * as dataHandler from "../../typescript/fdd/data";
  import * as sess from "../../typescript/fdd/session";

  const { cpdlcStatus, cpdlcOpen, cpdlcPick, cpdlcFreetexts } = cp;
  const { session } = sess;

  let messages: cp.CpdlcMessage[] = [];
  let loadError: string = "";
  let text: string = "";
  let ra: string = "WU";
  let replyTo: cp.CpdlcMessage | null = null;
  let sendMsg: string = "";
  let sendOk: boolean = true;
  let busy: boolean = false;
  let timer: any;
  let thread: HTMLElement;
  let lastSeenFor: string = "";
  let unicomAsk: boolean = false;
  let hoppieCode: string = "";
  let logonErr: string = "";
  let logonBusy: boolean = false;

  async function signIn(): Promise<void> {
    logonBusy = true;
    logonErr = await cp.logonCpdlc(hoppieCode.trim());
    logonBusy = false;
    if (logonErr === "") hoppieCode = "";          // do not keep the code in the page
  }
  function useFree(e: Event): void {
    const sel = e.target as HTMLSelectElement;
    const f = $cpdlcFreetexts[parseInt(sel.value)];
    if (f) { text = f.text; ra = f.ra === "NE" ? "NE" : "R"; replyTo = null; }
    sel.value = "";
  }
  async function doUnicom(): Promise<void> {
    unicomAsk = false;
    busy = true;
    const err = await cp.sendUnicom(shown);
    busy = false;
    sendOk = err === "";
    sendMsg = err === "" ? "UNICOM message sent. The pilot is logged off when they answer WILCO/ROGER." : err;
    if (err === "") refresh();
    setTimeout(() => (sendMsg = ""), 8000);
  }
  function signOut(): void { messages = []; cp.logoffCpdlc(); }

  $: connected = $cpdlcStatus.connected;
  $: canUse = sess.canEdit($session);
  // Which aircraft is shown: the one picked, else the one selected on the FDD if it is connected, else the first connected
  $: shown = $cpdlcPick && connected.some((c) => c.callsign === $cpdlcPick)
    ? $cpdlcPick
    : (dataHandler.asel && connected.some((c) => c.callsign === dataHandler.asel.Callsign) ? dataHandler.asel.Callsign : connected[0]?.callsign ?? "");

  async function refresh(): Promise<void> {
    if (!$cpdlcOpen || !shown || !canUse || !$cpdlcStatus.active) { return; }
    const r = await cp.loadMessages(shown);
    if (typeof r === "string") { loadError = r; return; }
    loadError = "";
    const grew = r.length !== messages.length || r[r.length - 1]?.id !== messages[messages.length - 1]?.id;
    messages = r;
    if (grew && thread) setTimeout(() => (thread.scrollTop = thread.scrollHeight), 30);
    if (messages.some((m) => m.dir === "DOWN" && !m.seen) && lastSeenFor !== shown + messages[messages.length - 1].id) {
      lastSeenFor = shown + messages[messages.length - 1].id;
      cp.markSeen(shown);
    }
  }

  onMount(() => { timer = setInterval(refresh, 3000); });
  onDestroy(() => clearInterval(timer));
  $: if ($cpdlcOpen && shown) { messages = []; replyTo = null; refresh(); }

  function pick(callsign: string): void { cpdlcPick.set(callsign); }
  function useTemplate(t: { text: string; ra: string }): void { text = t.text; ra = t.ra; }
  function answer(m: cp.CpdlcMessage, t: { text: string; ra: string }): void { replyTo = m; text = t.text; ra = t.ra; }
  function hhmm(ts: number): string {
    const d = new Date(ts * 1000);
    return String(d.getUTCHours()).padStart(2, "0") + String(d.getUTCMinutes()).padStart(2, "0") + "z";
  }

  async function send(): Promise<void> {
    if (!text.trim() || !shown) return;
    busy = true;
    const err = await cp.sendUplink(shown, text, ra, replyTo ? replyTo.min : null);
    busy = false;
    sendOk = err === "";
    sendMsg = err === "" ? "Sent" : err;
    if (err === "") { text = ""; replyTo = null; refresh(); }
    setTimeout(() => (sendMsg = ""), 5000);
  }
  function key(e: KeyboardEvent): void { if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); send(); } }

  const stateColour: Record<string, string> = { WILCO: "text-green-300", ROGER: "text-green-300", AFFIRM: "text-green-300", UNABLE: "text-red-400", NEGATIVE: "text-red-400", STANDBY: "text-yellow-300", SENT: "opacity-60", OPEN: "text-yellow-300", ANSWERED: "opacity-60" };
</script>

<!-- Pull-out CPDLC panel: slides in from the right edge -->
<div class="fixed z-40 top-0 bottom-0 right-0 flex flex-row font-regular text-white" style="transform: translateX({$cpdlcOpen ? '0' : '100%'}); transition: transform 0.25s ease;">
  <div class="w-96 max-w-screen h-full flex flex-col bg-blue-2 border-l-2 border-blue-10 shadow-5xl">
    <div class="flex flex-row justify-between items-center px-3 py-2 bg-blue-3 font-bold select-none">
      <span>CPDLC {$cpdlcStatus.active && $cpdlcStatus.station ? "- station " + $cpdlcStatus.station : ""}</span>
      <span class="text-sm">
        {#if $cpdlcStatus.active}<span class="underline hover:cursor-pointer mr-3" on:click={signOut}>sign out of CPDLC</span>{/if}
        <span class="underline hover:cursor-pointer" on:click={() => cpdlcOpen.set(false)}>close</span>
      </span>
    </div>
    {#if !$cpdlcStatus.enabled}
      <div class="p-3 text-sm opacity-80">CPDLC is switched off on this server.</div>
    {:else}
      {#if canUse && !$cpdlcStatus.active}
        <div class="p-3 border-b border-blue-4 text-sm">
          {#if !$cpdlcStatus.stationForPosition}
            <div>Your position has no CPDLC station, so you cannot use CPDLC from it.</div>
          {:else if $cpdlcStatus.inUse}
            <div class="text-yellow-300">{$cpdlcStatus.stationForPosition} is already being used by another controller.</div>
          {:else}
            <div class="font-bold mb-1">Sign in to CPDLC as {$cpdlcStatus.stationForPosition}</div>
            <div class="opacity-80 mb-2">Type your own Hoppie logon code. It is kept in server memory only while you are signed in, and is never saved.</div>
            <form on:submit|preventDefault={signIn}>
              <input type="password" autocomplete="off" class="w-full px-2 py-1 text-black" placeholder="Your Hoppie code" bind:value={hoppieCode} />
              <button class="mt-2 px-4 py-1 font-bold bg-blue-10 text-black" disabled={logonBusy || !hoppieCode.trim()}>{logonBusy ? "Checking..." : "Sign in"}</button>
            </form>
            {#if logonErr}<div class="mt-1 text-yellow-300">{logonErr}</div>{/if}
          {/if}
        </div>
      {/if}
      {#if $cpdlcStatus.error}
        <div class="px-3 py-1 text-xs text-yellow-300">Hoppie: {$cpdlcStatus.error}</div>
      {/if}
      <div class="px-3 py-1 text-xs opacity-70 select-none">Connected aircraft ({connected.length}){$cpdlcStatus.autoAccept ? " - logons accepted automatically" : ""}</div>
      <div class="flex flex-row flex-wrap gap-1 px-2 pb-2 border-b border-blue-4" style="max-height: 7rem; overflow-y: auto;">
        {#each connected as c}
          <button class="px-2 py-0.5 text-sm border border-blue-6 {c.callsign === shown ? 'bg-blue-10 text-black' : 'bg-blue-3'} {c.unread > 0 && !($cpdlcOpen && c.callsign === shown) ? 'nt-flash text-black' : ''}"
            on:click={() => pick(c.callsign)}>[{c.callsign}]{c.unread > 0 ? " (" + c.unread + ")" : ""}</button>
        {/each}
        {#if connected.length === 0}
          <span class="text-sm opacity-70 px-1">Nobody is connected.{$cpdlcStatus.station ? " Pilots send a logon request to " + $cpdlcStatus.station + "." : ""}</span>
        {/if}
      </div>
      {#if !canUse}
        <div class="p-3 text-sm opacity-80">Sign in and choose a position (not Observer) to read and send CPDLC messages.</div>
      {:else if !$cpdlcStatus.active}
        <!-- sign-in form is shown above -->
      {:else if shown}
        <div class="px-3 py-1 font-bold border-b border-blue-4 select-none">[{shown}]</div>
        <div class="flex-1 min-h-0 overflow-y-auto px-2 py-1 text-sm" bind:this={thread}>
          {#if loadError}<div class="text-yellow-300">{loadError}</div>{/if}
          {#each messages as m}
            <div class="my-1 px-2 py-1 border-l-4 {m.dir === 'UP' ? 'border-blue-10 bg-blue-3' : 'border-yellow-300 bg-blue-4'}">
              <div class="flex flex-row justify-between text-xs opacity-80">
                <span>{m.dir === "UP" ? "▲ UPLINK" : "▼ DOWNLINK"} {hhmm(m.ts)}{m.by && m.by !== "auto" ? " " + m.by : ""}</span>
                <span class={stateColour[m.state] ?? ""}>{m.state === "LOGON" ? "" : m.state}{m.dir === "DOWN" && !m.seen ? " NEW" : ""}</span>
              </div>
              <div class="font-bold break-words">{m.readable}</div>
              {#if m.dir === "DOWN" && m.state === "OPEN"}
                <div class="flex flex-row gap-2 mt-1 text-xs">
                  <span class="underline hover:cursor-pointer" on:click={() => answer(m, { text: "UNABLE", ra: "NE" })}>unable</span>
                  <span class="underline hover:cursor-pointer" on:click={() => answer(m, { text: "STANDBY", ra: "NE" })}>standby</span>
                  <span class="underline hover:cursor-pointer" on:click={() => answer(m, { text: "ROGER", ra: "NE" })}>roger</span>
                  <span class="underline hover:cursor-pointer" on:click={() => answer(m, { text: "", ra: "WU" })}>reply...</span>
                </div>
              {/if}
            </div>
          {/each}
          {#if messages.length === 0 && !loadError}<div class="opacity-70 px-1">No messages yet.</div>{/if}
        </div>
        <div class="px-2 py-2 border-t-2 border-blue-10">
          <div class="flex flex-row flex-wrap gap-1 mb-1">
            {#each cp.templates as t}
              <button class="px-1.5 py-0.5 text-xs bg-blue-3 border border-blue-6 hover:bg-blue-4" on:click={() => useTemplate(t)}>{t.label}</button>
            {/each}
          </div>
          <div class="flex flex-row items-center gap-2 mb-1">
            <select class="text-black text-xs flex-1 min-w-0" on:change={useFree}>
              <option value="">Standard messages...</option>
              {#each $cpdlcFreetexts as f, i}<option value={i}>{f.text.length > 60 ? f.text.slice(0, 58) + "..." : f.text}</option>{/each}
            </select>
            {#if !unicomAsk}
              <button class="px-2 py-0.5 text-xs font-bold bg-yellow-300 text-black" on:click={() => (unicomAsk = true)}>UNICOM</button>
            {:else}
              <span class="text-xs text-yellow-300">Send [{shown}] to UNICOM?</span>
              <button class="px-2 py-0.5 text-xs font-bold bg-yellow-300 text-black" on:click={doUnicom}>Yes</button>
              <button class="px-2 py-0.5 text-xs bg-blue-3 border border-blue-6" on:click={() => (unicomAsk = false)}>No</button>
            {/if}
          </div>
          {#if replyTo}<div class="text-xs text-yellow-300 mb-1">Replying to: {replyTo.readable} <span class="underline hover:cursor-pointer" on:click={() => (replyTo = null)}>cancel</span></div>{/if}
          <textarea class="w-full px-2 py-1 text-black uppercase" rows="2" maxlength="240" placeholder="Message to [{shown}] - @ marks the changeable parts" bind:value={text} on:keydown={key}></textarea>
          <div class="flex flex-row items-center gap-2 mt-1 text-sm">
            <label>Reply wanted
              <select class="text-black" bind:value={ra}>
                <option value="WU">Wilco / Unable</option>
                <option value="AN">Affirm / Negative</option>
                <option value="R">Roger</option>
                <option value="NE">None</option>
              </select>
            </label>
            <button class="ml-auto px-4 py-1 font-bold bg-blue-10 text-black" disabled={busy || !text.trim()} on:click={send}>Send</button>
          </div>
          {#if sendMsg}<div class="text-xs mt-1 {sendOk ? 'text-green-300' : 'text-yellow-300'}">{sendMsg}</div>{/if}
        </div>
      {:else}
        <div class="p-3 text-sm opacity-80">Select a connected aircraft above.</div>
      {/if}
    {/if}
  </div>
</div>
