<script lang="ts">
  import * as st from "../../typescript/fdd/settings";
  import * as sess from "../../typescript/fdd/session";

  const { settingsOpen } = st;
  const { session } = sess;

  type Opt = { key: string; label: string; help: string; choices?: { v: string; t: string }[] };
  const yesNo = [{ v: "1", t: "On" }, { v: "0", t: "Off" }];
  const options: Opt[] = [
    { key: "autotype", label: "Auto-type into EuroScope", help: "The FDD buttons for SELCAL, Contact Me and the natTrak message type the command into EuroScope for you.", choices: yesNo },
    { key: "autoclick", label: "Click the aircraft after Contact Me", help: "After typing .contactme, the plugin also clicks the aircraft so the message is sent.", choices: yesNo },
    { key: "highlight", label: "Circle the aircraft when clicked here", help: "Clicking an aircraft on the FDD circles it in white in EuroScope for 3 seconds.", choices: yesNo },
    { key: "retrack", label: "Re-track aircraft automatically", help: "After a crash, a lost connection or a pilot reconnecting within 5 minutes, track the aircraft again.", choices: yesNo },
    { key: "squawk", label: "Assign squawk 2000 on tracking", help: "Code 2000 is given as soon as you start tracking an aircraft; domestic reassign it later.", choices: yesNo },
    { key: "remotelog", label: "Send problems to the vNAAATS server", help: "Warnings and errors go to the server so faults can be found quickly (never your key).", choices: [{ v: "1", t: "Warnings and errors" }, { v: "2", t: "Everything" }, { v: "0", t: "Off" }] },
  ];

  let values: Record<string, string> = {};
  let loaded: boolean = false;
  let error: string = "";
  let saved: boolean = false;
  let busy: boolean = false;

  async function load(): Promise<void> {
    error = ""; saved = false; loaded = false;
    const r = await st.loadSettings();
    if (typeof r === "string") { error = r; return; }
    values = { autotype: "1", autoclick: "1", highlight: "1", retrack: "1", squawk: "1", remotelog: "1", ...(r.defaults ?? {}), ...r.settings };
    loaded = true;
  }
  $: if ($settingsOpen) load();

  async function save(): Promise<void> {
    busy = true; error = ""; saved = false;
    error = await st.saveSettings(values);
    busy = false;
    saved = error === "";
  }
</script>

{#if $settingsOpen}
  <div class="fixed inset-0 z-40 flex items-center justify-center bg-black bg-opacity-60" on:click|self={() => settingsOpen.set(false)}>
    <div class="w-full max-w-xl max-h-full overflow-y-auto bg-blue-2 border-2 border-blue-10 text-white font-web">
      <div class="flex flex-row justify-between items-center px-3 py-1 bg-blue-3 font-bold">
        <span>Plugin settings (vnaaats.cfg)</span>
        <button class="px-3 py-0.5 text-sm font-bold border-2 border-blue-10 bg-blue-4 hover:bg-blue-10 hover:text-black" on:click={() => settingsOpen.set(false)}>Close</button>
      </div>
      <div class="m-3 p-2 border-2 border-yellow-300 text-yellow-300 text-sm">
        <b>EuroScope must be closed and opened again for any change to take effect.</b>
        Your EuroScope saves the new choices into vnaaats.cfg within about 2 minutes of you pressing Save (it must be running with the plugin loaded).
      </div>
      {#if error}<div class="mx-3 mb-2 text-yellow-300">{error}</div>{/if}
      {#if !$session.loggedIn}
        <div class="m-3 opacity-80">Sign in first.</div>
      {:else if loaded}
        {#each options as o}
          <div class="mx-3 mb-3">
            <div class="flex flex-row justify-between items-center">
              <label class="font-bold" for={"set-" + o.key}>{o.label}</label>
              <select id={"set-" + o.key} class="text-black px-1" bind:value={values[o.key]}>
                {#each o.choices ?? [] as c}<option value={c.v}>{c.t}</option>{/each}
              </select>
            </div>
            <div class="text-xs opacity-80">{o.help}</div>
          </div>
        {/each}
        <div class="px-3 pb-3 flex flex-row items-center gap-3">
          <button class="px-4 py-1 font-bold bg-blue-10 text-black" disabled={busy} on:click={save}>{busy ? "Saving..." : "Save"}</button>
          {#if saved}<span class="text-green-300 text-sm">Saved. Close EuroScope and open it again once your EuroScope has updated vnaaats.cfg.</span>{/if}
        </div>
      {:else if !error}
        <div class="m-3 opacity-80">Loading...</div>
      {/if}
    </div>
  </div>
{/if}
