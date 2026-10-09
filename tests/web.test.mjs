import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import {PinballSession, LEFT, RIGHT, LAUNCH, readBest} from '../web/session.mjs';

const root = fileURLToPath(new URL('../', import.meta.url));
const module = new WebAssembly.Module(readFileSync(join(root,'web/pinball.wasm')));
function core() { const instance = new WebAssembly.Instance(module,{}).exports; instance._initialize(); return instance; }
function session(storage=null) { const s=new PinballSession(core(),storage);s.enter();s.enter();return s; }

test('WASM has no system or JavaScript imports', () => {
  assert.deepEqual(WebAssembly.Module.imports(module),[]);
  const c=core();c.pb_init(1);assert.equal(c.pb_lives(),3);assert.equal(c.pb_table(),1);
});

test('native and WASM builds render the same menu and follow both table trajectories', () => {
  const temporary=mkdtempSync(join(tmpdir(),'pinball-web-test-'));
  try {
    const exe=join(temporary,'reference');
    execFileSync(process.env.CC || 'cc',['-std=c11','-Wall','-Wextra','-Werror','tests/web_reference.c','pinball_physics.c','pinball_render.c','-lm','-o',exe],{cwd:root});
    const lines=execFileSync(exe,{encoding:'utf8'}).trim().split('\n');
    const c=core();
    for (const line of lines) {
      if (line.startsWith('menu')) {
        const [,table,expected]=line.split(' ');c.pb_init(Number(table));
        const bits=new Uint8Array(c.memory.buffer,c.pb_render(0,1234,0),1024);
        let hash=2166136261;for(const bit of bits) hash=Math.imul(hash^bit,16777619)>>>0;
        assert.equal(hash,Number(expected));continue;
      }
      const [table,frame,phase,score,lives,x,y]=line.split(' ').map(Number);
      c.pb_controls(frame<20?LAUNCH:frame%60<25?LEFT:RIGHT);c.pb_advance(frame%3===0?16:17);
      assert.deepEqual([c.pb_table(),c.pb_phase(),c.pb_score(),c.pb_lives()],[table,phase,score,lives]);
      assert.ok(Math.abs(c.pb_ball_x(0)-x)<.002,`x at table ${table}, frame ${frame}`);
      assert.ok(Math.abs(c.pb_ball_y(0)-y)<.002,`y at table ${table}, frame ${frame}`);
    }
  } finally { rmSync(temporary,{recursive:true,force:true}); }
});

test('hold and release launches; pause cancels a charge without launching', () => {
  const s=session();s.press(LAUNCH,'touch');s.step(50);assert.ok(s.core.pb_charge()>0);
  s.pause();assert.equal(s.core.pb_phase(),0);assert.equal(s.core.pb_charge(),0);
  const before=[s.core.pb_ball_x(0),s.core.pb_ball_y(0)];s.step(1000);
  assert.deepEqual([s.core.pb_ball_x(0),s.core.pb_ball_y(0)],before);
  s.enter();s.release('touch');assert.equal(s.core.pb_phase(),0);
  s.press(LAUNCH,'touch');s.release('touch');assert.equal(s.core.pb_phase(),1);
});

test('two touches and duplicate keyboard sources cannot release each other', () => {
  const s=session();s.press(LEFT,'finger1');s.press(RIGHT,'finger2');s.press(LEFT,'keyboard');
  assert.equal(s.core.pb_inputs(),LEFT|RIGHT);
  s.release('finger1');assert.equal(s.core.pb_inputs(),LEFT|RIGHT);
  s.release('keyboard');assert.equal(s.core.pb_inputs(),RIGHT);
  s.cancel();assert.equal(s.core.pb_inputs(),0);assert.equal(s.held.size,0);
});

test('menu selection and returning from help preserve separate table bests', () => {
  const store={getItem:()=>JSON.stringify([12,34]),setItem:()=>{}};
  const s=new PinballSession(core(),store);s.press(RIGHT,'menu');assert.equal(s.table,1);
  s.enter();assert.equal(s.state,'help');s.back();assert.equal(s.state,'menu');
  assert.deepEqual(s.best,[12,34]);s.enter();s.enter();s.select(-1);assert.equal(s.table,1);
});

test('invalid or denied storage does not stop play', () => {
  for(const value of ['null','{}','[-1,2]','[1,2,3]','["4",5]','[1e99,1]','damaged']) assert.deepEqual(readBest({getItem:()=>value}),[0,0]);
  const denied={getItem(){throw new Error('denied')},setItem(){throw new Error('denied')}};
  const s=session(denied);s.pause();assert.equal(s.storageAvailable,false);s.enter();
  s.press(LAUNCH,'key');s.release('key');assert.equal(s.core.pb_phase(),1);
});

test('background-sized elapsed time stays bounded and pixels expose exactly one screen', () => {
  const a=session(),b=session();
  for(const s of [a,b]) {s.press(LAUNCH,'key');s.release('key');}
  a.step(60000);b.step(50);
  assert.equal(a.core.pb_ball_y(0),b.core.pb_ball_y(0));assert.equal(a.pixels().length,1024);
  assert.ok(a.pixels().some(n=>n>0));
});
