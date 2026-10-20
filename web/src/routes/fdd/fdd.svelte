<script lang="ts">
  import { onDestroy, onMount } from "svelte";
  import * as utils from "../../typescript/fdd/utils";
  import Button from "../../props/fdd/button.svelte";
  import Checkbox from "../../props/fdd/checkbox.svelte";
  import FlightStrip from "../../props/fdd/flight_strip.svelte";
  import * as dataHandler from "../../typescript/fdd/data";
  import * as site from "../../typescript/site";

  // Initialisation of data
  let isLoading: boolean = true;
  let noData: boolean = true;
  onMount(async () => {
    // Bye bye homepage
    site.setSiteSection(2);

    // Start the fetcher
    dataHandler.runDataFetcher();

    // Title
    site.setTitle("Flight Data Display");

  });

  onDestroy(async () => {
    // Hello homepage
    site.setSiteSection(0);
    clearInterval(dataHandler.acUpdateJob);
    clearInterval(dataHandler.tkUpdateJob);
  });

  // Level/track filter for the strip list (HL / LL / ALL TK / RR)
  const HL_MIN_LEVEL: number = 350; // flight levels >= this count as 'high level'
  let filter: string = "ALL";
  // Flight-type checkboxes. None ticked = show everything; otherwise show flights matching ANY ticked box.
  // (These are my interpretation of the original GAATS PST / FaD / WST / TERM boxes - see the tooltips.)
  let showPst: boolean = false;
  let showFad: boolean = false;
  let showWst: boolean = false;
  let showTerm: boolean = false;
  function typeMatch(ac: any, pst: boolean, fad: boolean, wst: boolean, term: boolean): boolean {
    if (!pst && !fad && !wst && !term) return true;
    if (pst && ac.Source != "vatsim") return true;
    if (fad && ac.Source == "vatsim") return true;
    if (wst && !ac.Direction) return true;
    if (term && Date.now() - ac.LastUpdated >= 30 * 60 * 1000) return true;
    return false;
  }
  function setFilter(f: string): void { filter = f; }
  function resetFilters(): void { filter = "ALL"; showPst = showFad = showWst = showTerm = false; }
  function matches(ac: any, track: string, f: string, pst: boolean, fad: boolean, wst: boolean, term: boolean): boolean {
    if (!ac.Relevant || !utils.isRecent(ac.LastUpdated)) return false;
    if (!typeMatch(ac, pst, fad, wst, term)) return false;
    switch (f) {
      case "HL": return ac.AssignedLevel >= HL_MIN_LEVEL;
      case "LL": return ac.AssignedLevel < HL_MIN_LEVEL;
      case "RR": return track == "RR";
      default: return true;
    }
  }
  function groupHasMatch(track: string, f: string, pst: boolean, fad: boolean, wst: boolean, term: boolean): boolean {
    const m = dataHandler.networkAircraft.get(track);
    if (!m) return false;
    for (const ac of m.values()) if (matches(ac, track, f, pst, fad, wst, term)) return true;
    return false;
  }

  // Externals
  $: acListKeys = [...dataHandler.networkAircraft.keys()];
  $: tmi = dataHandler.currentTMI;

  // Time string
  $: timeString = utils.parseTime(new Date());

  // DOM updater (1 second)
  setInterval(() => {
    // Reset externals
    acListKeys = [...dataHandler.networkAircraft.keys()];
    tmi = dataHandler.currentTMI;

    // Update time
    timeString = utils.parseTime(new Date());

    // Handle 'loading' and 'no content'
    if (dataHandler.networkAircraft.size > 1 || dataHandler.aircraftCount > 0) {
      // tracks are downloaded so we are no longer 'loading'
      isLoading = false;
      // Re-check every tick whether any aircraft exist
      let any: boolean = false;
      dataHandler.networkAircraft.forEach(function (value, key) {
        if (value.size > 0) any = true;
      });
      noData = !any;
    }
  }, 1000);
</script>

