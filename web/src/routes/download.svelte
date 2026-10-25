<script lang="ts">
  import { onMount } from "svelte";
  import * as site from "../typescript/site";

  const repo = "garythms/vatsim-NAAATS-v2";
  let version: string = "";
  let published: string = "";
  let zipUrl: string = "https://github.com/" + repo + "/releases/tag/latest-build";
  let direct: boolean = false;
  let dllUrl: string = "";
  let note: string = "";

  onMount(async () => {
    site.setTitle("Download");
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
        const d = (rel.assets ?? []).find((x: any) => x.name.endsWith(".dll"));
        if (d) dllUrl = d.browser_download_url;
        if (a) { zipUrl = a.browser_download_url; direct = true; published = new Date(a.updated_at).toUTCString(); }
      }
    } catch (e) { note = "Could not check GitHub just now; the button opens the download page instead."; }
  });
</script>

<div class="min-h-screen bg-blue-1 text-white font-web flex flex-col items-center px-4 py-10">
  <div class="max-w-2xl w-full">
    <a href="/" class="underline text-sm opacity-80">&larr; Homepage</a>
    <h1 class="text-4xl font-bold mt-3 mb-1">vNAAATS V2 plugin - download</h1>
    <div class="opacity-80 mb-6">Version 2 &ndash; Modernised by Gary Thomas. For EuroScope on Windows.</div>

    <div class="p-5 mb-6 border-2 border-blue-10 bg-blue-3">
      <div class="text-lg font-bold">Update the plugin{version ? " - " + version : ""}</div>
      {#if published}<div class="text-sm opacity-80 mb-3">Built {published}</div>{/if}
      <a href={dllUrl || zipUrl} class="inline-block px-5 py-2 font-bold bg-blue-10 text-black">{dllUrl ? "Download VatsimNAAATS.dll" : direct ? "Download VatsimNAAATS-plugin.zip" : "Open the download page"}</a>
      {#if note}<div class="text-sm text-yellow-300 mt-2">{note}</div>{/if}
      <div class="text-sm opacity-80 mt-3">Already installed? Close EuroScope and replace <b>only</b> the DLL. Keep your <b>vnaaats.cfg</b>.</div>
    </div>

    <div class="p-5 mb-6 border-2 border-blue-10 bg-blue-3">
      <div class="text-lg font-bold mb-2">Video: how to download and use vNAAATS</div>
      <video class="w-full bg-black" controls preload="metadata" playsinline src="/vNAAATS-intro.mp4">Your browser cannot play this video. <a class="underline" href="/vNAAATS-intro.mp4">Download it</a>.</video>
      <div class="text-sm opacity-80 mt-2">About three minutes, with captions. The flights shown are demo data.</div>
    </div>

    <div class="p-5 mb-6 border-2 border-blue-10 bg-blue-3">
      <div class="text-lg font-bold">User guide (PDF)</div>
      <div class="text-sm opacity-80 mb-3">The install guide and the full user manual in one file. The same text is on the <a href="https://vnaaats.com/guide" class="underline">install guide</a> and <a href="https://vnaaats.com/manual" class="underline">user manual</a> pages.</div>
      <a href="/vNAAATS-user-guide.pdf" class="inline-block px-5 py-2 mr-2 mb-2 font-bold bg-blue-10 text-black">Open the guide (PDF)</a>
      <a href="/vNAAATS-user-guide.pdf" download class="inline-block px-5 py-2 mb-2 border-2 border-blue-10">Download</a>
      <details class="mt-2">
        <summary class="cursor-pointer underline text-sm">Read it on this page</summary>
        <iframe class="w-full mt-3 bg-white" style="height: 70vh;" title="vNAAATS user guide" src="/vNAAATS-user-guide.pdf#view=FitH"></iframe>
      </details>
    </div>

    <p class="mb-4 p-3 border-2 border-blue-10 bg-blue-3">First time installing? Use the <a href="https://vnaaats.com/guide" class="underline font-bold">install guide</a>. It has the full first-time pack (plugin, radar screen file, example config) and the sector file steps.</p>
    <h2 class="text-2xl font-bold mb-2">Updating an existing install</h2>
    <p class="mb-6 opacity-90">Close EuroScope and replace <b>only VatsimNAAATS.dll</b> with the one you just downloaded. <b>Keep your existing vnaaats.cfg</b> - do not copy the example file over it, or you will lose your key. Make sure it is the DLL EuroScope actually loads (Other SET &gt; Plug-ins shows its folder).</p>

    <p class="mb-6 opacity-90">If it will not load, install the Microsoft Visual C++ 2015-2022 Redistributable (x86). The <a href="https://vnaaats.com/guide" class="underline">install guide</a> has the full steps.</p>

    <h2 class="text-2xl font-bold mb-2">What you need</h2>
    <ul class="list-disc ml-6 space-y-1 mb-6">
      <li>Your own <b>key</b> for the plugin, and your sign-in for the <a href="/fdd" class="underline">Flight Data Display</a>: both come from your page on the Gander Oceanic website, <a href="https://ganderoceanic.ca/my/vnaaats" class="underline">ganderoceanic.ca/my/vnaaats</a>. They are for controllers who are <b>certified</b>. Keep your key private.</li>
      <li>The display shows nothing until you have signed in and chosen a position, to protect pilot and controller data.</li>
    </ul>

    <p class="text-sm opacity-80">Please report problems at
      <a class="underline" href="https://github.com/{repo}/issues">github.com/{repo}/issues</a>. Source code:
      <a class="underline" href="https://github.com/{repo}">github.com/{repo}</a>.</p>
  </div>
</div>
