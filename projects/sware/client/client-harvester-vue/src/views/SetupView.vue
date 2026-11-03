<template>
  <div class="min-h-screen bg-slate-50 px-4 pt-4 pb-24 md:pb-4 md:px-8">
    <StatusToast :message="statusMsg" :type="statusClass" @close="statusMsg = ''" />
    <div class="max-w-7xl mx-auto">

      <div class="flex flex-col sm:flex-row justify-between items-start sm:items-end gap-4 mb-10">
        <div>
          <h1 class="text-3xl font-black text-slate-800 tracking-tight">System-Stammdaten</h1>
          <p class="text-slate-500 font-medium">Harvester v3.0 Konfigurationspanel</p>
        </div>
        <button @click="pushToESP" :disabled="saving"
          class="w-full sm:w-auto bg-emerald-600 hover:bg-emerald-700 text-white px-6 py-3 rounded-2xl shadow-lg sticky bottom-4 z-30 sm:static shadow-emerald-200 transition-all font-bold flex items-center gap-2">
          <span>💾 Konfiguration speichern</span>
        </button>
      </div>

      <div v-if="validationErrors.length" role="alert" class="mb-6 rounded-xl bg-rose-50 p-4 text-rose-800">
        <p class="font-bold">Nicht gespeichert. Bitte folgende Angaben korrigieren:</p>
        <ul class="list-disc pl-5">
          <li v-for="(error, index) in validationErrors" :key="index">{{ error.field }}: {{ error.message }}<span v-if="error.maxLength"> (max. {{ error.maxLength }} Bytes)</span></li>
        </ul>
      </div>
      <p class="mb-4 text-sm text-slate-500">MQTT und Influx lassen sich unabhängig aktivieren. Beim Abschalten bleiben die Zugangsdaten erhalten.</p>
      <div class="mb-4 text-sm text-slate-600" role="status" aria-live="polite">
        {{ applyMessage }}
        <span v-if="setup.mqtt_enabled"> · MQTT: {{ runtimeState.mqttConnected ? 'verbunden' : 'nicht verbunden' }}</span>
        <span v-if="setup.influx_enabled"> · Influx: {{ runtimeState.influxConnected ? 'verbunden' : 'nicht verbunden' }}</span>
      </div>
      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">

        <div class="bg-white p-6 rounded-[2rem] shadow-sm border border-slate-100">
          <h2 class="text-[11px] font-black text-blue-500 uppercase tracking-widest mb-6">Konnektivität</h2>
          <div class="space-y-4">
            <ConfigInput label="WLAN SSID" v-model="setup.wlan_ssid" icon="wifi" />
            <ConfigInput label="WLAN Passwort" v-model="setup.wlan_password" type="password" />
            <ConfigInput label="Wechselrichter IP" v-model="setup.ip_wechselrichter" placeholder="192.168..." />
            <div class="pt-4 border-t border-slate-50">
              <ConfigInput label="AMIS Reader IP" v-model="setup.amisreader_ip" />
              <ConfigInput label="AMIS Key" v-model="setup.amisreader_key" />
            </div>
          </div>
        </div>

        <div class="bg-white p-6 rounded-[2rem] shadow-sm border border-slate-100">
          <h2 class="text-[11px] font-black text-rose-500 uppercase tracking-widest mb-6">Leistung & Regelung</h2>
          <div class="space-y-4">
            <ConfigInput label="Heizstab Leistung" v-model="setup.heizstab_leistung" type="number" unit="W" />
            <div class="grid grid-cols-2 gap-3">
              <ConfigInput label="Temp Min" v-model="setup.heizstab_temp_min" type="number" unit="°C" />
              <ConfigInput label="Temp Max" v-model="setup.heizstab_temp_max" type="number" unit="°C" />
            </div>
            <div class="grid grid-cols-2 gap-3">

              <ConfigInput label="Regel-Präzision (PID)" v-model="setup.pid_epsilon" type="number" step="0.01" />
              <ConfigInput label="Watt-Bias" v-model="setup.watt_bias" type="number" />
            </div>

            <div class="pt-2">
              <label class="text-[10px] font-bold text-slate-400 uppercase ml-1">Force Mode (Laden)</label>
              <select v-model.number="setup.heizstab_force"
                class="w-full bg-slate-50 border border-slate-200 rounded-xl px-3 py-2.5 mt-1 focus:ring-2 focus:ring-rose-500 outline-none font-medium">
                <option :value="1">Erzwungen (Immer An)</option>
                <option :value="0">Automatik (PV-Überschuss)</option>
              </select>
            </div>
          </div>
        </div>

        <div class="bg-white p-6 rounded-[2rem] shadow-sm border border-slate-100">
          <h2 class="text-[11px] font-black text-amber-500 uppercase tracking-widest mb-6">Management & Hygiene</h2>
          <div class="space-y-4">
            <div class="p-4 bg-amber-50/50 rounded-2xl border border-amber-100">
              <ConfigInput label="Legionellen Ziel-Temp" v-model="setup.legionellen_temp" type="number" unit="°C" />
              <ConfigInput label="Legionellenintervall" v-model="setup.legionellen_differenz" type="number" unit="h" />
            </div>
            <div class="pt-4">
              <label class="text-[10px] font-bold text-slate-400 uppercase ml-1 text-emerald-600">
                Speicher-Konfiguration
              </label>

              <div
                class="flex items-center space-x-3 bg-slate-50 border border-slate-200 rounded-xl px-3 py-2.5 mt-1 mb-3">
                <input id="akku-checkbox" type="checkbox" v-model="setup.akku_vorhanden" :true-value="true"
                  :false-value="false" class="w-5 h-5 accent-emerald-600 cursor-pointer" />
                <label for="akku-checkbox" class="text-sm font-medium text-slate-700 cursor-pointer">
                  Akku vorhanden
                </label>
              </div>

              <ConfigInput v-if="setup.akku_vorhanden" label="Priorität (0 = Heizpatrone zuerst, 1 = Akku zuerst)"
                v-model="setup.akku_priori" type="number" min="0" max="1" />
              <p v-if="setup.akku_vorhanden" class="mt-2 text-sm text-slate-500">
                Beide Modi schützen vor Akkuentladung im Automatikbetrieb. Akku zuerst:
                Heizfreigabe ab 90 %, erneute Sperre unter 88 %.
              </p>
            </div>
          </div>
        </div>

        <div class="bg-white p-6 rounded-[2rem] shadow-sm border border-slate-100 lg:col-span-2">
          <h2 class="text-[11px] font-black text-indigo-500 uppercase tracking-widest mb-6">Datenschnittstellen (MQTT &
            InfluxDB)</h2>
          <div class="grid grid-cols-1 md:grid-cols-2 gap-8">
            <div class="space-y-4">
              <p class="text-[10px] font-bold text-slate-300 -mb-2 italic">MQTT Broker</p>
              <label class="flex gap-2"><input type="checkbox" v-model="setup.mqtt_enabled" :disabled="!capabilities.mqtt" /> MQTT aktivieren</label>
              <p v-if="!capabilities.mqtt" class="text-sm text-amber-700">In dieser Firmware nicht verfügbar.</p>
              <fieldset :disabled="!setup.mqtt_enabled || !capabilities.mqtt" class="space-y-4 disabled:opacity-50">
              <ConfigInput label="Hostname / IP" v-model="setup.mqtt_server_ip" />
              <ConfigInput label="Port" v-model="setup.mqtt_port" type="number" min="1" max="65535" step="1" />
              <div class="grid grid-cols-2 gap-3">
                <ConfigInput label="User" v-model="setup.mqtt_user" />
                <ConfigInput label="Passwort" v-model="setup.mqtt_passwd" type="password" />
              </div>
              </fieldset>
            </div>
            <div class="space-y-4 border-l border-slate-50 md:pl-8">
              <p class="text-[10px] font-bold text-slate-300 -mb-2 italic">InfluxDB Time-Series</p>
              <label class="flex gap-2"><input type="checkbox" v-model="setup.influx_enabled" :disabled="!capabilities.influx" /> Influx aktivieren</label>
              <p v-if="!capabilities.influx" class="text-sm text-amber-700">In dieser Firmware nicht verfügbar.</p>
              <fieldset :disabled="!setup.influx_enabled || !capabilities.influx" class="space-y-4 disabled:opacity-50">
              <ConfigInput label="Server URL" v-model="setup.influx_server_ip" placeholder="http://influx.local:8086" />
              <ConfigInput label="Organization" v-model="setup.influx_org" />
              <ConfigInput label="Access Token" v-model="setup.influx_token" type="password" />
              <ConfigInput label="Bucket" v-model="setup.influx_bucket" />
              </fieldset>
            </div>
          </div>
        </div>

        <div class="bg-white p-6 rounded-[2rem] shadow-sm border border-slate-100 lg:col-span-3">
          <h2 class="font-bold mb-4">Adaptive Energy-Abfrage</h2>
          <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
            <ConfigInput label="Normalintervall" v-model="setup.energy_day_seconds" type="number" min="5" max="3600" step="1" unit="s" />
            <ConfigInput label="Intervall bei geringer PV-Leistung" v-model="setup.energy_saving_seconds" type="number" :min="setup.energy_day_seconds" max="86400" step="1" unit="s" />
            <ConfigInput label="Mindestdauer geringer PV-Leistung" v-model="setup.energy_hold_seconds" type="number" min="1" max="86400" step="1" unit="s" />
            <ConfigInput label="Sparbetrieb unter" v-model="setup.energy_low_pv_watts" type="number" min="0" max="1000000" step="0.1" unit="W" />
            <ConfigInput label="Normalbetrieb über" v-model="setup.energy_wake_pv_watts" type="number" min="0" max="1000000" step="0.1" unit="W" />
          </div>
          <label class="flex gap-2 my-4"><input type="checkbox" v-model="setup.energy_night_enabled" /> Nächtliche Abfragepause aktivieren</label>
          <fieldset :disabled="!setup.energy_night_enabled" class="flex gap-6 disabled:opacity-50">
            <label>Beginn <select v-model.number="setup.energy_night_start"><option v-for="hour in 24" :key="hour" :value="hour - 1">{{ String(hour - 1).padStart(2, '0') }}:00</option></select></label>
            <label>Ende <select v-model.number="setup.energy_night_end"><option v-for="hour in 24" :key="hour" :value="hour - 1">{{ String(hour - 1).padStart(2, '0') }}:00</option></select></label>
          </fieldset>
          <p class="mt-3 text-sm text-slate-500">Während der Pause erfolgen keine Energy-Abfragen. Gleiche Start- und Endzeit bedeutet keine Pause. Energieparameter werden ohne Neustart übernommen.</p>
        </div>

      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue';
