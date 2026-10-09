export const LEFT = 1, RIGHT = 2, LAUNCH = 4;
const SAVE_KEY = 'pinball-zero-web-v1';

export function readBest(storage) {
  try {
    const value = JSON.parse(storage.getItem(SAVE_KEY));
    if (Array.isArray(value) && value.length === 2 && value.every(n => Number.isInteger(n) && n >= 0 && n <= 0xffffffff)) return value;
  } catch { /* Storage can be denied, or an older save can be damaged. */ }
  return [0, 0];
}

export class PinballSession {
  constructor(core, storage = null) {
    this.core = core;
    this.storage = storage;
    this.best = readBest(storage);
    this.storageAvailable = true;
    this.state = 'menu';
    this.table = 0;
    this.held = new Map();
    this.sound = false;
    this.core.pb_init(0);
  }

  select(direction) {
    if (this.state !== 'menu') return;
    this.table = (this.table + direction + 2) % 2;
    this.core.pb_init(this.table);
  }

  enter() {
    if (this.state === 'menu') this.state = 'help';
    else if (this.state === 'help' || this.state === 'over') {
      this.cancel(); this.core.pb_init(this.table); this.state = 'playing';
    } else if (this.state === 'paused') this.state = 'playing';
    else this.pause();
  }

  press(control, source) {
    if (this.held.has(source)) return;
    if (this.state === 'menu') {
      if (control === LEFT || control === RIGHT) this.select(control === LEFT ? -1 : 1);
      else this.enter();
      return;
    }
    if (this.state !== 'playing') {
      if (control === LAUNCH) this.enter();
      return;
    }
    const alreadyHeld = [...this.held.values()].includes(control);
    this.held.set(source, control);
    if (control === LAUNCH && !alreadyHeld && this.core.pb_phase() === 1) this.core.pb_nudge();
    this.applyControls();
  }

  release(source) {
    if (this.held.delete(source)) this.applyControls();
  }

  applyControls() {
    if (this.state === 'playing') this.core.pb_controls([...this.held.values()].reduce((mask, n) => mask | n, 0));
  }

  cancel() {
    this.held.clear();
    this.core.pb_cancel_controls();
  }

  pause() {
    if (this.state !== 'playing') return;
    this.cancel(); this.state = 'paused'; this.saveBest();
  }

  back() {
    if (this.state === 'playing') this.pause();
    else if (this.state === 'paused') this.enter();
    else if (this.state === 'help' || this.state === 'over') this.menu();
  }

  menu() {
    this.saveBest(); this.cancel(); this.state = 'menu'; this.core.pb_init(this.table);
  }

  saveBest() {
    this.best[this.table] = Math.max(this.best[this.table], this.core.pb_score());
    try { this.storage.setItem(SAVE_KEY, JSON.stringify(this.best)); }
    catch { this.storageAvailable = false; }
  }

  step(milliseconds) {
    if (this.state !== 'playing') return 0;
    this.applyControls();
    const events = this.core.pb_advance(Math.min(50, Math.max(0, Math.floor(milliseconds))));
    if (this.core.pb_phase() === 3) { this.saveBest(); this.cancel(); this.state = 'over'; }
    return events;
  }

  pixels() {
    const view = {menu: 0, help: 3, paused: 2, playing: 1, over: 1}[this.state];
    const pointer = this.core.pb_render(view, this.best[this.table], this.sound ? 1 : 0);
    return new Uint8Array(this.core.memory.buffer, pointer, 1024);
  }
}
