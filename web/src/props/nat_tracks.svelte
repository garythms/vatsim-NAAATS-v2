<script lang="ts">
  import { onMount } from "svelte";

  // The NAT tracks in force now, or the next ones to come, for the homepage (from the public /api/status)
  type Track = { id: string; direction: string; route: string[]; levels: number[]; validFrom: number | null; validTo: number | null };
  type Group = { dir: string; label: string; state: "now" | "next"; from: number; to: number; tracks: Track[]; next: { from: number; to: number } | null };

  let groups: Group[] = [];
  let loaded: boolean = false;

  const hhmm = (t: number): string => {
    const d = new Date(t * 1000);
    return String(d.getUTCHours()).padStart(2, "0") + String(d.getUTCMinutes()).padStart(2, "0") + "z";
  };
  const day = (t: number): string => new Date(t * 1000).toLocaleDateString("en-GB", { weekday: "short", day: "numeric", month: "short", timeZone: "UTC" });
  const sameDay = (a: number, b: number): boolean => new Date(a * 1000).toUTCString().slice(0, 16) === new Date(b * 1000).toUTCString().slice(0, 16);
  const window_ = (g: { from: number; to: number }): string => hhmm(g.from) + "-" + hhmm(g.to);
  const levels = (l: number[]): string => l.map((x) => Math.round(x / 100)).sort((a, b) => a - b).join(" ");

  function build(tracks: Track[], now: number): Group[] {
    const out: Group[] = [];
    for (const dir of ["west", "east"]) {
      const mine = tracks.filter((t) => t.direction === dir && t.validFrom && t.validTo && t.validTo > now);
      const windows = Array.from(new Set(mine.map((t) => t.validFrom + "-" + t.validTo)))
        .map((k) => k.split("-").map(Number) as [number, number]).sort((a, b) => a[0] - b[0]);
      if (!windows.length) continue;
      const cur = windows.find((w) => w[0] <= now && now < w[1]);
      const pick = cur ?? windows[0];
      const later = windows.find((w) => w[0] > pick[0]);
      out.push({
        dir, label: dir === "west" ? "Westbound" : "Eastbound", state: cur ? "now" : "next", from: pick[0], to: pick[1],
        tracks: mine.filter((t) => t.validFrom === pick[0] && t.validTo === pick[1]).sort((a, b) => a.id.localeCompare(b.id)),
        next: later ? { from: later[0], to: later[1] } : null,
      });
    }
    return out;
  }

  async function load(): Promise<void> {
    const ctl = new AbortController();
    const t = setTimeout(() => ctl.abort(), 5000);
    try {
      const r = await fetch(((import.meta as any).env?.VITE_API_BASE ?? "") + "/api/status", { cache: "no-store", signal: ctl.signal });
      if (!r.ok) throw new Error(String(r.status));
      const d = await r.json();
      groups = build((d.natTracks ?? []) as Track[], d.now as number);
    } catch (e) {
      /* keep what we have; the card hides itself when there is nothing to show */
    } finally { clearTimeout(t); loaded = true; }
  }

  onMount(() => {
    load();
    const t = setInterval(load, 120000);
    return () => clearInterval(t);
  });
</script>

{#if loaded && groups.length}
  <div class="flex flex-col items-center bg-blue-1 px-6 py-10 font-web" id="nat-tracks">
    <div class="text-center text-3xl font-bitmap font-semibold tracking-wider select-none">NAT tracks</div>
    <div class="mt-2 mb-6 text-center text-lg opacity-80">The tracks in force now, or the next to come into force. Times are Zulu.</div>
    <div class="flex flex-row flex-wrap justify-center gap-6 w-full max-w-5xl">
      {#each groups as g}
        <div class="flex-1 min-w-[18rem] border-2 rounded shadow-5xl bg-blue-0 p-4 {g.state === 'now' ? 'border-green-400' : 'border-yellow-300'}">
          <div class="flex flex-row items-center justify-between mb-3">
            <div class="text-xl font-bitmap tracking-wider">{g.label}</div>
            <div class="text-sm px-2 py-0.5 rounded-full border {g.state === 'now' ? 'border-green-400 text-green-300' : 'border-yellow-300 text-yellow-200'}">
              {g.state === "now" ? "In force now" : "Upcoming"}
            </div>
          </div>
          <div class="text-sm opacity-80 mb-3">
            {window_(g)}{#if g.state === "next" || !sameDay(g.from, g.to) || !sameDay(g.from, Date.now() / 1000)}, {day(g.from)}{/if}
          </div>
          <div class="flex flex-col gap-2">
            {#each g.tracks as t}
              <div class="flex flex-row gap-3 items-baseline" title={t.route.join(" ")}>
                <span class="font-bitmap text-lg w-6 shrink-0">{t.id}</span>
                <span class="text-sm"><span class="font-bold">{t.route[0]}</span> &rarr; <span class="font-bold">{t.route[t.route.length - 1]}</span><span class="block opacity-70">FL {levels(t.levels)}</span></span>
              </div>
            {/each}
          </div>
          {#if g.next}
            <div class="mt-3 pt-3 border-t border-blue-10 text-sm opacity-80">Next: {window_(g.next)}, {day(g.next.from)}</div>
          {/if}
        </div>
      {/each}
    </div>
    <div class="mt-4 text-sm opacity-70">Hover over a track to see its full route. Source: <a class="underline" href="https://nattrak.vatsim.net" target="_blank" rel="noopener">natTrak</a>.</div>
  </div>
{/if}
