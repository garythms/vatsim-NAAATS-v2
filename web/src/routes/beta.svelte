<script lang="ts">
  import { onMount } from "svelte";
  import * as site from "../typescript/site";

  const repo = "garythms/vatsim-NAAATS-v2";
  let version: string = "";
  let published: string = "";
  let zipUrl: string = "https://github.com/" + repo + "/releases/tag/latest-build";
  let direct: boolean = false;
  let note: string = "";

  onMount(async () => {
    site.setTitle("Beta download");
    document.body.style.opacity = "100%";
    try {
      const v = await fetch("https://raw.githubusercontent.com/" + repo + "/server-and-squawk/pluginversion.txt");
      if (v.ok) version = (await v.text()).trim();
    } catch (e) { /* the page still works without it */ }
    try {
      const r = await fetch("https://api.github.com/repos/" + repo + "/releases/tags/latest-build");
      if (r.ok) {
        const rel = await r.json();
        const a = (rel.assets ?? []).find((x: any) => x.name.endsWith(".zip"));
        if (a) { zipUrl = a.browser_download_url; direct = true; published = new Date(a.updated_at).toUTCString(); }
      }
    } catch (e) { note = "Could not check GitHub just now; the button opens the download page instead."; }
  });
</script>

<div class="min-h-screen bg-blue-1 text-white font-web flex flex-col items-center px-4 py-10">
  <div class="max-w-2xl w-full">
    <a href="/" class="underline text-sm opacity-80">&larr; Homepage</a>
    <h1 class="text-4xl font-bold mt-3 mb-1">vNAAATS V2 plugin - beta download</h1>
    <div class="opacity-80 mb-6">Version 2 &ndash; Modernised by Gary Thomas. For EuroScope on Windows.</div>

    <div class="p-5 mb-6 border-2 border-blue-10 bg-blue-3">
      <div class="text-lg font-bold">Latest beta{version ? " - " + version : ""}</div>
      {#if published}<div class="text-sm opacity-80 mb-3">Built {published}</div>{/if}
      <a href={zipUrl} class="inline-block px-5 py-2 font-bold bg-blue-10 text-black">{direct ? "Download VatsimNAAATS-plugin.zip" : "Open the download page"}</a>
      {#if note}<div class="text-sm text-yellow-300 mt-2">{note}</div>{/if}
      <div class="text-sm opacity-80 mt-3">The zip has the plugin DLL, an example config file and a testers' README. It always holds the newest build.</div>
    </div>

    <h2 class="text-2xl font-bold mb-2">Install in five steps</h2>
    <ol class="list-decimal ml-6 space-y-1 mb-6">
      <li>Close EuroScope and unzip the download.</li>
      <li>Put <b>VatsimNAAATS.dll</b> anywhere you like (your EuroScope Plugins folder is fine).</li>
      <li>Copy <b>vnaaats.example.cfg</b> into the same folder and rename it <b>vnaaats.cfg</b>.</li>
      <li>Open it in Notepad. Set <b>server=https://vnaaats.com</b> and paste the <b>key=</b> you were sent. Keep the key private.</li>
      <li>In EuroScope: Other SET &gt; Plug-ins &gt; Load, and pick the DLL.</li>
    </ol>
    <h2 class="text-2xl font-bold mb-2">Updating an existing install</h2>
    <p class="mb-6 opacity-90">Close EuroScope and replace <b>only VatsimNAAATS.dll</b> with the new one from the zip. <b>Keep your existing vnaaats.cfg</b> - do not copy the example file over it, or you will lose your key. Make sure it is the DLL EuroScope actually loads (Other SET &gt; Plug-ins shows its folder).</p>

    <p class="mb-6 opacity-90">If it will not load, install the Microsoft Visual C++ 2015-2022 Redistributable (x86). The README in the zip has the full steps.</p>

    <h2 class="text-2xl font-bold mb-2">What you need</h2>
    <ul class="list-disc ml-6 space-y-1 mb-6">
      <li>The <b>key</b> for the plugin: ask Gary privately (it is not published here).</li>
      <li>A controller sign-in for the <a href="/fdd" class="underline">Flight Data Display</a> to edit flights on the web.</li>
    </ul>

    <p class="text-sm opacity-80">This is beta software. Please report problems at
      <a class="underline" href="https://github.com/{repo}/issues">github.com/{repo}/issues</a>. Source code:
      <a class="underline" href="https://github.com/{repo}">github.com/{repo}</a>.</p>
  </div>
</div>
