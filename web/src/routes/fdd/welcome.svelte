<script lang="ts">
  import { onMount } from "svelte";
  import * as app from "../../typescript/fdd/app";

  // "Welcome to vNAAATS": a short opening animation with a little tune, played once each time the app is opened.
  let show: boolean = false;
  let out: boolean = false;
  let ctx: AudioContext | null = null;
  const KEY = "vnaaats.welcomeSeen";

  function note(freq: number, at: number, len: number, vol: number, type: OscillatorType = "sine"): void {
    if (!ctx) return;
    const o = ctx.createOscillator(), g = ctx.createGain();
    o.type = type; o.frequency.value = freq;
    const t = ctx.currentTime + at;
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(vol, t + 0.03);
    g.gain.exponentialRampToValueAtTime(0.0001, t + len);
    o.connect(g); g.connect(ctx.destination);
    o.start(t); o.stop(t + len + 0.05);
  }
  function whoosh(at: number, len: number): void {
    if (!ctx) return;
    const n = Math.floor(ctx.sampleRate * len), buf = ctx.createBuffer(1, n, ctx.sampleRate), d = buf.getChannelData(0);
    for (let i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * Math.sin(Math.PI * i / n);
    const s = ctx.createBufferSource(), f = ctx.createBiquadFilter(), g = ctx.createGain();
    s.buffer = buf; f.type = "bandpass"; f.Q.value = 1.2;
    const t = ctx.currentTime + at;
    f.frequency.setValueAtTime(300, t); f.frequency.exponentialRampToValueAtTime(3000, t + len);
    g.gain.value = 0.12;
    s.connect(f); f.connect(g); g.connect(ctx.destination); s.start(t);
  }
  function tune(): void {
    try {
      ctx = new (window.AudioContext || (window as any).webkitAudioContext)();
      void ctx.resume();
      note(65.4, 0, 3.6, 0.10, "triangle");            // low swell under everything
      whoosh(0.9, 1.1);                                 // the plane crossing the line
      [523.25, 659.25, 783.99, 1046.5].forEach((f, i) => note(f, 1.6 + i * 0.16, 1.6, 0.13));   // rising C major chime
      note(1318.5, 2.35, 1.4, 0.07);
      note(1567.98, 2.5, 1.3, 0.05);
    } catch (e) { /* no sound is fine */ }
  }

  function finish(): void {
    if (out) return;
    out = true;
    setTimeout(() => { show = false; app.finishWelcome(); }, 600);
  }
  onMount(() => {
    let seen = false;
    try { seen = sessionStorage.getItem(KEY) === "1"; sessionStorage.setItem(KEY, "1"); } catch (e) { /* play it */ }
    if (!app.inApp() || seen) { app.finishWelcome(); return; }
    show = true;
    tune();
    setTimeout(finish, 4600);
  });
</script>

{#if show}
  <!-- svelte-ignore a11y-click-events-have-key-events -->
  <div class="wl-root fixed inset-0 z-[80] flex flex-col items-center justify-center select-none cursor-pointer" class:wl-out={out} on:click={finish}>
    <div class="wl-title">
      <div class="wl-word wl-a">vNAAATS</div>
      <div class="wl-word wl-b" aria-hidden="true">vNAAATS</div>
      <div class="wl-word wl-c" aria-hidden="true">vNAAATS</div>
    </div>
    <div class="wl-lane">
      <div class="wl-line"></div>
      <svg class="wl-plane" viewBox="0 0 24 24" width="30" height="30"><path fill="currentColor" d="M21 16v-2l-8-5V3.5a1.5 1.5 0 0 0-3 0V9l-8 5v2l8-2.5V19l-2 1.5V22l3.5-1 3.5 1v-1.5L13 19v-5.5z" transform="rotate(90 12 12)" /></svg>
    </div>
    <div class="wl-hello">Welcome to vNAAATS</div>
    <div class="wl-sub">The Virtual North Atlantic Advanced Air Traffic System</div>
  </div>
{/if}

<style>
  .wl-root { background: radial-gradient(ellipse at center, #12294a 0%, #0b1b30 60%, #050d19 100%); color: #fff; transition: opacity 0.6s ease; font-family: sans-serif; }
  .wl-root.wl-out { opacity: 0; }
  .wl-title { position: relative; font-size: min(14vw, 7.5rem); font-weight: 800; letter-spacing: 0.04em; line-height: 1; }
  .wl-word { animation: wl-reveal 1.1s 0.2s both ease-out; }
  .wl-b, .wl-c { position: absolute; inset: 0; mix-blend-mode: screen; opacity: 0; }
  .wl-b { color: #4fd1ff; animation: wl-glitch-b 0.9s 0.25s both steps(1); }
  .wl-c { color: #ff4f7a; animation: wl-glitch-c 0.9s 0.25s both steps(1); }
  .wl-lane { position: relative; width: min(70vw, 34rem); height: 34px; margin-top: 0.6rem; }
  .wl-line { position: absolute; left: 0; top: 16px; height: 2px; background: #fff; width: 0; animation: wl-draw 1.3s 0.9s forwards ease-in-out; }
  .wl-plane { position: absolute; left: 0; top: 0; color: #fff; opacity: 0; transform: translateX(0); animation: wl-fly 1.3s 0.9s forwards ease-in-out; }
  .wl-hello { margin-top: 1.4rem; font-size: 1.9rem; letter-spacing: 0.12em; opacity: 0; animation: wl-fade 0.9s 2.2s forwards ease-out; }
  .wl-sub { margin-top: 0.5rem; font-size: 0.95rem; opacity: 0; letter-spacing: 0.06em; animation: wl-fade2 0.9s 2.7s forwards ease-out; }
  @keyframes wl-reveal { 0% { clip-path: inset(0 100% 0 0); opacity: 0; } 100% { clip-path: inset(0 0 0 0); opacity: 1; } }
  @keyframes wl-glitch-b { 0% { opacity: 1; transform: translate(-6px, 2px); clip-path: inset(10% 0 60% 0); } 25% { opacity: 1; transform: translate(5px, -3px); clip-path: inset(55% 0 15% 0); } 50% { opacity: 1; transform: translate(-3px, 0); clip-path: inset(30% 0 40% 0); } 75% { opacity: 1; transform: translate(4px, 2px); clip-path: inset(70% 0 5% 0); } 100% { opacity: 0; } }
  @keyframes wl-glitch-c { 0% { opacity: 1; transform: translate(6px, -2px); clip-path: inset(60% 0 10% 0); } 25% { opacity: 1; transform: translate(-5px, 3px); clip-path: inset(5% 0 70% 0); } 50% { opacity: 1; transform: translate(3px, 0); clip-path: inset(45% 0 30% 0); } 75% { opacity: 1; transform: translate(-4px, -2px); clip-path: inset(20% 0 55% 0); } 100% { opacity: 0; } }
  @keyframes wl-draw { to { width: 100%; } }
  @keyframes wl-fly { 0% { opacity: 0; transform: translateX(0); } 10% { opacity: 1; } 90% { opacity: 1; } 100% { opacity: 0; transform: translateX(calc(min(70vw, 34rem) - 30px)); } }
  @keyframes wl-fade { from { opacity: 0; transform: translateY(8px); } to { opacity: 1; transform: none; } }
  @keyframes wl-fade2 { from { opacity: 0; } to { opacity: 0.8; } }
</style>
