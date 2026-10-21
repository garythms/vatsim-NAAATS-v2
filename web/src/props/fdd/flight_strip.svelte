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
  export let cpdlc: boolean = false;      // connected to CPDLC: callsign shown as [BAW123]
  export let cpdlcAlert: boolean = false; // unread CPDLC message: flash
  export let concOverdue: boolean = false; // Concorde overdue a position report: flash
  export let warning: string = "";   // conflict / sanity warning text ("" = none)
  export let updatedBy: string = "";
  export let updatedAt: number = 0;

  // A pencil shows for 10 minutes after someone changed level / Mach / track / SELCAL
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
        <div class="flex flex-row w-0.32" />
        <div class="flex flex-row w-0.68">
          {#each route as rte}
            <div class="w-1/11 select-none hover:cursor-default whitespace-nowrap">
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
    <div class="flex flex-row w-1/7 px-2 hover:cursor-pointer select-none" title="Click for the aircraft menu" on:click|stopPropagation={() => { dataHandler.setAsel(callsign); dataHandler.ackNatTrak(callsign); menuCallsign.set(callsign); }}>
      {(cpdlc ? "[" + callsign + "]" : callsign) + (!isEquipped ? "*" : "")}{#if warning}<span class="text-red-600 pl-1">&#9888;</span>{/if}{#if recentlyChanged}<span class="pl-1">&#9998;</span>{/if}
    </div>
    <div class="flex flex-row w-5/7 hover:cursor-default select-none">
      <!-- TODO fix this alignment -->
      <div class="w-1/7 whitespace-nowrap pr-1" style="font-size: 0.88em;">
        {departure}/{arrival}
      </div>
      <div class="w-1/20">
        {assignedLevel}
      </div>
      <div class="w-1/12 mr-2">
        {assignedMach > 0 ? "M" + String(assignedMach).padStart(3, "0") : ""}
      </div>
      {#each routeEtas as eta}
        <div class="w-1/11">
          {eta != "--" ? eta : "\xa0\xa0\xa0\xa0"}
        </div>
      {/each}
    </div>
    <div class="flex flex-row justify-end w-1/5 hover:cursor-default select-none">
      <div class="w-1/3">{type}</div>
      <div class="w-3/7">
        {trackedBy != "" ? trackedBy : "\xa0"}
      </div>
    </div>
  </div>
</div>