import ConfigInput from '../components/ConfigInput.vue'; // Eine kleine Hilfs-Komponente
import StatusToast from '../components/StatusToast.vue'; // Importieren

const notify = (msg, type = 'success', timeout = 4000) => {
  statusMsg.value = msg;
  statusClass.value = type;
  if (timeout > 0) {
    setTimeout(() => statusMsg.value = '', timeout);
  }
};

const statusMsg = ref('');
const statusClass = ref('');
const setup = ref({
  // Netzwerk & Kommunikation
  wlan_ssid: '',
  wlan_password: '',
  ip_wechselrichter: '',
  amisreader_ip: '',
  amisreader_key: '',

  // Heizstab & Temperatur
  heizstab_leistung: 0,
  heizstab_temp_min: 0,
  heizstab_temp_max: 0,
  heizstab_force: 0,

  // Hygiene & Legionellen
  legionellen_temp: 65,
  legionellen_differenz: 5,

  // Energiemanagement
  akku_vorhanden: false,
  akku_priori: 1,

  // Daten-Schnittstellen (MQTT / Influx)
  mqtt_enabled: false,
  mqtt_port: 1883,
  influx_enabled: false,
  energy_day_seconds: 40,
  energy_saving_seconds: 300,
  energy_night_enabled: true,
  energy_night_start: 23,
  energy_night_end: 5,
  energy_low_pv_watts: 20,
  energy_wake_pv_watts: 50,
  energy_hold_seconds: 300,
  mqtt_server_ip: '',
  mqtt_user: '',
  mqtt_passwd: '',
  influx_server_ip: '',
  influx_token: '',
  influx_org: '',
  influx_bucket: '',

  // Regelungstechnik
  pid_epsilon: 0.1,
  watt_bias: 0
});

