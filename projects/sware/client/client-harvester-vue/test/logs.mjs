import assert from 'node:assert/strict';
import { createServer } from 'vite';
import { createSSRApp } from 'vue';
import { renderToString } from 'vue/server-renderer';
import { parseLogBlob } from '../src/utils/logs.js';

// Little-endian firmware packet: ts=1, state=3, power=-100, pwm=255, temp=-5.
const packet = Buffer.from([1, 0, 0, 0, 3, 156, 255, 255, 251, 255]);
const entry = { ts: 1, state: 3, power: -100, pwm: 255, temp: -5 };
assert.deepEqual(parseLogBlob(packet.toString('base64'), 1), [entry]);
assert.deepEqual(parseLogBlob(packet.toString('base64'), 60), [entry]);
assert.deepEqual(parseLogBlob(packet.subarray(0, 9).toString('base64'), 1), []);
assert.deepEqual(parseLogBlob(Buffer.concat([packet, packet]).toString('base64'), 1), [entry]);
assert.deepEqual(parseLogBlob('', 0), []);
assert.throws(() => parseLogBlob('!', 1));

const server = await createServer({ server: { middlewareMode: true, ws: false }, appType: 'custom' });
try {
  const { default: LogView } = await server.ssrLoadModule('/src/views/LogView.vue');
  for (const count of [0, 1, 21]) {
    const logs = Array.from({ length: count }, (_, i) => ({ ...entry, ts: i + 1, firstSeen: i + 1, lastSeen: i + 1 }));
    const app = createSSRApp(LogView, { logs });
    const warnings = [];
    app.config.warnHandler = (message) => warnings.push(message);
    const html = await renderToString(app);
    assert.ok(html.includes(`Seite 1 / ${count > 20 ? 2 : 1}`));
    assert.deepEqual(warnings, []);
  }
} finally {
  await server.close();
}
console.log('Binary log decoding and log pagination checks passed.');
