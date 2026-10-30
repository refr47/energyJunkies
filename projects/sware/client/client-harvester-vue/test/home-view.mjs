import assert from 'node:assert/strict';
import { createServer } from 'vite';
import { createSSRApp, effectScope, nextTick, ref } from 'vue';
import { renderToString } from 'vue/server-renderer';
import { useHeaterToggle } from '../src/composables/useHeaterToggle.js';

const server = await createServer({ server: { middlewareMode: true, ws: false }, appType: 'custom' });
try {
  const { default: HomeView } = await server.ssrLoadModule('/src/views/HomeView.vue');
  const warnings = [];
  const render = async (liveData, isConnected = true) => {
    const app = createSSRApp(HomeView, { liveData, isConnected });
    app.config.warnHandler = (message) => warnings.push(message);
    return renderToString(app);
  };
  const sample = {
    L1: 1, L2: 0, L3: 255, hsPhase: 1000, bTemp: 45,
    solEr: 4000, netzBezug: -1000, ev: 3000,
    errors: 0, forceHeizung: 0, forceHeizungOff: 0,
    weatherBonus: 3, weatherPvRatio: 1, weatherCloudAvg: 0.5, weatherOutTemp: 20,
  };
  for (const source of ['fronius', 'huawei', 'loxone', 'amis', 'none']) {
    const html = await render({ ...sample, dataSource: source });
    assert.ok(html.includes('Power Hub'));
    assert.ok(html.includes('100%'));
    assert.ok(html.includes('1000W'));
    assert.ok(html.includes('0W'));
    assert.ok(!html.includes('TinyNN-Prädiktion'));
    assert.equal(html.includes('Keine Energiequelle verfuegbar'), source === 'none');
  }
  const html = await render({ ...sample, dataSource: 'loxone', errors: 8,
    forceHeizungOff: 1, tinyNN_preheat: 0.75, tinyNN_buffer: 0.5 }, false);
  for (const text of ['Temperatur Sensorik', 'Verbindung verloren', 'STOP KILL', 'TinyNN-Prädiktion', '75%'])
    assert.ok(html.includes(text), text);
  await render({});
  await render(undefined);
  assert.deepEqual(warnings, []);
} finally {
  await server.close();
}

const originalFetch = globalThis.fetch;
const originalError = console.error;
const scope = effectScope();
try {
  for (const key of ['heizstab_force', 'heizstab_force_off']) {
    const value = ref(0);
    const control = scope.run(() => useHeaterToggle(() => value.value, key));
    let request;
    globalThis.fetch = async (url, options) => { request = { url, ...options }; return { ok: true }; };
    await control.toggle();
    assert.equal(control.enabled.value, true);
    assert.equal(request.url, '/storeSetup');
    assert.equal(request.method, 'POST');
    assert.deepEqual(JSON.parse(request.body), { [key]: 1 });
    value.value = 1;
    await nextTick();
    value.value = 0;
    await nextTick();
    assert.equal(control.enabled.value, false);
    globalThis.fetch = async () => { throw new Error('network down'); };
    console.error = () => {};
    await control.toggle();
    assert.equal(control.enabled.value, false);
  }
} finally {
  scope.stop();
  globalThis.fetch = originalFetch;
  console.error = originalError;
}
console.log('HomeView rendering and heater control checks passed.');
