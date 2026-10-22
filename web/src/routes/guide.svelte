<script lang="ts">
  import { onMount } from "svelte";
  import * as site from "../typescript/site";
  onMount(() => { site.setTitle("Install guide"); document.body.style.opacity = "100%"; });
</script>

<div class="min-h-screen bg-blue-1 text-white font-web flex flex-col items-center px-4 py-10">
  <div class="max-w-2xl w-full">
    <a href="https://vnaaats.com/" class="underline text-sm opacity-80">&larr; Homepage</a>
    <h1 class="text-4xl font-bold mt-3 mb-1">vNAAATS V2 - first-time install guide</h1>
    <div class="opacity-80 mb-6">For controllers using EuroScope on Windows. Allow about 15 minutes.</div>

    <h2 class="text-2xl font-bold mb-2">Before you start</h2>
    <ul class="list-disc ml-6 space-y-1 mb-6">
      <li>EuroScope installed and working, and a VATSIM controller rating.</li>
      <li>The official <b>Gander Oceanic sector package from AeroNav</b> (it also keeps your AIRAC up to date).</li>
      <li>The plugin zip, with the <b>key</b> you were given (ask the Gander Oceanic team; do not share it).</li>
    </ul>

    <h2 class="text-2xl font-bold mb-2">1. Get the Gander package</h2>
    <p class="mb-6 opacity-90">Download and run the Gander Oceanic package from <b>AeroNav</b> (aero-nav.com). Open EuroScope with that package once, so you can see the oceanic map, then close EuroScope.</p>

    <h2 class="text-2xl font-bold mb-2">2. Download the plugin</h2>
    <p class="mb-3 opacity-90">Get the newest zip from the <a class="underline" href="https://vnaaats.com/beta">download page</a> (or from the Gander website if you were given a ready-made zip). Unzip it into a folder you will remember, for example <b>Documents\vNAAATS</b>. It holds:</p>
    <ul class="list-disc ml-6 space-y-1 mb-6">
      <li><b>VatsimNAAATS.dll</b> - the plugin.</li>
      <li><b>vnaaats.cfg</b> - your settings and key (or <b>vnaaats.example.cfg</b>: rename it to <b>vnaaats.cfg</b>, open it in Notepad, set <b>server=https://vnaaats.com</b> and paste your <b>key=</b>).</li>
      <li><b>vNAAATS.asr</b> - the radar screen layout.</li>
    </ul>
    <p class="mb-6 opacity-90">Keep the DLL and <b>vnaaats.cfg</b> in the <b>same folder</b>.</p>

    <h2 class="text-2xl font-bold mb-2">3. Load the plugin in EuroScope</h2>
    <ol class="list-decimal ml-6 space-y-1 mb-6">
      <li>Open EuroScope with the Gander package.</li>
      <li>Other SET &gt; Plug-ins &gt; Load, and pick <b>VatsimNAAATS.dll</b>.</li>
      <li><b>Only vNAAATS should be loaded.</b> Remove any other plugin from that list while you work oceanic. (Discord is fine.)</li>
    </ol>

    <h2 class="text-2xl font-bold mb-2">4. Open the vNAAATS screen</h2>
    <ol class="list-decimal ml-6 space-y-1 mb-3">
      <li>Open the file menu and choose <b>New vNAAATS radar screen</b>. You get a blank screen with a blue bar at the top.</li>
      <li>Open the file menu again, choose <b>Open</b>, and pick <b>vNAAATS.asr</b>.</li>
      <li>If EuroScope warns about a different sector file, pick your <b>latest Gander (EXCXO) sector file</b>.</li>
      <li>The oceanic map appears. Choose <b>Save</b> so you never have to do this again.</li>
    </ol>
    <p class="mb-6 opacity-80 text-sm">If the screen stays blank, check that the Gander package is the one loaded and try the steps again in order.</p>

    <h2 class="text-2xl font-bold mb-2">5. Check it works</h2>
    <ul class="list-disc ml-6 space-y-1 mb-6">
      <li>A pop-up shows the plugin version if there is a newer one.</li>
      <li>Open <b>vNAAATS.log</b> (next to the DLL). The first line shows the version.</li>
      <li>Open the <a class="underline" href="https://vnaaats.com/fdd">Flight Data Display</a> in your browser, sign in and pick your position.</li>
    </ul>

    <h2 class="text-2xl font-bold mb-2">Updating</h2>
    <p class="mb-6 opacity-90">Close EuroScope and replace <b>only VatsimNAAATS.dll</b>. <b>Keep your existing vnaaats.cfg</b> and your saved screen. Do not copy the example config over it or you will lose your key.</p>

    <h2 class="text-2xl font-bold mb-2">Optional: let the display type commands for you</h2>
    <p class="mb-6 opacity-90">Add <b>autotype=1</b> on a new line in <b>vnaaats.cfg</b>. The Send buttons for SELCAL, contact me and RCL REQ in the display then type the command into EuroScope for you. Leave it out and use the Copy buttons instead if you prefer.</p>

    <h2 class="text-2xl font-bold mb-2">If something goes wrong</h2>
    <ul class="list-disc ml-6 space-y-2 mb-6">
      <li><b>EuroScope closes when it starts:</b> check <b>server=https://vnaaats.com</b> (with https) in <b>vnaaats.cfg</b>. Install the Microsoft Visual C++ 2015-2022 Redistributable (x86) if the plugin will not load at all.</li>
      <li><b>Wrong version in the log:</b> EuroScope is loading a different copy of the DLL. Other SET &gt; Plug-ins shows which folder it is using.</li>
      <li><b>The map is wrong or not oceanic:</b> you have another sector file loaded. Use the Gander package from AeroNav.</li>
      <li><b>No flights appear or changes do not save:</b> the key is missing or wrong in <b>vnaaats.cfg</b>. Ask for the current key.</li>
      <li><b>Still stuck:</b> send <b>vNAAATS.log</b> to the Gander Oceanic team.</li>
    </ul>

    <p class="text-sm opacity-80">Independent community project, not run or endorsed by VATSIM.
      <a class="underline" href="https://vnaaats.com/privacy/">Privacy and data handling</a>.</p>
  </div>
</div>
