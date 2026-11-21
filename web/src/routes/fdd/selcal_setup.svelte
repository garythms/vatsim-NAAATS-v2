<script lang="ts">
  // Optional set-up step in FDD for Windows: SELCAL tones on the controller's frequency. This is the only place the set-up is done;
  // afterwards the SELCAL tab of an aircraft only has an on/off tick box.
  import { onMount } from "svelte";
  import * as app from "../../typescript/fdd/app";
  import { selcalDevice, setSelcalFreq, setSelcalDevice, listOutputs } from "../../typescript/fdd/selcal_sound";

  let cable: boolean | null = null;
  let busy = false;
  let note = "";
  let steps = false;
  async function check(): Promise<void> { try { cable = await app.cableFound(); } catch (e) { cable = null; } }
  onMount(check);
  async function setUp(): Promise<void> {
    busy = true; note = "";
    const r = await listOutputs();                  // Windows asks once for the microphone so the device names can be read; nothing is recorded
    const c = r.devices.find((d) => d.id === r.cable);
    if (c) { setSelcalDevice(c.id, c.label); setSelcalFreq(true); steps = true; }
    else note = r.named ? "The cable's sound output was not found." : "Windows did not show the device names. Allow the microphone for this app and try again.";
    busy = false;
  }
  function remove(): void { setSelcalDevice("", ""); setSelcalFreq(false); steps = false; }
  const BTN = "px-3 py-1 font-bold bg-blue-10 text-black";
</script>

<b>SELCAL tones on your frequency (optional)</b>
<div class="text-sm opacity-90 mb-1">Lets the pilot hear the SELCAL tones on your frequency when you press Send. It uses TrackAudio and a virtual audio cable (VB-Cable).</div>
{#if cable === false}
  <div class="text-sm text-yellow-300 mb-1">No virtual cable found. Install VB-Cable, <b>restart Windows</b>, then open FDD for Windows again: this step will finish then.</div>
  <button class="mr-2 {BTN}" on:click={() => app.openExternal("https://vb-audio.com/Cable/")}>Get VB-Cable</button>
  <button class="px-3 py-1 border border-blue-6" on:click={check}>Check again</button>
{:else if cable && !$selcalDevice}
  <div class="text-sm text-green-300 mb-1">✓ Virtual cable found.</div>
  <button class={BTN} disabled={busy} on:click={setUp}>{busy ? "Setting up..." : "Set it up"}</button>
  {#if note}<div class="mt-1 text-sm text-yellow-300">{note}</div>{/if}
{:else if cable && $selcalDevice}
  <div class="text-sm text-green-300 mb-1">✓ Set up. Switch the tones on or off in the SELCAL tab of any aircraft.</div>
  <details class="text-sm" open={steps}>
    <summary class="cursor-pointer underline">The two settings that go with it</summary>
    <ol class="list-decimal ml-5 mt-1">
      <li>In TrackAudio's settings, set the microphone to <b>CABLE Output</b>, and make sure TX is on for your frequency.</li>
      <li>To keep your voice going out: in Windows sound settings, open your real microphone's properties, tick <b>Listen to this device</b> and play it through <b>CABLE Input</b>.</li>
    </ol>
  </details>
  <button class="mt-1 text-sm underline opacity-80" on:click={remove}>Remove this set-up</button>
{:else if cable === null}
  <div class="text-sm opacity-80">Checking for a virtual cable...</div>
{/if}