const capabilities = ref({ mqtt: false, influx: false });
const runtimeState = ref({});
const applyMessage = ref('');
const saving = ref(false);
let pollTimer;
let disposed = false;

const updateState = data => {
  capabilities.value = data.capabilities || { mqtt: false, influx: false };
  runtimeState.value = data.state || {};
  if (data.source === 'defaults') { applyMessage.value = 'Standardwerte geladen; noch nicht gespeichert.'; return; }
  applyMessage.value = runtimeState.value.pending ? 'Gespeichert; Übernahme durch die Dienste steht aus.' : 'Gespeicherte Einstellungen sind übernommen.';
};
const pollApplied = (revision, attempt = 0) => {
  clearTimeout(pollTimer);
  pollTimer = setTimeout(async () => {
    if (disposed) return;
    try {
      const response = await fetch('/getSetup');
      const data = await response.json();
      if (disposed) return;
      if (response.ok && data.error?.code === 1) {
        updateState(data); // Do not overwrite edits made while polling.
        if (data.state?.savedRevision === revision && !data.state.pending) return;
      }
    } catch { /* A restart temporarily interrupts the connection. */ }
    if (!disposed && attempt < 29) pollApplied(revision, attempt + 1);
    else if (!disposed) applyMessage.value = 'Übernahme noch nicht bestätigt. Verbindung oder Gerätestatus prüfen.';
  }, 2000);
};

