<script lang="ts">
  // One-time question in the start-up window: if a virtual audio cable is installed, ask whether SELCAL tones should also be sent on the
  // controller's frequency. Yes = set-up steps (cable already chosen); No = never asked again. Without a cable nothing is shown.
  import { onMount } from "svelte";
  import * as app from "../../typescript/fdd/app";
  import { setSelcalMode, setSelcalDevice, listOutputs } from "../../typescript/fdd/selcal_sound";

  const KEY = "fdd.selcalFreqAsk";                  // "" not asked, "yes", "no"
  function asked(): string { try { return localStorage.getItem(KEY) ?? ""; } catch (e) { return ""; } }
  function remember(v: string): void { try { localStorage.setItem(KEY, v); } catch (e) { /* asked again next time */ } }

  let stage: "none" | "ask" | "steps" = "none";
  let note = "";
  onMount(async () => {
    if (asked() || !app.inApp()) return;
    try { if (await app.cableFound()) stage = "ask"; } catch (e) { /* no cable check: say nothing */ }
  });
  async function yes(): Promise<void> {
    remember("yes"); setSelcalMode("all"); stage = "steps";
    const r = await listOutputs();                  // Windows asks once for the microphone so the device names can be read; nothing is recorded
    const c = r.devices.find((d) => d.id === r.cable);
    if (c) setSelcalDevice(c.id, c.label); else note = "The cable output could not be chosen automatically. Choose it later in the SELCAL tab of an aircraft.";
  }
  function no(): void { remember("no"); stage = "none"; }
  const BTN = "px-3 py-1 font-bold bg-blue-10 text-black";
</script>

{#if stage === "ask"}
  <div class="mb-3 p-2 border border-blue-6 bg-blue-4 text-sm">
    <div class="font-bold">A virtual audio cable (VB-Cable) is installed.</div>
    <div class="mb-2 opacity-90">Do you want the SELCAL tones sent on your frequency too, so pilots hear them? It uses TrackAudio and can be switched off at any time.</div>
    <button class="mr-2 {BTN}" on:click={yes}>Yes, set it up</button>
    <button class="px-3 py-1 border border-blue-6" on:click={no}>No thanks</button>
  </div>
{:else if stage === "steps"}
  <div class="mb-3 p-2 border border-blue-6 bg-blue-4 text-sm">
    <div class="font-bold mb-1">SELCAL on your frequency is switched on. Two Windows settings are left, once:</div>
    <ol class="list-decimal ml-5 mb-2">
      <li>Press <b>Open Sound settings</b> below. A window called "Sound" opens on the <b>Recording</b> tab.</li>
      <li>Right-click your <b>headset microphone</b> (the one marked "Default Device") and choose <b>Properties</b>.</li>
      <li>Open the <b>Listen</b> tab, tick <b>Listen to this device</b>, and under "Playback through this device" choose <b>CABLE Input (VB-Audio Virtual Cable)</b>. Press <b>OK</b>.</li>
      <li>Open <b>TrackAudio</b>'s settings and set its microphone to <b>CABLE Output</b>. Make sure TX is on for your frequency.</li>
    </ol>
    {#if note}<div class="mb-2 text-yellow-300">{note}</div>{/if}
    <button class="mb-2 px-3 py-1 font-bold bg-blue-10 text-black" on:click={() => app.openSoundPanel()}>Open Sound settings</button>
    <div class="mb-2 opacity-80">When you press Send on a SELCAL, TrackAudio's push-to-talk is keyed for about 3 seconds. Change it any time in the SELCAL tab.</div>
    <button class={BTN} on:click={() => (stage = "none")}>Done</button>
  </div>
{/if}
