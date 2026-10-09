const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

const web = path.resolve(__dirname,'../web');
const output = path.resolve(__dirname,'../build/browser-tests');
const types = {'.html':'text/html','.mjs':'text/javascript','.css':'text/css','.wasm':'application/wasm','.json':'application/json'};
const server = http.createServer((request,response) => {
  const file = path.resolve(web,'.' + new URL(request.url,'http://local').pathname.replace(/\/$/,'/index.html'));
  if (!file.startsWith(web+path.sep) || !fs.existsSync(file)) {response.writeHead(404);response.end();return;}
  response.writeHead(200,{'Content-Type':types[path.extname(file)]||'application/octet-stream'});
  fs.createReadStream(file).pipe(response);
});
const state = page => page.evaluate(async()=>{const {session:s}=await import('./app.mjs');return {state:s.state,phase:s.core.pb_phase(),inputs:s.core.pb_inputs(),charge:s.core.pb_charge(),x:s.core.pb_ball_x(0),y:s.core.pb_ball_y(0),table:s.table};});

(async()=>{
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const base=`http://127.0.0.1:${server.address().port}`;
  const browser=await chromium.launch(process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE ? {
    executablePath:process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE,
    args:['--no-sandbox','--disable-dev-shm-usage','--disable-gpu'],
  } : {});
  const errors=[];
  async function open(context) {
    const page=await context.newPage();page.on('pageerror',error=>errors.push(error.message));
    await page.goto(base);await page.waitForFunction(()=>document.body.dataset.state==='menu');return page;
  }
  async function start(page) {await page.locator('#action').click();await page.locator('#action').click();assert.equal((await state(page)).phase,0);}
  try {
    fs.mkdirSync(output,{recursive:true});
    const desktop=await browser.newContext({viewport:{width:1200,height:1000}});
    const page=await open(desktop);
    await page.screenshot({path:path.join(output,'desktop.png'),fullPage:true});
    await page.keyboard.press('ArrowRight');assert.equal((await state(page)).table,1);
    await start(page);
    await page.keyboard.down('Space');await page.waitForFunction(async()=>{const {session:s}=await import('./app.mjs');return s.core.pb_charge()>.1;});
    await page.keyboard.down('ArrowLeft');await page.keyboard.down('ArrowRight');await page.keyboard.up('Space');
    assert.equal((await state(page)).phase,1);assert.equal((await state(page)).inputs,3);
    await page.keyboard.up('ArrowLeft');assert.equal((await state(page)).inputs,2);await page.keyboard.up('ArrowRight');
    await page.keyboard.press('KeyP');assert.equal((await state(page)).state,'paused');
    const frozen=await state(page);await page.waitForTimeout(180);assert.deepEqual(await state(page),frozen);
    await page.locator('#action').click();await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
    assert.equal((await state(page)).state,'paused');
    await page.locator('#sound').click();assert.equal(await page.locator('#sound').getAttribute('aria-pressed'),'true');
    await page.locator('#sound').click();assert.equal(await page.locator('#sound').getAttribute('aria-pressed'),'false');
    await desktop.close();

    const mobile=await browser.newContext({viewport:{width:390,height:844},isMobile:true,hasTouch:true,deviceScaleFactor:1});
    const phone=await open(mobile);await start(phone);
    assert.equal(await phone.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
    const cdp=await mobile.newCDPSession(phone);
    async function point(control,id) {const box=await phone.locator(`[data-control="${control}"]`).boundingBox();return {x:box.x+box.width/2,y:box.y+box.height/2,id};}
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[await point(1,1),await point(2,2)]});
    assert.equal((await state(phone)).inputs,3);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});assert.equal((await state(phone)).inputs,0);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[await point(4,3)]});
    await phone.waitForFunction(async()=>{const {session:s}=await import('./app.mjs');return s.core.pb_charge()>.1;});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchCancel',touchPoints:[]});
    assert.equal((await state(phone)).state,'paused');assert.equal((await state(phone)).phase,0);
    assert.equal((await state(phone)).charge,0);
    await phone.locator('#action').click();assert.equal((await state(phone)).phase,0);
    await phone.screenshot({path:path.join(output,'mobile.png'),fullPage:true});
    await mobile.close();

    const blocked=await browser.newContext();
    await blocked.addInitScript(()=>Object.defineProperty(window,'localStorage',{get(){throw new DOMException('denied','SecurityError');}}));
    const denied=await open(blocked);await start(denied);
    assert.equal(await denied.locator('#storage-note').isVisible(),true);
    await denied.keyboard.press('Space');assert.equal((await state(denied)).phase,1);
    await blocked.close();
    assert.deepEqual(errors,[]);
    console.log(JSON.stringify({browser:browser.version(),checks:['desktop keyboard and launch','both flippers','pause freeze','blur pause','audio toggle','mobile viewport','simultaneous touch','cancelled charge','denied storage','no page errors'],screenshots:output},null,2));
  } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(()=>server.close());
