<script lang="ts">
  import * as dataHandler from "../../typescript/fdd/data";
  import * as utils from "../../typescript/fdd/utils";
  import Titlebar from "../../props/fdd/titlebar.svelte";
  import TextInput from "../../props/fdd/text_input.svelte";
  import EditInput from "../../props/fdd/edit_input.svelte";
  import * as sess from "../../typescript/fdd/session";
  import type { Aircraft } from "../../typescript/fdd/objects";
  import { posOpen } from "../../typescript/fdd/posreports";

  const { session } = sess;
  $: editable = sess.canEdit($session);
  let message: string = "";
  let messageOk: boolean = true;
  let messageTimer: any;

  // Route amendment. Lat/long fixes must be two numbers / two numbers (51/20), never 51N20W.
  let routeDraft: string = "";
  let routeFor: string = "";
  let routeFocus: boolean = false;
  $: if (asel && !routeFocus && (asel.Callsign !== routeFor || true)) { routeFor = asel.Callsign; routeDraft = asel.Route.join(" "); }
  function routeProblem(text: string): string {
    const fixes = text.trim().toUpperCase().split(/\s+/).filter((f) => f !== "");
    if (fixes.length === 0) return "Type the route fixes separated by spaces, e.g. 52/50 52/40";
    for (const f of fixes) {
      if (/^\d{2}\/\d{2}$/.test(f) || /^[A-Z]{3,5}$/.test(f)) continue;
      if (/\d/.test(f)) return f + " is not accepted - write latitude/longitude as two numbers / two numbers, e.g. 51/20 (not 51N20W)";
      return f + " is not a fix name (3-5 letters, or two numbers / two numbers like 51/20)";
    }
    return "";
  }
  $: routeWarn = routeFocus ? routeProblem(routeDraft) : "";
  async function saveRoute(): Promise<void> {
    routeFocus = false;
    if (!asel) return;
    const bad = routeProblem(routeDraft);
    if (bad) { messageOk = false; message = bad; routeDraft = asel.Route.join(" "); return; }
    if (routeDraft.trim().toUpperCase().split(/\s+/).join(" ") === asel.Route.join(" ")) return;
    let fixes: string[] = routeDraft.trim().toUpperCase().split(/\s+/);
    if (!asel.Direction) fixes = fixes.reverse();     // the display shows westbound routes the other way round
    await save("route", fixes.join(" "));
  }

  async function save(field: string, text: string): Promise<void> {
    if (!asel) return;
    const digits = text.replace(/\D/g, "").replace(/^0+/, "");
    const changes: Record<string, string | number> = {};
    if (field === "mach") changes.mach = digits;
    else if (field === "level") changes.level = digits;
    else changes[field] = text;
    const err = await sess.editFlight(asel.Callsign, changes);
    messageOk = err === "";
    message = err === "" ? (field === "route" ? "Route saved on the display. EuroScope's own flight plan is not changed." : "Saved - the plugin picks it up within a few seconds") : err;
    clearTimeout(messageTimer);
    messageTimer = setTimeout(() => (message = ""), 6000);
  }

  function hhmm(ms: number): string {
    if (!ms) return "";
    const d = new Date(ms);
    return String(d.getUTCHours()).padStart(2, "0") + String(d.getUTCMinutes()).padStart(2, "0") + "z";
  }
  const names: Record<string, string> = { assignedLevel: "FL", assignedMach: "Mach", track: "Track", selcal: "SELCAL", route: "Route", state: "State" };
  function lastChange(ac: Aircraft): string {
    const h = ac.History?.length ? ac.History[ac.History.length - 1] : null;
    if (!h) return "";
    return Object.entries(h.changes).filter(([k]) => k !== "route").map(([k, v]) => (names[k] ?? k) + " " + (v[0] === "" || v[0] == null ? "-" : v[0]) + "\u2192" + (v[1] === "" ? "-" : v[1])).join(", ");
  }

  // Externals
  let asel: Aircraft;
  $: asel = dataHandler.asel;

  // Component updater (50ms)
  setInterval(() => {
    asel = dataHandler.asel;
  }, 50);
</script>

