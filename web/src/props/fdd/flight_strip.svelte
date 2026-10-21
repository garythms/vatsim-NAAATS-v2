<script lang="ts">
  import * as utils from "../../typescript/fdd/utils";
  import * as dataHandler from "../../typescript/fdd/data";
  import { menuCallsign } from "../../typescript/fdd/acmenu";

  // Elements for spread prop
  export let callsign: string;
  export let type: string;
  export let assignedLevel: number;
  export let assignedMach: number;
  export let track: string;
  export let route: string[];
  export let routeEtas: string[];
  export let departure: string;
  export let arrival: string;
  //export let selcal: string;
  //export let datalink: boolean;
  export let direction: boolean;
  export let isEquipped: boolean;
  export let alert: boolean = false; // natTrak request waiting for action: flash
  //export let state: string;
  //export let relevant: boolean;
  export let trackedBy: string;
  export let ntSent: boolean = false;    // natTrak reminder sent: red star
  export let cpdlc: boolean = false;      // connected to CPDLC: callsign shown as [BAW123]
  export let cpdlcAlert: boolean = false; // unread CPDLC message: flash
  export let concOverdue: boolean = false; // Concorde overdue a position report: flash
  export let warning: string = "";   // conflict / sanity warning text ("" = none)
  export let updatedBy: string = "";
  export let updatedAt: number = 0;

  // A pencil shows for 10 minutes after someone changed level / Mach / track / SELCAL
  // RCL (oceanic clearance) processed on natTrak: when, and by whom if natTrak says
  $: clearanceTip = (() => {
    const nt: any = dataHandler.natTrakData.get(callsign.toUpperCase());
    const when: string = nt?.clearance_issued ? " at " + nt.clearance_issued : "";
    const who: string = nt?.cleared_by ? " by " + nt.cleared_by : "";
    return "RCL processed" + when + who + " (natTrak)";
  })();
  $: recentlyChanged = updatedBy !== "" && updatedAt > 0 && Date.now() - updatedAt < 10 * 60 * 1000;
  $: tip = [warning ? "\u26A0 " + warning : "", updatedBy ? "Last change by " + updatedBy + " at " + new Date(updatedAt).toISOString().slice(11, 16) + "z" : ""].filter((x) => x).join("\n");
  //export let targetMode: number;
  //export let sectorId: string;

  let colour: string = direction ? "bg-blue-10" : "bg-yellow-100";
  let darkerColour: string = direction ? "bg-blue-9" : "bg-yellow-200";
  let opacity: string = "bg-opacity-75";

  // Opacity
  let isActive: boolean = false;
  function stripMouseDown(): void {
    isActive = true;
  }
  function stripMouseUp(): void {
    isActive = false;
  }
</script>

<div class="hover:border-green-500 hover:border-2" title={tip}
  on:click={() => {
    if (dataHandler.isNatTrakAlert(callsign)) dataHandler.openNatTrakFor(callsign); // flashing request: open natTrak
    dataHandler.ackNatTrak(callsign);
    dataHandler.setAsel(callsign);
  }}>
  {#if track === "RR"}
    <div class="flex flex-col items-center justify-end w-full h-8 {darkerColour} {isActive ? 'bg-opacity-75' : ''} text-black hover:cursor-default"
      on:mousedown={stripMouseDown}
      on:mouseup={stripMouseUp}
      on:mouseleave={stripMouseUp}>
      <div class="flex flex-row justify-start items-center w-full">
        <div class="w-1/7 flex-shrink-0" />
        <div class="flex flex-row flex-1 min-w-0 overflow-hidden">
          <div style="width: 14.8rem; flex-shrink: 0;" />
          {#each route as rte}
            <div style="width: 4.6rem; flex-shrink: 0;" class="select-none hover:cursor-default whitespace-nowrap overflow-hidden">
              {rte}
            </div>
          {/each}
        </div>
      </div>
    </div>
  {/if}
  <div class="flex flex-row items-center w-full h-8 {colour} {isActive ? 'bg-opacity-75' : ''} {alert || cpdlcAlert || concOverdue ? 'nt-flash' : ''} text-black hover:cursor-default"
    on:mousedown={stripMouseDown}
    on:mouseup={stripMouseUp}
    on:mouseleave={stripMouseUp}>
    <div class="flex flex-row w-1/7 flex-shrink-0 px-2 hover:cursor-pointer select-none" title="Click for the aircraft menu" on:click|stopPropagation={() => { dataHandler.setAsel(callsign); dataHandler.ackNatTrak(callsign); menuCallsign.set(callsign); }}>
      {(cpdlc ? "[" + callsign + "]" : callsign) + (!isEquipped ? "*" : "")}{#if ntSent}<span class="text-red-600 font-bold pl-0.5" style="font-size: 1.5em; line-height: 0.6;" title="natTrak reminder sent to the pilot">*</span>{/if}{#if warning}<span class="text-red-600 pl-1">&#9888;</span>{/if}{#if recentlyChanged}<span class="pl-1">&#9998;</span>{/if}
    </div>
    <div class="flex flex-row flex-1 min-w-0 overflow-hidden hover:cursor-default select-none">
      <!-- columns: dep/arr 8rem, level 3rem, Mach 3.8rem, then each ETA 4.6rem (the header row in fdd.svelte uses the same widths) -->
      <div class="whitespace-nowrap overflow-hidden pr-1" style="width: 8rem; flex-shrink: 0; font-size: 0.88em;">
        {departure}/{arrival}
      </div>
      <div style="width: 3rem; flex-shrink: 0;">
        {assignedLevel}
      </div>
      <div style="width: 3.8rem; flex-shrink: 0;">
        {assignedMach > 0 ? "M" + String(assignedMach).padStart(3, "0") : ""}
      </div>
      {#each routeEtas as eta}
        <div style="width: 4.6rem; flex-shrink: 0;">
          {eta != "--" ? eta : "\xa0\xa0\xa0\xa0"}
        </div>
      {/each}
    </div>
    <div class="flex flex-row items-center justify-end flex-shrink-0 select-none hover:cursor-default" style="width: 6.5rem;">
      <div style="width: 3.6rem;" class="whitespace-nowrap">{type}</div>
      <div style="width: 2rem;" class="flex items-center justify-center">
        {#if trackedBy === "NATTRAK"}
          <span title={clearanceTip} class="inline-flex" aria-label="Clearance processed on natTrak">
            <svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor"><path d="M19 3h-4.2A3 3 0 0 0 12 1a3 3 0 0 0-2.8 2H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V5a2 2 0 0 0-2-2zm-7 0a1 1 0 1 1 0 2 1 1 0 0 1 0-2zm-2 14-4-4 1.4-1.4L10 14.2l6.6-6.6L18 9z" /></svg>
          </span>
        {:else if trackedBy != ""}
          <span title={"Tracked by " + trackedBy} class="inline-flex" aria-label={"Tracked by " + trackedBy}>
            <svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor"><path d="M12 2a9 9 0 0 0-9 9v7a3 3 0 0 0 3 3h1v-8H5v-2a7 7 0 0 1 14 0v2h-2v8h2v1a2 2 0 0 1-2 2h-3v2h3a4 4 0 0 0 4-4V11a9 9 0 0 0-9-9z" /></svg>
          </span>
        {/if}
      </div>
    </div>
  </div>
</div>