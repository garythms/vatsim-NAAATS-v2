<script lang="ts">
  import type { Waypoint } from "../../typescript/fdd/objects";

  export let id: string;
  export let route: Waypoint[];
  export let flightLevels: number[];
  export let direction: number;
  export let validFrom: number;
  export let validTo: number;

  // Format the time validity
  let vFDate: Date = new Date(validFrom * 1000);
  let vTDate: Date = new Date(validTo * 1000);

  const hhmm = (d: Date): string => String(d.getUTCHours()).padStart(2, "0") + String(d.getUTCMinutes()).padStart(2, "0");
  // One short line: 1130-1900z
  let timeString = hhmm(vFDate) + "-" + hhmm(vTDate) + "z";
</script>

<div class="flex flex-col text-black select-none hover:cursor-default">
  <div
    class="flex flex-row px-3 py-1 {direction ? 'bg-blue-9' : 'bg-yellow-300'}"
  >
    <div class="flex flex-row flex-shrink-0 whitespace-nowrap" style="width: 7em;">
      {timeString}
    </div>
    <div class="flex flex-row flex-1 min-w-0">
      {#each flightLevels as level}
        <div class="pr-2.5">
          {level / 100}
        </div>
      {/each}
    </div>
  </div>
  <div
    class="flex flex-row px-3 py-1 {direction
      ? 'bg-blue-10'
      : 'bg-yellow-100'} border-b-2 border-black border-opacity-30"
  >
    <div class="flex flex-row flex-shrink-0" style="width: 7em;">
      {id}
    </div>
    <div class="flex flex-row flex-1 min-w-0">
      {#each route as wp}
        <div class="pr-2.5">
          {wp.name}
        </div>
      {/each}
    </div>
  </div>
</div>
