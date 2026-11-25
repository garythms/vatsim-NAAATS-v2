<script lang="ts">
  import { onMount } from "svelte";
  import * as site from "../typescript/site";
  onMount(() => {
    site.setTitle("Tablet or iPad use");
    document.body.style.opacity = "100%";
  });
</script>

<div class="min-h-screen bg-blue-1 text-white font-web flex flex-col items-center px-4 py-10">
  <div class="max-w-3xl w-full">
    <a href="/" class="underline text-sm opacity-80">&larr; Homepage</a>
    <h1 class="text-4xl font-bold mt-3 mb-1">Tablet or iPad use</h1>
    <div class="opacity-80 mb-6">Put the Flight Data Display on a tablet, an iPad or a second screen while EuroScope stays on your Windows computer. It starts from FDD for Windows.</div>

    <video class="w-full bg-black mb-2" controls preload="metadata" playsinline src="/training/vNAAATS-howto-8-Tablet-iPad.mp4"><track kind="captions" srclang="en" label="English" src="/training/vNAAATS-howto-8-Tablet-iPad.vtt" />Your browser cannot play this video. <a class="underline" rel="external" href="/training/vNAAATS-howto-8-Tablet-iPad.mp4">Download it</a>.</video>
    <p class="text-sm opacity-80 mb-6">About a minute. It uses sample flights and is not a live display.</p>

    <h2 class="text-2xl font-bold mb-2">How it works</h2>
    <ol class="list-decimal ml-6 mb-6 space-y-2 opacity-90">
      <li>Open <a class="underline" href="/windows">FDD for Windows</a>, sign in and choose your position.</li>
      <li>Press <b>Settings</b>, find <b>Tablet or iPad mode</b> and press <b>Show link and QR code</b>.</li>
      <li>On the tablet, scan the code with the camera or type the link into the browser. The display opens signed in as you, with your position.</li>
    </ol>

    <h2 class="text-2xl font-bold mb-2">Good to know</h2>
    <ul class="list-disc ml-6 mb-6 space-y-2 opacity-90">
      <li>The link works <b>once</b> and lasts <b>five minutes</b>. If it expires, or you sign out, make a new one.</li>
      <li>EuroScope and the plugin stay on your Windows computer. Buttons that type into EuroScope still go through your plugin, so EuroScope must stay open there.</li>
      <li>On an iPad, choose Share, then Add to Home Screen, for a full-screen icon.</li>
    </ul>

    <p class="text-sm opacity-80">Not set up yet? Start with <a class="underline" href="/windows">FDD for Windows</a>. More videos: <a class="underline" href="/videos">How To Use - Videos</a>.</p>
  </div>
</div>