const load = async () => {
  try {
    const res = await fetch('/getSetup');
    const data = await res.json();
    if (data.error.code !== 1) {
      notify("Fehler beim Laden: " + (data.error.msg || "Unbekannt"), "error");
      return;
    }
    let result = data.result;
    // ESP32 sendet akku_vorhanden als Zahl (0/1) oder bool → auf true/false normalisieren
    if (typeof result.akku_vorhanden === 'number') {
      result.akku_vorhanden = result.akku_vorhanden !== 0;
    }
    setup.value = { ...setup.value, ...result };
    updateState(data);


    notify(data.warning || "Konfiguration erfolgreich geladen!", data.warning ? "info" : "success");
  } catch (e) {
    notify("Keine Verbindung zum ESP32.", "error");
    console.error("Fetch-Error:", e);
  }
};

const validationErrors = ref([]);

const pushToESP = async () => {
  if (saving.value) return;
  saving.value = true;
  clearTimeout(pollTimer);
  validationErrors.value = [];
  statusMsg.value = "Speichere...";

  try {
    const res = await fetch('/storeSetup', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(setup.value)
    });
    const data = await res.json();
    console.log("storeSetup - returned data:", data);
    if (data.done) {
      const warnings = Array.isArray(data.warnings) ? data.warnings : [];
      applyMessage.value = data.apply === 'restart_required' ? 'Gespeichert; Gerät startet neu.' : 'Gespeichert; Übernahme läuft.';
      notify(warnings.length ? warnings.join(' ') : applyMessage.value, warnings.length ? 'info' : 'success', warnings.length ? 0 : 4000);
      pollApplied(data.revision);

    } else {
      validationErrors.value = Array.isArray(data.errors) ? data.errors : [];
      notify(data.error || "Fehler beim Speichern der Konfiguration!", "error", 0);
      if (data.error) {
        console.error("Error in storing setup: ", data.error);
      }
    }
  } catch (e) {
    notify("Fehler beim Speichern der Konfiguration!", "error");
    console.log("Fehlerdetails:", e);
  } finally {
    saving.value = false;
  }
};

onMounted(load);
onUnmounted(() => { disposed = true; clearTimeout(pollTimer); });
</script>