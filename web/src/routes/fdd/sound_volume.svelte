<script lang="ts">
  import { soundVolumes, soundKinds, setMaster, setLevel, resetVolumes } from "../../typescript/fdd/sounds";
  import type { SoundKind } from "../../typescript/fdd/sounds";
  import * as dataHandler from "../../typescript/fdd/data";
  import * as cp from "../../typescript/fdd/cpdlc";
  import * as h from "../../typescript/fdd/handoff";
  import { playSelcal } from "../../typescript/fdd/selcal_sound";

  let open: boolean = false;

  // Plays one sound at its set volume, whatever the on/off switches say, so the slider can be judged by ear
  function test(kind: SoundKind): void {
    if (kind === "natrak") dataHandler.playPing();
    else if (kind === "cpdlc") cp.playCpdlcSound(true);
    else if (kind === "rcl") cp.playRclSound(true);
    else if (kind === "handoff") h.chime(true);
    else if (kind === "selcal") playSelcal("ABCD", true);
  }
  function num(e: Event): number { return Number((e.currentTarget as HTMLInputElement).value); }
</script>

<span class="underline hover:cursor-pointer" title="Volume and mute for each alert sound" on:click={() => (open = !open)}>Volume</span>

{#if open}
  <div class="fixed top-14 right-4 z-50 w-96 bg-blue-2 text-white font-regular border-2 border-blue-10 shadow-5xl" style="font-size: 0.8rem;">
    <div class="flex flex-row justify-between items-center px-3 py-1.5 bg-blue-3 font-bold select-none">
      <span>Alert sound volumes</span>
      <button class="px-2 border border-blue-6 hover:bg-blue-4" on:click={() => (open = false)}>Close</button>
    </div>
    <div class="px-3 py-2 border-b border-blue-4">
      <div class="flex flex-row items-center gap-2">
        <span class="w-28 font-bold">All sounds</span>
        <input type="range" min="0" max="100" step="5" class="flex-1" value={$soundVolumes.master} on:input={(e) => setMaster(num(e))} />
        <span class="w-10 text-right">{$soundVolumes.master === 0 ? "Muted" : $soundVolumes.master + "%"}</span>
      </div>
      <div class="opacity-70 mt-1">Each sound below is multiplied by this. The Audio ON/OFF switch above still turns every alert off.</div>
    </div>
    {#each soundKinds as s}
      <div class="px-3 py-1.5 border-b border-blue-4">
        <div class="flex flex-row items-center gap-2">
          <span class="w-28 font-bold">{s.label}</span>
          <input type="range" min="0" max="100" step="5" class="flex-1" value={$soundVolumes.levels[s.kind]} on:input={(e) => setLevel(s.kind, num(e))} />
          <span class="w-10 text-right">{$soundVolumes.levels[s.kind] === 0 ? "Muted" : $soundVolumes.levels[s.kind] + "%"}</span>
          <button class="px-2 border border-blue-6 hover:bg-blue-4" title="Play this sound at its volume" on:click={() => test(s.kind)}>Test</button>
        </div>
        <div class="opacity-70">{s.note}</div>
      </div>
    {/each}
    <div class="flex flex-row justify-between items-center px-3 py-1.5">
      <span class="opacity-70">Saved on this computer only.</span>
      <button class="px-2 border border-blue-6 hover:bg-blue-4" on:click={resetVolumes}>Reset to full</button>
    </div>
  </div>
{/if}
