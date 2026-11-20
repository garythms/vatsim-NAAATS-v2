<script lang="ts">
  // Shown in the startup dialogs: asks for the EuroScope details (before EuroScope is opened, so they can be written
  // into its profile), then greets the controller by the time of day on this computer, with local time and UTC.
  import { onMount, onDestroy } from "svelte";
  import * as app from "../../typescript/fdd/app";

  export let missing: boolean = false;              // true while the name or CID is still blank (the parent holds "open" back)
  export let loaded: boolean = false;
  let pf: app.ProfileInfo | null = null;
  let name = ""; let cid = ""; let rating = 4; let pass = "";
  let busy = false; let msg = ""; let edit = false;
  let now = Date.now();
  let timer: any = null;

  async function load(): Promise<void> {
    try {
      pf = await app.profileGet();
      name = pf.realname; cid = pf.certificate; rating = pf.rating ?? 4; pass = "";
    } catch (e) { pf = null; }
    loaded = true;
  }
  $: missing = !!pf && pf.profiles > 0 && (!pf.realname.trim() || !pf.certificate.trim());
  $: first = ((pf?.realname ?? "").trim().split(/\s+/)[0] ?? "").toLowerCase().replace(/^./, (c) => c.toUpperCase());
  $: hour = new Date(now).getHours();
  $: hello = hour < 5 ? "Good evening" : hour < 12 ? "Good morning" : hour < 18 ? "Good afternoon" : "Good evening";
  const two = (n: number) => String(n).padStart(2, "0");
  $: d = new Date(now);
  $: localT = two(d.getHours()) + ":" + two(d.getMinutes());
  $: utcT = two(d.getUTCHours()) + two(d.getUTCMinutes()) + "z";

  async function save(): Promise<void> {
    busy = true; msg = "";
    try {
      const n = await app.profileSave(name, cid, rating, pass);
      await load();
      edit = false;
      msg = "Saved to " + n + " EuroScope profile" + (n === 1 ? "" : "s") + ".";
    } catch (e) { msg = "✖ " + app.errText(e); }
    busy = false;
  }
  onMount(() => { load(); timer = setInterval(() => (now = Date.now()), 1000); });
  onDestroy(() => clearInterval(timer));
  const BTN = "px-3 py-1 font-bold bg-blue-10 text-black";
</script>

{#if pf && pf.profiles > 0}
  {#if !missing}
    <div class="mb-3 p-2 border border-blue-6 bg-blue-4 flex items-center justify-between gap-2">
      <div>
        <div class="font-bold">{hello}{first ? ", " + first : ""}</div>
        <div class="text-xs opacity-80">CID {pf.certificate} · <button class="underline" on:click={() => (edit = !edit)}>{edit ? "Hide details" : "Change my details"}</button></div>
      </div>
      <div class="text-right text-sm leading-tight">
        <div><span class="opacity-70 text-xs">Local</span> <b>{localT}</b></div>
        <div><span class="opacity-70 text-xs">UTC</span> <b>{utcT}</b></div>
      </div>
    </div>
  {:else}
    <div class="mb-3 p-2 border-2 border-yellow-400 text-sm">
      <b class="text-yellow-300">{hello}. Before EuroScope opens, enter your details.</b>
      <div class="opacity-80">These fill in EuroScope's Connect dialog. They are saved only in your EuroScope profile{pf.profiles === 1 ? "" : "s"} on this computer and are kept when the sector pack updates.</div>
    </div>
  {/if}
  {#if missing || edit}
    <form class="mb-3 grid gap-1 text-sm" on:submit|preventDefault={save}>
      <label>Real name <input class="w-full px-2 py-1 text-black" bind:value={name} autocomplete="off" /></label>
      <label>CID <input class="w-full px-2 py-1 text-black" bind:value={cid} inputmode="numeric" autocomplete="off" /></label>
      <label>Controller rating
        <select class="w-full px-2 py-1 text-black" bind:value={rating}>
          {#each app.RATINGS as r}<option value={r[0]}>{r[1]}</option>{/each}
        </select>
      </label>
      <label>VATSIM password <input class="w-full px-2 py-1 text-black" type="password" bind:value={pass} placeholder={pf.has_password ? "saved - leave blank to keep it" : "not saved yet"} autocomplete="new-password" /></label>
      <button class="{BTN} mt-1" disabled={busy || !name.trim() || !cid.trim() || pf.euroscope_running}>{busy ? "Saving..." : "Save"}</button>
      {#if pf.euroscope_running}<div class="text-yellow-300">Close EuroScope first. It rewrites its profile when it closes.</div>{/if}
    </form>
  {/if}
  {#if msg}<div class="mb-2 text-sm text-yellow-300">{msg}</div>{/if}
{/if}
