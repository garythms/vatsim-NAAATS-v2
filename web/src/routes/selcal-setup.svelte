<script lang="ts">
  import { onMount } from "svelte";
  import * as site from "../typescript/site";
  onMount(() => {
    site.setTitle("SELCAL tones on your frequency");
    document.body.style.opacity = "100%";
  });
</script>

<div class="min-h-screen bg-blue-1 text-white font-web flex flex-col items-center px-4 py-10">
  <div class="max-w-3xl w-full">
    <a href="/" class="underline text-sm opacity-80">&larr; Homepage</a>
    <h1 class="text-4xl font-bold mt-3 mb-1">SELCAL tones on your frequency</h1>
    <div class="opacity-80 mb-6">Let the pilot hear the SELCAL tones on your frequency when you press Send in the Flight Data Display. It is optional, it is set up once in FDD for Windows, and it uses a free virtual audio cable.</div>

    <video class="w-full bg-black mb-2" controls preload="metadata" playsinline src="/training/vNAAATS-howto-9-SELCAL-tones.mp4">Your browser cannot play this video. <a class="underline" rel="external" href="/training/vNAAATS-howto-9-SELCAL-tones.mp4">Download it</a>.</video>
    <p class="text-sm opacity-80 mb-6">About two minutes. It includes the sound the pilot hears.</p>

    <h2 class="text-2xl font-bold mb-2">The three settings</h2>
    <p class="opacity-90 mb-2">In the SELCAL tab of any aircraft you choose who hears the tones when you press Send:</p>
    <ul class="list-disc ml-6 mb-6 space-y-1 opacity-90">
      <li><b>Controller only hears</b>: the tones play on your computer only. Works straight away, nothing to set up.</li>
      <li><b>Controller and pilots hear</b>: the tones also go out on your frequency through TrackAudio. Needs the set-up below.</li>
      <li><b>Off</b>: no sound.</li>
    </ul>
    <p class="opacity-90 mb-6">A code that has no tones of its own (a repeated letter, or I, N, O) plays a random SELCAL sound instead of staying silent. The <b>Hear the tones (only you)</b> button tests the sound without sending anything.</p>

    <h2 class="text-2xl font-bold mb-2">Set-up, once</h2>
    <p class="opacity-90 mb-3">TrackAudio cannot play a sound by itself, so vNAAATS plays the tones into a virtual audio cable that TrackAudio uses as its microphone, and presses TrackAudio's push-to-talk for you while they play. You need <b>FDD for Windows</b>, <b>TrackAudio</b> and the free <b>VB-Cable</b>.</p>
    <ol class="list-decimal ml-6 mb-6 space-y-3 opacity-90">
      <li><b>Get VB-Cable.</b> Open <b>FDD for Windows setup</b> and press <b>Get VB-Cable</b>. It opens the official download page (vb-audio.com). Download it and run the installer as administrator. <b>Restart Windows</b>: the restart is needed.</li>
      <li><b>Set it up.</b> Open FDD for Windows again. The setup screen finds the cable. Press <b>Set it up</b>. Windows may ask once to use the microphone: that is only so the device names can be read. Nothing is recorded.</li>
      <li><b>Open Sound settings.</b> Press <b>Open Sound settings</b>. On the <b>Recording</b> tab, right-click your headset microphone (the one marked Default Device) and choose <b>Properties</b>.
        <img class="mt-2 border border-blue-6 max-w-xs" src="/selcal/sound-recording.png" alt="The Windows Sound window on the Recording tab, with the headset microphone and CABLE Output listed" /></li>
      <li><b>Listen to this device.</b> On the <b>Listen</b> tab tick <b>Listen to this device</b>. Under "Playback through this device" choose <b>CABLE Input (VB-Audio Virtual Cable)</b>. Press <b>OK</b>.
        <img class="mt-2 border border-blue-6 max-w-xs" src="/selcal/sound-listen.png" alt="The Microphone Properties window on the Listen tab with Listen to this device ticked and CABLE Input chosen" /></li>
      <li><b>TrackAudio.</b> In TrackAudio's settings set its microphone to <b>CABLE Output</b>, and make sure <b>TX</b> is on for your frequency.</li>
      <li><b>Choose the setting.</b> In the SELCAL tab of any aircraft choose <b>Controller and pilots hear</b>, then press <b>Send</b>.</li>
    </ol>

    <h2 class="text-2xl font-bold mb-2">Good to know</h2>
    <ul class="list-disc ml-6 mb-6 space-y-2 opacity-90">
      <li>Your own voice still goes out as normal: "Listen to this device" passes your headset microphone into the cable, so TrackAudio hears you and the tones.</li>
      <li>The set-up is only in FDD for Windows setup, because installing VB-Cable needs a Windows restart. After that you can switch between the three settings at any time from the SELCAL tab.</li>
      <li>To take it away again, use <b>Remove this set-up</b> in FDD for Windows setup.</li>
      <li>VB-Cable is a separate programme by VB-Audio. It is free for personal use and is not part of vNAAATS.</li>
    </ul>

    <p class="text-sm opacity-80">Not set up yet? Start with <a class="underline" href="/windows">FDD for Windows</a>. More videos: <a class="underline" href="/videos">How To Use - Videos</a>.</p>
  </div>
</div>