<!-- The main flight data panel with flight strips -->
<div class="flex flex-col w-full border-b-2 border-r-2 border-blue-10">
  <div class="flex flex-row justify-around w-full h-1/14 border-blue-10 border-b-2 text-white py-5 font-regular font-light">
    <div class="flex flex-row items-center justify-start 2xl:scale-100 xl:scale-85">
      <div class="hover:cursor-default">
        <Button label="Default" />
      </div>
      <div class="px-0.5 hover:cursor-default">
        <div class="flex items-center justify-between min-w-5 2xl:h-9 xl:h-8 px-1 border-2 ml-3 border-r-blue-8 border-b-blue-8 border-l-blue-0 border-t-blue-0">
          <Checkbox identifier={"pst"} label={"PST"} title={"Posted: flights a controller has cleared/posted through the plugin"} bind:checked={showPst} />
          <Checkbox identifier={"fad"} label={"FaD"} title={"Filed data: flights known only from their filed flight plan (VATSIM data feed)"} bind:checked={showFad} />
          <Checkbox identifier={"wst"} label={"WST"} title={"Westbound flights only"} bind:checked={showWst} />
          <Checkbox identifier={"term"} label={"TERM"} title={"Terminated: flights with no update for 30 minutes or more"} bind:checked={showTerm} />
        </div>
      </div>
    </div>
    <div class="flex flex-row items-center justify-end 2xl:w-1/5 xl:w-1/6 2xl:scale-100 xl:scale-85 xl:ml-5">
      <div class="flex-row px-3">
        <div class="select-none flex items-center h-10 px-3 text-2xl border-2 hover:cursor-default bg-blue-2 border-r-blue-8 border-b-blue-8 border-l-blue-0 border-t-blue-0">
          {timeString}
        </div>
      </div>
      <div class="flex-row px-3">
        <div class="select-none flex items-center h-10 px-3 text-2xl border-2 hover:cursor-default bg-blue-2 border-r-blue-8 border-b-blue-8 border-l-blue-0 border-t-blue-0">
          {tmi != "" ? tmi : "000"}
        </div>
      </div>
    </div>
    <div class="flex flex-row-reverse items-center 2xl:scale-100 xl:scale-85">
      <div class="px-3" on:click={resetFilters}>
        <Button label="Default" />
      </div>
      <div class="flex flex-row pr-1.5 font-bitmap font-thin 2xl:text-xl xl:text-base">
        {#each ["HL", "LL", "ALL", "RR"] as f}
          <div class="px-0.5" on:click={() => setFilter(f)}>
            <Button
              label={f == "ALL" ? "ALL TK" : f}
              font={"bitmap"}
              bold={"false"}
              textColour={filter == f ? "text-yellow-300" : "text-white"}
            />
          </div>
        {/each}
      </div>
    </div>
  </div>
  <div
    class="flex flex-col h-full font-bitmap font-light text-lg overflow-y-scroll overflow-x-hidden"
    style="font-size: clamp(0.65rem, 1.2vw, 1.125rem);">
    <!-- Aircraft component cards -->
    {#if isLoading}
      <div class="select-none flex flex-row w-full h-full justify-items-center justify-center text-2xl my-10 hover:cursor-default">
        Loading data...
      </div>
    {:else if noData}
      <div class="select-none flex flex-row w-full h-full justify-items-center justify-center text-2xl my-10 hover:cursor-default">
        No data on remote.
      </div>
    {:else}
      {#each acListKeys as acTrack}
        {#if groupHasMatch(acTrack, filter, showPst, showFad, showWst, showTerm)}
          <div class="flex flex-row justify-start w-full bg-grey-500 bg-opacity-70 select-none hover:cursor-default">
            <div class="w-1/2 px-3">
              {acTrack != "RR" ? acTrack : "RANDOM"}
            </div>
            <div class="flex flex-row w-full justify-start">
              {#if acTrack == "RR"}
                RANDOM ROUTINGS
              {:else}
                {#each dataHandler.currentNatTracks.get(acTrack).Route as waypoint}
                  <div class="w-1/10">
                    {waypoint.name}
                  </div>
                {/each}
              {/if}
            </div>
            <div class="1/7 px-3" />
          </div>
        {/if}
        {#each Array.from(dataHandler.networkAircraft.get(acTrack).values()) as ac}
        {#if matches(ac, acTrack, filter, showPst, showFad, showWst, showTerm)}
          <FlightStrip 
            callsign={ac.Callsign}
            type={ac.Type}
            assignedLevel={ac.AssignedLevel}
            assignedMach={ac.AssignedMach}
            track={ac.Track}
            route={ac.Route}
            routeEtas={ac.RouteEtas}
            departure={ac.Departure}
            arrival={ac.Arrival}
            trackedBy={ac.TrackedBy}
            isEquipped={ac.IsEquipped}
            direction={ac.Direction ?? false}
          />
          {/if}
        {/each}
      {/each}
    {/if}
  </div>
</div>
