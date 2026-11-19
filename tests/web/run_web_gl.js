#!/usr/bin/env node
// run_web_gl.js - the zen adapter's OpenGL renderer in a real browser (WebGL 2): tools/check.sh, stage web.
//
//   node tests/web/run_web_gl.js <build dir with igui_web_probe.html> [--chrome=PATH] [--presets=dark,light] [--keep]
//
// The page is examples/web_probe/main.cpp built with emcmake (-DIGUI_BUILD_WEB_PROBE=ON; shell file tests/web/shell.html): a window of
// widgets with colours that are known, read back from the framebuffer by the page and published as window.igProbe. A headless Chrome (nothing is
// shown; its profile, HOME and XDG folders are in a temporary folder that is removed; WebGL runs on SwiftShader, in software) is driven
// through the DevTools protocol. For each preset (dark and light) it checks
//   (a) the console has no WebGL error (INVALID_ENUM, INVALID_OPERATION, a shader that did not compile...), no uncaught exception, and
//       the program drew with OpenGL and not the software rasteriser;
//   (b) the picture is not single-channel: the pixels at the centre of widgets with known colours (three panels, one for each channel,
//       the theme's panel, a label's text) have those colours within a tolerance. A renderer that loses two channels (the red page of the
//       texture swizzle that WebGL 2 does not have) fails here.
// Exit code 0 when everything held, 1 when something did not, 77 when there is no Chrome or no WebSocket in this Node.
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const os = require('os');
const cp = require('child_process');

const args = process.argv.slice(2);
const dir = args.find(a => !a.startsWith('--'));
const chromeArg = (args.find(a => a.startsWith('--chrome=')) || '').slice(9);
const presets = ((args.find(a => a.startsWith('--presets=')) || '--presets=dark,light').slice(10)).split(',');
const keep = args.includes('--keep');
if (!dir || !fs.existsSync(path.join(dir, 'igui_web_probe.html'))) {
  console.error('usage: node run_web_gl.js <dir with igui_web_probe.html> [--chrome=PATH] [--presets=dark,light]');
  process.exit(2);
}
if (typeof WebSocket === 'undefined') { console.log('SKIP: this Node has no WebSocket'); process.exit(77); }
const chrome = chromeArg || ['google-chrome', 'chromium', 'chromium-browser'].map(n => {
  try { return cp.execFileSync('sh', ['-c', 'command -v ' + n]).toString().trim(); } catch (e) { return ''; }
}).find(Boolean);
if (!chrome) { console.log('SKIP: no Chrome or Chromium'); process.exit(77); }

let pass = 0, fail = 0;
const check = (name, ok, detail) => {
  if (ok) { pass++; console.log('  ok    ' + name); }
  else { fail++; console.log('  FAIL  ' + name + (detail !== undefined ? '   ' + (typeof detail === 'string' ? detail : JSON.stringify(detail)) : '')); }
};
const sleep = ms => new Promise(r => setTimeout(r, ms));

const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm' };
const server = http.createServer((req, res) => {
  const name = decodeURIComponent(req.url.split('?')[0]).replace(/^\/+/, '') || 'igui_web_probe.html';
  fs.readFile(path.join(dir, path.basename(name)), (err, data) => {
    if (err) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'content-type': types[path.extname(name)] || 'application/octet-stream' });
    res.end(data);
  });
});

let tmp, chromeProc, ws, nextId = 1;
const pending = new Map();
const events = []; // what the page said: console messages, log entries, exceptions
const send = (method, params = {}, sessionId) => new Promise((resolve, reject) => {
  const id = nextId++;
  pending.set(id, { resolve, reject, method });
  ws.send(JSON.stringify(Object.assign({ id, method, params }, sessionId ? { sessionId } : {})));
});

async function startChrome() {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'igui-web-'));
  for (const d of ['profile', 'home', 'cfg', 'cache', 'data']) fs.mkdirSync(path.join(tmp, d));
  const env = Object.assign({}, process.env, { HOME: path.join(tmp, 'home'), XDG_CONFIG_HOME: path.join(tmp, 'cfg'),
    XDG_CACHE_HOME: path.join(tmp, 'cache'), XDG_DATA_HOME: path.join(tmp, 'data') });
  delete env.DISPLAY; delete env.WAYLAND_DISPLAY;
  // WebGL in a headless browser with no GPU: ANGLE over SwiftShader
  chromeProc = cp.spawn(chrome, ['--headless=new', '--remote-debugging-port=0', '--user-data-dir=' + path.join(tmp, 'profile'),
    '--no-first-run', '--no-default-browser-check', '--disable-dev-shm-usage', '--disable-extensions', '--mute-audio',
    '--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl', 'about:blank'],
    { env, stdio: 'ignore' });
  const portFile = path.join(tmp, 'profile', 'DevToolsActivePort');
  for (let i = 0; i < 200 && !fs.existsSync(portFile); i++) await sleep(100);
  if (!fs.existsSync(portFile)) throw new Error('Chrome did not start');
  const [p, wsPath] = fs.readFileSync(portFile, 'utf8').split('\n');
  ws = new WebSocket('ws://127.0.0.1:' + p + wsPath);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  ws.onmessage = ev => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) {
      const { resolve, reject, method } = pending.get(m.id);
      pending.delete(m.id);
      if (m.error) reject(new Error(method + ': ' + JSON.stringify(m.error))); else resolve(m.result);
    } else if (m.method === 'Runtime.consoleAPICalled') {
      events.push({ kind: 'console.' + m.params.type, text: m.params.args.map(a => a.value !== undefined ? String(a.value) : (a.description || '')).join(' ') });
    } else if (m.method === 'Log.entryAdded') {
      events.push({ kind: 'log.' + m.params.entry.level, text: m.params.entry.text + (m.params.entry.url ? ' (' + m.params.entry.url + ')' : '') });
    } else if (m.method === 'Runtime.exceptionThrown') {
      const d = m.params.exceptionDetails;
      events.push({ kind: 'exception', text: (d.exception && d.exception.description) || d.text });
    }
  };
}

