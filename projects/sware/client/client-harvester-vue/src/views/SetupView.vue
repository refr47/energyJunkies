<template>
  <div class="min-h-screen bg-slate-50 p-4 md:p-8">
    <StatusToast :message="statusMsg" :type="statusClass" @close="statusMsg = ''" />
    <div class="max-w-7xl mx-auto">

      <div class="flex justify-between items-end mb-10">
        <div>
          <h1 class="text-3xl font-black text-slate-800 tracking-tight">System-Stammdaten</h1>
          <p class="text-slate-500 font-medium">Harvester v3.0 Konfigurationspanel</p>
        </div>
        <button @click="pushToESP"
          class="bg-emerald-600 hover:bg-emerald-700 text-white px-8 py-3 rounded-2xl shadow-lg shadow-emerald-200 transition-all font-bold flex items-center gap-2">
          <span>💾 Konfiguration speichern</span>
        </button>
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
            <ConfigInput label="Regel-Präzision (PID)" v-model="setup.pid_epsilon" type="number" step="0.01" />
            <div class="pt-2">
              <label class="text-[10px] font-bold text-slate-400 uppercase ml-1">Force Mode (Laden)</label>
              <select v-model="setup.heizstab_force"
                class="w-full bg-slate-50 border border-slate-200 rounded-xl px-3 py-2.5 mt-1 focus:ring-2 focus:ring-rose-500 outline-none font-medium">
                <option value="1">Erzwungen (Immer An)</option>
                <option value="0">Automatik (PV-Überschuss)</option>
              </select>
            </div>
          </div>
        </div>

        <div class="bg-white p-6 rounded-[2rem] shadow-sm border border-slate-100">
          <h2 class="text-[11px] font-black text-amber-500 uppercase tracking-widest mb-6">Management & Hygiene</h2>
          <div class="space-y-4">
            <div class="p-4 bg-amber-50/50 rounded-2xl border border-amber-100">
              <ConfigInput label="Legionellen Ziel-Temp" v-model="setup.legionellen_temp" type="number" unit="°C" />
              <ConfigInput label="Hysterese (Diff)" v-model="setup.legionellen_differenz" type="number" unit="K" />
            </div>
            <div class="pt-4">
              <label
                class="text-[10px] font-bold text-slate-400 uppercase ml-1 text-emerald-600">Speicher-Priorität</label>
              <select v-model="setup.akku_vorhanden"
                class="w-full bg-slate-50 border border-slate-200 rounded-xl px-3 py-2.5 mt-1 outline-none font-medium mb-3">
                <option value="1">Akku vorhanden</option>
                <option value="0">Kein Akku</option>
              </select>
              <ConfigInput v-if="setup.akku_vorhanden === 'j'" label="Priorität" v-model="setup.akku_priori"
                type="number" />
            </div>
          </div>
        </div>

        <div class="bg-white p-6 rounded-[2rem] shadow-sm border border-slate-100 lg:col-span-2">
          <h2 class="text-[11px] font-black text-indigo-500 uppercase tracking-widest mb-6">Datenschnittstellen (MQTT &
            InfluxDB)</h2>
          <div class="grid grid-cols-1 md:grid-cols-2 gap-8">
            <div class="space-y-4">
              <p class="text-[10px] font-bold text-slate-300 -mb-2 italic">MQTT Broker</p>
              <ConfigInput label="Server IP" v-model="setup.mqtt_server_ip" icon="server" />
              <div class="grid grid-cols-2 gap-3">
                <ConfigInput label="User" v-model="setup.mqtt_user" />
                <ConfigInput label="Passwort" v-model="setup.mqtt_passwd" />
              </div>
            </div>
            <div class="space-y-4 border-l border-slate-50 md:pl-8">
              <p class="text-[10px] font-bold text-slate-300 -mb-2 italic">InfluxDB Time-Series</p>
              <ConfigInput label="Server URL" v-model="setup.influx_server_ip" />
              <ConfigInput label="Organization" v-model="setup.influg_org" />
              <ConfigInput label="Access Token" v-model="setup.influx_token"/>
            </div>
          </div>
        </div>

      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
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
  heizstab_force: 0, // 'j' oder 'n'

  // Hygiene & Legionellen
  legionellen_temp: 65,
  legionellen_differenz: 5,

  // Energiemanagement
  akku_vorhanden: 'n',
  akku_priori: 1,

  // Daten-Schnittstellen (MQTT / Influx)
  mqtt_server_ip: '',
  mqtt_user: '',
  mqtt_passwd: '',
  influx_server_ip: '',
  influx_token: '',
  influx_org: '',
  influx_bucket: '',

  // Regelungstechnik
  pid_epsilon: 0.1
});
const load = async () => {
  try {
    const res = await fetch('/getSetup');
    const data = await res.json();
    setup.value = { ...setup.value, ...data };
    statusMsg.value = "Erfolgreich geladen.";
    console.log("Setup Daten geladen:", data);
    
    notify("Konfiguration erfolgreich geladen!", "success");
  } catch (e) {
    statusMsg.value = "Konnte Daten vom ESP32 nicht laden.";
   
    notify("Fehler beim Laden der Konfiguration!", "error");
  }
};

const pushToESP = async () => {
  statusMsg.value = "Speichere...";
  console.log("Sende Setup Daten:", setup.value);
  try {
    const res = await fetch('/storeSetup', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(setup.value)
    });
    const data = await res.json();
    console.log("storeSetup - returned data:", data);
    if (data.done) {
      notify("Konfiguration erfolgreich gespeichert! Der ESP32 startet neu...", "success");
   
    } else {
      notify("Fehler beim Speichern der Konfiguration!", "error");
    }
  } catch (e) {
    notify("Fehler beim Speichern der Konfiguration!", "error");
    console.log("Fehlerdetails:", e);
   
  }
};

onMounted(load);
</script>