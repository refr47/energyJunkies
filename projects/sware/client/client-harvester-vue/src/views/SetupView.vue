<template>
  <div class="min-h-screen bg-slate-50 p-4 md:p-8">
    <div class="max-w-4xl mx-auto">

      <div class="flex justify-between items-center mb-8">
        <h1 class="text-2xl font-bold text-slate-800">System-Konfiguration</h1>
        <button @click="pushToESP"
          class="bg-blue-600 hover:bg-blue-700 text-white px-6 py-2 rounded-lg shadow-lg transition-all font-bold">
          💾 Alles Speichern
        </button>
      </div>

      <div v-if="statusMsg"
        :class="statusClass === 'success' ? 'bg-emerald-100 text-emerald-800' : 'bg-rose-100 text-rose-800'"
        class="mb-6 p-4 rounded-xl border flex items-center gap-3">
        <span>{{ statusClass === 'success' ? '✅' : '❌' }}</span>
        <p>{{ statusMsg }}</p>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">

        <div class="bg-white p-6 rounded-2xl shadow-sm border border-slate-100">
          <h2 class="text-xs font-black text-slate-400 uppercase tracking-widest mb-4">Netzwerk & WLAN</h2>
          <div class="space-y-4">
            <ConfigInput label="WLAN SSID" v-model="setup['WLAN_ESSID']" />
            <ConfigInput label="WLAN Passwort" v-model="setup['WLAN_Password']" type="password" />
            <ConfigInput label="Inverter IP" v-model="setup['IP_Inverter']" placeholder="192.168.1.x" />
          </div>
        </div>

        <div class="bg-white p-6 rounded-2xl shadow-sm border border-slate-100">
          <h2 class="text-xs font-black text-slate-400 uppercase tracking-widest mb-4">Heizstab-Parameter</h2>
          <div class="space-y-4">
            <ConfigInput label="Max. Leistung (W)" v-model="setup['Heizstableistung']" type="number" unit="Watt" />
            <ConfigInput label="Abschalt-Temp (°C)" v-model="setup['Ausschalt_Temperatur']" type="number" unit="°C" />
            <ConfigInput label="Einschalt-Temp (°C)" v-model="setup['Einschalt_Temperatur']" type="number" unit="°C" />
          </div>
        </div>

        <div class="bg-white p-6 rounded-2xl shadow-sm border border-slate-100">
          <h2 class="text-xs font-black text-slate-400 uppercase tracking-widest mb-4">Management</h2>
          <div class="space-y-4">
            <div>
              <label class="text-[10px] font-bold text-slate-500 uppercase ml-1">Externer Speicher</label>
              <select v-model="setup['Speicher']"
                class="w-full bg-slate-50 border border-slate-200 rounded-lg px-3 py-2 mt-1 focus:ring-2 focus:ring-blue-500 outline-none">
                <option value="j">Ja, vorhanden</option>
                <option value="n">Nein, nicht vorhanden</option>
              </select>
            </div>
            <ConfigInput label="Speicher Priorität" v-model="setup['Speicher_Prioritaet']" type="number" />
          </div>
        </div>

      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import ConfigInput from '../components/ConfigInput.vue'; // Eine kleine Hilfs-Komponente

const setup = ref({});
const statusMsg = ref('');
const statusClass = ref('');

const load = async () => {
  try {
    const res = await fetch('/getSetup');
    setup.value = await res.json();
  } catch (e) {
    statusMsg.value = "Konnte Daten vom ESP32 nicht laden.";
    statusClass.value = "error";
  }
};

const pushToESP = async () => {
  statusMsg.value = "Speichere...";
  try {
    const res = await fetch('/storeSetup', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(setup.value)
    });
    const data = await res.json();
    if (data.done) {
      statusMsg.value = "Erfolgreich gespeichert! Der ESP32 startet neu...";
      statusClass.value = "success";
    }
  } catch (e) {
    statusMsg.value = "Fehler beim Senden der Daten.";
    statusClass.value = "error";
  }
};

onMounted(load);
</script>