<div
  class="flex flex-col w-full h-1/3 overflow-hidden border-2 border-l-blue-0 border-t-blue-0 border-r-blue-10 border-b-blue-10">
  <Titlebar
    label="Flight Plan - {asel == null ? 'No selection' : asel.Cpdlc ? '[' + asel.Callsign + ']' : asel.Callsign}"/>
  {#if asel != null}
    <div
      class="flex flex-row items-center justify-start px-3 py-2 border-b-2 w-full border-b-blue-0">
      <TextInput
        label={"ACID"}
        identifier={"acid"}
        width={"w-32"}
        value={asel.Callsign}
        isDisabled={true}
      />
      <TextInput
        label={"Type"}
        identifier={"type"}
        width={"w-16"}
        value={asel.Type}
        isDisabled={true}
      />
      <TextInput
        label={"Depart"}
        identifier={"depart"}
        width={"w-14"}
        value={asel.Departure}
        isDisabled={true}
      />

      <TextInput
        label={"Etd"}
        identifier={"etd"}
        width={"w-16"}
        value={asel.Etd.toString() + "z"}
        isDisabled={true}
      />
      <EditInput
        label={"SELCAL"}
        width={"w-16"}
        value={asel.Selcal ?? ""}
        maxlength={5}
        {editable}
        onCommit={(t) => save("selcal", t)}
      />
      <TextInput
        label={"Datalink"}
        identifier={"datalink"}
        width={"w-32"}
        value={dataHandler.natTrakData.get(asel.Callsign) != null ? "Connected" : "Disconnected"}
        inputForeColour={"text-black"}
        backColour={dataHandler.natTrakData.get(asel.Callsign) != null ? "bg-green-400" : "bg-red-500"}
        isDisabled={true}
      />
      <!-- TODO: Change datalink colour if not connected -->
      <TextInput
        label={"Sector"}
        identifier={"sector"}
        width={"w-10"}
        value={asel.SectorID == "" ? "--" : asel.SectorID}
        isDisabled={true}
      />
    </div>
    <div class="flex flex-row items-top justify-start">
      <div class="flex flex-row justify-start w-1/4">
        <div
          class="flex flex-row items-center justify-center m-1.5 ml-2 w-48 h-8 bg-blue-9 text-black font-bitmap text-xl font-thin">
          {asel.Cpdlc ? "[" + asel.Callsign + "]" : asel.Callsign}
        </div>
      </div>
      <div class="flex flex-row items-start py-1.5 justify-start w-3/4">
        <EditInput
          label={"Spd"}
          width={"w-28"}
          value={"M" + String(asel.AssignedMach).padStart(3, "0")}
          {editable}
          onCommit={(t) => save("mach", t)}
        />
        <EditInput
          label={"FL"}
          width={"w-28"}
          value={asel.AssignedLevel.toString()}
          maxlength={5}
          {editable}
          onCommit={(t) => save("level", t)}
        />
        <TextInput
          label={"Dest"}
          identifier={"dest"}
          width={"w-14"}
          value={asel.Arrival}
          isDisabled={true}
          labelOnTop={false}
        />
      </div>
    </div>
    <div class="flex flex-row items-top pt-1 px-1 ml-1 justify-start">
      <div class="flex flex-row items-start overflow-x-scroll mt-2.5  px-1.5 py-0.5 justify-start w-5/6 h-16 bg-blue-9 
      border-2 border-l-blue-0 border-t-blue-0 border-r-blue-6 border-b-blue-6 text-black font-bitmap text-xl font-thin select-none cursor-default">
        <div class="flex flex-row">
          {#each asel.Route as point, i}
            <div class="flex flex-col pr-6">
              <div>
                {point}
              </div>
              <div>
                {asel.RouteEtas[i] == "--" ? "\xa0\xa0" : asel.RouteEtas[i]}
              </div>
            </div>
          {/each}
        </div>
      </div>
      <div class="flex flex-col px-2 w-1/3 relative">
        <label class="font-regular font-bold text-sm">Route (edit)</label>
        <input id="route-edit" class="h-6 px-1 text-base font-bitmap uppercase {editable ? 'bg-blue-1 text-yellow-300' : 'bg-blue-2 text-white'} border-2 border-l-blue-0 border-t-blue-0 border-r-blue-6 border-b-blue-6 focus:outline-none {routeWarn ? 'border-red-500' : 'focus:border-green-300'}"
          disabled={!editable} bind:value={routeDraft} on:focus={() => (routeFocus = true)} on:blur={saveRoute}
          on:keydown={(e) => { if (e.key === "Enter") e.currentTarget.blur(); if (e.key === "Escape") { routeFocus = false; routeDraft = asel.Route.join(" "); e.currentTarget.blur(); } }} />
        {#if routeWarn}<div class="absolute z-30 top-full left-2 mt-0.5 p-1 w-72 font-regular text-xs text-white bg-red-700 border border-red-300">{routeWarn}</div>{/if}
      </div>
      <EditInput
        label={"Tck"}
        width={"w-10"}
        value={asel.Track}
        maxlength={3}
        {editable}
        onCommit={(t) => save("track", t)}
      />
      <TextInput
        label={"State"}
        identifier={"level"}
        width={"w-9"}
        value={"UC"}
        isDisabled={true}
      />
    </div>
    <div class="flex flex-row items-top pt-0.5 w-full justify-self-start">
      <TextInput
        identifier={"level"}
        value={asel.IsEquipped ? "AGCS EQUIPPED" : "NON COMPLIANT"}
        isDisabled={true}
        widthWhole={"w-full"}
      />
    </div>
    <div class="px-3 pt-0.5 text-xs font-regular text-white leading-tight">
      {#if asel.IsConc}
        <button class="px-2 py-0.5 mb-0.5 font-bold border border-blue-6 {dataHandler.concOverdueFix(asel) !== '' && dataHandler.visualOn ? 'nt-flash text-black' : 'bg-blue-3'}"
          on:click={() => posOpen.set(true)}>Position reports{dataHandler.concOverdueFix(asel) !== "" ? " - OVERDUE at " + dataHandler.concOverdueFix(asel) : ""}</button>
      {/if}
      {#if message}
        <div class={messageOk ? "text-green-300" : "text-yellow-300"}>{message}</div>
      {/if}
      {#if asel.UpdatedBy}
        <div class="opacity-80" title="Who last changed level, Mach, track, SELCAL or route">
          Last change {hhmm(asel.UpdatedAt)} by {asel.UpdatedBy}{lastChange(asel) ? " - " + lastChange(asel) : ""}
        </div>
      {/if}
      {#if dataHandler.conflicts.get(asel.Callsign)}
        <div class="text-yellow-300">&#9888; {dataHandler.conflicts.get(asel.Callsign).join("; ")}</div>
      {/if}
      {#if !editable && $session.editing}
        <div class="opacity-60">View only - sign in and pick a position to edit</div>
      {/if}
    </div>
  {/if}
</div>