async function newPage(url) {
  const { targetId } = await send('Target.createTarget', { url: 'about:blank' });
  const { sessionId } = await send('Target.attachToTarget', { targetId, flatten: true });
  await send('Page.enable', {}, sessionId);
  await send('Runtime.enable', {}, sessionId);
  await send('Log.enable', {}, sessionId);
  await send('Page.bringToFront', {}, sessionId);
  await send('Page.navigate', { url }, sessionId);
  return { sessionId, targetId };
}

async function ev(s, expression) {
  const r = await send('Runtime.evaluate', { expression, returnByValue: true }, s);
  if (r.exceptionDetails) throw new Error('evaluate: ' + JSON.stringify(r.exceptionDetails.exception || r.exceptionDetails));
  return r.result.value;
}

async function main() {
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  const port = server.address().port;
  await startChrome();
  for (const preset of presets) {
    console.log('\n== preset ' + preset);
    const from = events.length;
    const { sessionId: s, targetId } = await newPage('http://localhost:' + port + '/igui_web_probe.html?preset=' + preset);
    let probe = null;
    for (let i = 0; i < 400; i++) { // 20 s
      probe = await ev(s, 'JSON.parse(JSON.stringify(window.igProbe || null))');
      if (probe && probe.frames >= 6) break;
      await sleep(50);
    }
    if (!probe || probe.frames < 6) {
      check('the page ran and drew frames', false, { probe, output: await ev(s, 'JSON.stringify({out: window.igPrint, err: window.igPrintErr})') });
    } else {
      await sleep(300); // a few more frames: an error that comes with the second draw
      probe = await ev(s, 'JSON.parse(JSON.stringify(window.igProbe))');
      console.log('  renderer: ' + probe.renderer + ', ' + probe.fb[0] + 'x' + probe.fb[1] + ', ' + probe.frames + ' frames');
      check('the program drew with OpenGL, not the software rasteriser', probe.usesGL === true, probe.renderer);
      check('glGetError is 0 after the frame', probe.glError === 0, probe.glError);
      const tolerance = 10;
      for (const r of probe.regions) {
        const d = [0, 1, 2].map(i => Math.abs(r.got[i] - r.want[i]));
        const ok = Math.max(...d) <= tolerance && r.got[3] === 255;
        check('"' + r.name + '" is ' + r.want.join(',') + ' within ' + tolerance, ok, ok ? undefined : 'got ' + r.got.join(','));
      }
      // not one channel: the three panels of their own colours differ from each other in every channel pair
      const own = probe.regions.filter(r => ['orange', 'green', 'blue'].includes(r.name));
      const channels = [0, 1, 2].map(c => Math.max(...own.map(r => r.got[c])) - Math.min(...own.map(r => r.got[c])));
      check('the picture is not single-channel (each channel varies across the three panels)', channels.every(v => v > 40), channels);
      check('the label draws text (' + probe.ink.pixels + ' of ' + probe.ink.of + ' pixels are ink)', probe.ink.pixels >= 20);
      // the chart's layer is a Mesh drawn by reference (a vertex array of its own): lines and a grid over the plot's background
      check('the chart is drawn (' + probe.chart.pixels + ' pixels differ from its background)', probe.chart.pixels >= 200);
    }
    const bad = events.slice(from).filter(e => /^(console\.(error|warning|assert)|log\.(error|warning)|exception)/.test(e.kind) &&
      !/favicon|DevTools|GPU stall|swiftshader|Automatic fallback to software WebGL|GroupMarker|Use --enable-unsafe/i.test(e.text));
    const webgl = bad.filter(e => /WebGL|INVALID_|GL_|GLSL|shader|texParameter|texImage/i.test(e.text));
    check('the console has no WebGL error', webgl.length === 0, webgl.map(e => e.kind + ': ' + e.text).slice(0, 5));
    check('the console has no other error or exception', bad.length === webgl.length, bad.filter(e => !webgl.includes(e)).map(e => e.kind + ': ' + e.text).slice(0, 5));
    await send('Target.closeTarget', { targetId });
  }
}

main().catch(e => { fail++; console.log('  FAIL  ' + (e && e.stack || e)); }).finally(async () => {
  try { ws && ws.close(); } catch (e) {}
  try { chromeProc && chromeProc.kill('SIGTERM'); } catch (e) {}
  server.close();
  await sleep(300);
  if (!keep && tmp) { try { fs.rmSync(tmp, { recursive: true, force: true }); } catch (e) {} }
  console.log('\n' + (fail ? 'FAIL' : 'PASS') + ': ' + pass + ' checks held, ' + fail + ' did not');
  process.exit(fail ? 1 : 0);
});
