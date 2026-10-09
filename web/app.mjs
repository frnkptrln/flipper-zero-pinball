import {PinballSession, LEFT, RIGHT, LAUNCH} from './session.mjs';

const $ = id => document.getElementById(id);
const buttons = [...document.querySelectorAll('[data-control]')];
export let session = null;
let last = null, fraction = 0, audio = null;
const keys = {ArrowLeft: LEFT, KeyA: LEFT, ArrowRight: RIGHT, KeyD: RIGHT, Space: LAUNCH};

function sounds(events) {
  if (!audio || audio.state !== 'running' || !session.sound || !events) return;
  const tones = [[1,180],[2,620],[4,110],[8,820],[16,1100],[32,90],[64,55]];
  for (const [bit, frequency] of tones) {
    if (!(events & bit)) continue;
    const tone = audio.createOscillator(), gain = audio.createGain();
    tone.type = bit === 4 ? 'triangle' : 'sine';
    tone.frequency.setValueAtTime(frequency, audio.currentTime);
    tone.frequency.exponentialRampToValueAtTime(frequency * .72, audio.currentTime + .08);
    gain.gain.setValueAtTime(.035, audio.currentTime);
    gain.gain.exponentialRampToValueAtTime(.001, audio.currentTime + .1);
    tone.connect(gain); gain.connect(audio.destination);
    tone.start(); tone.stop(audio.currentTime + .11);
    tone.onended = () => { tone.disconnect(); gain.disconnect(); };
  }
}

function update() {
  const s = session, phase = s.core.pb_phase();
  const messages = {
    menu: 'Left / right selects a table. Play opens the rules.',
    help: 'Light A, B and C for multiball. Press Start when ready.',
    paused: 'Paused. Your ball will wait.',
    over: 'Three lives spent. One more game?',
    playing: phase === 0 ? 'Hold the middle button or Space, then release to launch.'
      : s.core.pb_tilted() ? 'Tilt — wait for the next ball.' : phase === 2 ? 'Ball lost. Next ball coming…' : 'Keep it moving. Hold both flippers when you need them.',
  };
  const message = messages[s.state];
  if ($('status').textContent !== message) $('status').textContent = message;
  $('table-name').textContent = ['ORBIT', 'REACTOR'][s.table];
  $('score').textContent = s.core.pb_score(); $('best').textContent = s.best[s.table]; $('lives').textContent = s.core.pb_lives();
  $('action').textContent = {menu:'Choose table',help:'Start game',playing:'Pause',paused:'Resume',over:'Play again'}[s.state];
  $('menu').hidden = !['help','paused','over'].includes(s.state);
  $('launch-label').textContent = {menu:'PLAY',help:'START',paused:'RESUME',over:'AGAIN',playing:phase===0?'LAUNCH':'NUDGE'}[s.state];
  $('storage-note').hidden = s.storageAvailable;
  document.body.dataset.state = s.state;
  for (const button of buttons) button.classList.toggle('held', [...s.held.values()].includes(Number(button.dataset.control)));
}

function interrupted() {
  if (!session) return;
  session.pause(); session.cancel(); last = null; fraction = 0; update();
}

