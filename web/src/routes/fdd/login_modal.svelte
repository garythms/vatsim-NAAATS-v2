<script lang="ts">
  import { onMount } from "svelte";
  import * as sess from "../../typescript/fdd/session";

  const { session } = sess;
  let name: string = "";
  let password: string = "";
  let custom: string = "";
  let error: string = "";
  let busy: boolean = false;

  onMount(() => {
    sess.refreshSession();
    // Ask again now and then, so a session that has run out brings the sign-in screen back
    const t = setInterval(sess.refreshSession, 30000);
    return () => clearInterval(t);
  });

  // The flight data is only sent to a signed-in controller with a position, so until then the whole page is covered
  $: show = $session.editing && (!$session.loggedIn || $session.position === "");

  async function doPassword(): Promise<void> {
    busy = true; error = "";
    error = await sess.loginPassword(name, password);
    password = "";
    busy = false;
  }
  // Before the first time a position is used in this browser, explain the one-off natTrak sign-in.
  let askNat: string = "";                       // the position waiting for the answer
  function natDone(): boolean {
    try { return localStorage.getItem("vnaaats.nattrakSignedIn") === "1"; } catch (e) { return false; }
  }
  async function pick(p: string): Promise<void> {
    if (!natDone()) { askNat = p; return; }
    error = await sess.choosePosition(p);
  }
  async function natYes(): Promise<void> {
    const p = askNat;
    window.open("https://nattrak.vatsim.net/", "_blank");        // the controller signs in there by hand
    try { localStorage.setItem("vnaaats.nattrakSignedIn", "1"); } catch (e) { /* asked again next time */ }
    askNat = "";
    error = await sess.choosePosition(p);
  }
  async function natLater(already: boolean): Promise<void> {
    const p = askNat;
    if (already) { try { localStorage.setItem("vnaaats.nattrakSignedIn", "1"); } catch (e) { /* ignore */ } }
    askNat = "";
    error = await sess.choosePosition(p);
  }
</script>

{#if show}
  <div class="fixed inset-0 z-50 flex items-center justify-center" style="background: #0b1b30;">
    <div class="w-96 max-w-full bg-blue-3 border-2 border-l-blue-10 border-t-blue-10 border-r-blue-0 border-b-blue-0 text-white font-regular p-4">
      <div class="text-xl font-bold mb-1">vNAAATS Flight Data Display</div>
      {#if !$session.loggedIn}
        <div class="mb-3 text-sm opacity-80">Sign in with your controller account to use the display.</div>
        {#if $session.vatsim}
          <button class="w-full mb-3 py-2 font-bold bg-blue-10 text-black border-2 border-l-blue-0 border-t-blue-0 border-r-blue-8 border-b-blue-8" on:click={sess.vatsimLogin}>
            Sign in with VATSIM
          </button>
        {/if}
        {#if $session.password}
          <form on:submit|preventDefault={doPassword}>
            <div class="text-sm mb-1">{$session.vatsim ? "or use your password" : "Your name or CID, and your password (from your myCZQO vNAAATS key page)"}</div>
            <input class="w-full mb-2 px-2 py-1 text-black" placeholder="Name or CID" bind:value={name} maxlength="40" autocomplete="name" />
            <input class="w-full mb-2 px-2 py-1 text-black" type="password" placeholder="Password" bind:value={password} autocomplete="current-password" />
            <button class="w-full py-2 font-bold bg-blue-10 text-black" disabled={busy || !name || !password}>Sign in</button>
          </form>
        {/if}
      {:else if askNat}
        <div class="mb-2 font-bold">natTrak sign-in</div>
        <div class="mb-3 text-sm">To control natTrak from the FDD you need to sign in <b>once</b> to your natTrak account through VATSIM. Are you ready to proceed? This opens natTrak in a new tab, where you press <b>Sign in</b> yourself.</div>
        <button class="w-full mb-2 py-2 font-bold bg-blue-10 text-black" on:click={natYes}>Yes, open natTrak</button>
        <button class="w-full mb-2 py-1 text-sm border border-blue-6 bg-blue-4" on:click={() => natLater(true)}>I am already signed in to natTrak</button>
        <button class="w-full py-1 text-sm underline opacity-80" on:click={() => natLater(false)}>Not now (ask me next time)</button>
      {:else}
        <div class="mb-3 text-sm opacity-80">Hello {$session.name}. Which position are you working?</div>
        <div class="grid grid-cols-2 gap-2 mb-3">
          {#each $session.positions as p}
            <button class="py-2 font-bold bg-blue-10 text-black" on:click={() => pick(p)}>{p}</button>
          {/each}
        </div>
        <form class="flex gap-2" on:submit|preventDefault={() => custom && pick(custom)}>
          <input class="flex-1 px-2 py-1 text-black uppercase" placeholder="Other position e.g. CZQO_E_CTR" bind:value={custom} maxlength="16" />
          <button class="px-3 font-bold bg-blue-10 text-black" disabled={!custom}>Use</button>
        </form>
        <button class="mt-3 text-sm underline opacity-80" on:click={sess.logout}>Sign out</button>
      {/if}
      {#if error}<div class="mt-2 text-sm text-yellow-300">{error}</div>{/if}
    </div>
  </div>
{/if}