async function start() {
  const response = await fetch(new URL('./pinball.wasm', import.meta.url));
  if (!response.ok) throw new Error(`Module request failed (${response.status})`);
  const {instance} = await WebAssembly.instantiate(await response.arrayBuffer(), {});
  instance.exports._initialize?.();
  let storage = null;
  try { storage = window.localStorage; } catch { /* Play without persistence. */ }
  session = new PinballSession(instance.exports, storage);
  if (!storage) session.storageAvailable = false;
  const context = $('screen').getContext('2d', {alpha:false});
  if (!context) throw new Error('Canvas is unavailable');
  const image = context.createImageData(64,128);
  function frame(now) {
    if (last !== null && session.state === 'playing') {
      fraction += Math.min(50, Math.max(0, now-last));
      const elapsed = Math.floor(fraction); fraction -= elapsed;
      sounds(session.step(elapsed));
    }
    last = now;
    const pixels = session.pixels();
    for (let i=0;i<8192;i++) {
      const on = pixels[i>>3] & (1 << (i&7)), offset=i*4;
      image.data[offset]=on?23:197; image.data[offset+1]=on?36:210;
      image.data[offset+2]=on?30:163; image.data[offset+3]=255;
    }
    context.putImageData(image,0,0); update(); requestAnimationFrame(frame);
  }
  for (const button of buttons) {
    button.disabled = false;
    button.addEventListener('pointerdown', event => {
      if (event.button !== 0) return;
      event.preventDefault(); button.setPointerCapture(event.pointerId);
      session.press(Number(button.dataset.control), `pointer:${event.pointerId}`); update();
    });
    button.addEventListener('pointerup', event => { session.release(`pointer:${event.pointerId}`); update(); });
    button.addEventListener('pointercancel', interrupted);
    button.addEventListener('lostpointercapture', event => {
      if (session.held.has(`pointer:${event.pointerId}`)) interrupted();
    });
    button.addEventListener('keydown', event => {
      if (!['Space','Enter'].includes(event.code)) return;
      event.preventDefault(); event.stopPropagation();
      if (!event.repeat) session.press(Number(button.dataset.control), `button:${button.dataset.control}:${event.code}`);
      update();
    });
    button.addEventListener('keyup', event => {
      if (!['Space','Enter'].includes(event.code)) return;
      event.preventDefault(); event.stopPropagation();
      session.release(`button:${button.dataset.control}:${event.code}`); update();
    });
    button.addEventListener('blur', () => {
      if ([...session.held.keys()].some(source => source.startsWith(`button:${button.dataset.control}:`))) interrupted();
    });
    // Keyboard/assistive activation produces a click without a pointer event.
    button.addEventListener('click', event => {
      if (event.detail !== 0) return;
      const source = `activate:${button.dataset.control}`;
      session.press(Number(button.dataset.control), source); session.release(source); update();
    });
  }
  document.addEventListener('keydown', event => {
    if (event.repeat || event.altKey || event.ctrlKey || event.metaKey) return;
    const interactive = event.target.closest?.('button,a,input,select,textarea');
    if (interactive && (event.code === 'Enter' || event.code === 'Space')) return;
    if (keys[event.code]) { event.preventDefault(); session.press(keys[event.code], `key:${event.code}`); update(); }
    else if (event.code === 'Enter') { event.preventDefault(); session.enter(); last=null; fraction=0; update(); }
    else if (event.code === 'Escape' || event.code === 'KeyP') { event.preventDefault(); session.back(); last=null; fraction=0; update(); }
  });
  document.addEventListener('keyup', event => { if (keys[event.code]) { session.release(`key:${event.code}`); update(); } });
  window.addEventListener('blur', interrupted);
  document.addEventListener('visibilitychange', () => { if (document.hidden) interrupted(); });
  $('action').disabled = false;
  $('action').onclick = () => {
    session.enter(); last=null; fraction=0; update();
    if (session.state === 'playing') $('screen').focus({preventScroll:true});
  };
  $('menu').onclick = () => { session.menu(); last=null; fraction=0; update(); };
  $('sound').disabled = false;
  $('sound').onclick = async () => {
    try {
      if (!audio) { const Audio = window.AudioContext || window.webkitAudioContext; audio = new Audio(); }
      await audio.resume(); session.sound = !session.sound;
      if (!session.sound) await audio.suspend();
      $('sound').textContent = session.sound ? 'Sound on' : 'Sound off';
      $('sound').setAttribute('aria-pressed', String(session.sound));
    } catch { $('sound').textContent = 'Sound unavailable'; $('sound').disabled = true; }
  };
  update(); requestAnimationFrame(frame);
}

start().catch(error => {
  $('status').textContent = 'The table could not load. Serve the web folder over HTTP and check that pinball.wasm is present.';
  console.error(error);
});
