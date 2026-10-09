<template>
  <div id="wsInfo">
    <div id="a" :class="{ 'has-errors': errors.length > 0 }">
      <div class="flex-container">
        <ul id="errorL">
          <li v-if="errors.length === 0">Keine Fehler</li>
          <li v-for="(error, index) in errors" :key="index">{{ error }}</li>
        </ul>
      </div>
    </div>

    <h1>Übersicht</h1>

    <div class="wsBase">
      <div class="row">
        <div>WebSocket:</div>
        <div>{{ gateway }}</div>
      </div>
      <div class="row">
        <div>Verbindung:</div>
        <div>{{ isConnected ? "✔" : "✖" }}</div>
      </div>
      <div class="row">
        <div>Update:</div>
        <div>{{ isUpdating ? "⇅" : "✖" }}</div>
      </div>
    </div>

    <table class="stripe" class="w-full">
      <thead>
        <tr>
          <th>Titel</th>
          <th>Wert</th>
          <th>Einheit</th>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="item in liveDataItems"
          :key="item.label"
          :style="getRowStyle(item)"
        >
          <td>{{ item.label }}</td>
          <td>{{ item.value }}</td>
          <td>{{ item.unit }}</td>
        </tr>
      </tbody>
    </table></div>

    <table id="logTable">
      <thead>
        <tr>
          <th>Zeit</th>
          <th>L1</th>
          <th>L2</th>
          <th>PWM</th>
          <th>Temp</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="entry in logEntries" :key="entry.ts">
          <td>{{ new Date(entry.ts * 1000).toLocaleTimeString() }}</td>
          <td>{{ entry.l1 }}</td>
          <td>{{ entry.l2 }}</td>
          <td>{{ entry.pwm }}</td>
          <td>{{ entry.temp.toFixed(1) }}</td>
        </tr>
      </tbody>
    </table></div>

    <table v-if="tinyNNItems.length" class="stripe" class="w-full mt-4">
      <caption class="text-xs font-bold uppercase tracking-widest text-slate-400 mb-2">🧠 TinyNN-Prädiktion</caption>
      <thead>
        <tr>
          <th>Titel</th>
          <th>Wert</th>
          <th>Einheit</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="item in tinyNNItems" :key="item.label" class="bg-violet-50">
          <td>{{ item.label }}</td>
          <td>{{ item.value }}</td>
          <td>{{ item.unit }}</td>
        </tr>
      </tbody>
    </table></div>

    <!-- Stacked-Bar: Heizer-Leistung-Zusammensetzung -->
    <div class="w-full max-w-4xl mx-auto mt-6 p-4 border border-slate-200 rounded-lg bg-slate-50">
      <div class="text-xs font-bold uppercase tracking-widest text-slate-400 mb-3">⚡ Energie-Zusammensetzung</div>

      <div class="space-y-2">
        <!-- Basis (PWM L3) -->
        <div>
          <div class="flex justify-between text-xs mb-1">
            <span class="text-slate-500">Basis (PWM)</span>
            <span class="font-bold text-blue-600">{{ Math.round(pwmVal) }}%</span>
          </div>
          <div class="h-3 bg-slate-200 rounded-full overflow-hidden">
            <div class="h-full bg-blue-500 rounded-full transition-all duration-500"
              :style="{ width: Math.max(0, Math.min(100, pwmVal)) + '%' }"></div>
          </div>
        </div>

        <!-- Wetter-Bonus -->
        <div>
          <div class="flex justify-between text-xs mb-1">
            <span class="text-slate-500">Wetter-Bonus</span>
            <span class="font-bold text-amber-600">+{{ weatherVal }}°C</span>
          </div>
          <div class="h-3 bg-slate-200 rounded-full overflow-hidden">
            <div class="h-full bg-amber-400 rounded-full transition-all duration-500"
              :style="{ width: weatherBarWidth + '%' }"></div>
          </div>
        </div>

        <!-- TinyNN Preheat (optional) -->
        <div v-if="tinyNNPreheatVal > 0">
          <div class="flex justify-between text-xs mb-1">
            <span class="text-slate-500">NN Preheat</span>
            <span class="font-bold text-violet-600">{{ Math.round(tinyNNPreheatVal) }}%</span>
          </div>
          <div class="h-3 bg-slate-200 rounded-full overflow-hidden">
            <div class="h-full bg-violet-500 rounded-full transition-all duration-500"
              :style="{ width: tinyNNPreheatVal + '%' }"></div>
          </div>
        </div>

        <!-- TinyNN Budget (optional) -->
        <div v-if="tinyNNBufferVal > 0">
          <div class="flex justify-between text-xs mb-1">
            <span class="text-slate-500">NN Energiebudget</span>
            <span class="font-bold text-teal-600">{{ Math.round(tinyNNBufferVal) }}%</span>
          </div>
          <div class="h-3 bg-slate-200 rounded-full overflow-hidden">
            <div class="h-full bg-teal-400 rounded-full transition-all duration-500"
              :style="{ width: tinyNNBufferVal + '%' }"></div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, computed } from "vue";

// State
const isConnected = ref(false);
const isUpdating = ref(false);
const errors = ref([]);
const live = ref({});
const logEntries = ref([]);
const gateway = `ws://${window.location.hostname}/ws`;

// Definition der Tabellenstruktur (Mapping von Key zu Label/Einheit)
const dataMapping = [
  { key: "PR", label: "Produktion", unit: "Watt" },
  { key: "EV", label: "Verbrauch", unit: "Watt" },
  { key: "EINS", label: "Einspeisung/Bezug", unit: "Watt" },
  { key: "TPS", label: "Temperatur", unit: "Grad" },
  { key: "L1", label: "Heizstab L1", unit: "on/off" },
  { key: "L2", label: "Heizstab L2", unit: "on/off" },
  { key: "L3", label: "PWM L3", unit: "%" },
  { key: "hsPhase", label: "Heizstab/Phase", unit: "W" },
  { key: "weatherBonus", label: "Wetter-Bonus", unit: "°C" },
  { key: "weatherPvRatio", label: "PV-Verhältnis", unit: "--" },
  { key: "weatherCloudAvg", label: "Wolken", unit: "%" },
  { key: "weatherOutTemp", label: "Außentemperatur", unit: "°C" },
];

// TinyNN-Felder (optional, nur wenn TinyNN im ESP32 aktiv)
const tinyNNMapping = [
  { key: "tinyNN_preheat", label: "NN Preheat", unit: "%" },
  { key: "tinyNN_buffer", label: "NN Energiebudget", unit: "%" },
];

const liveDataItems = computed(() => {
  return dataMapping.map((m) => ({
    label: m.label,
    value: fmtValue(m.key, live.value[m.key] ?? "--"),
    unit: m.unit,
  }));
});

const tinyNNItems = computed(() => {
  if (!"tinyNN_preheat" in live.value) return [];
  return tinyNNMapping.map((m) => ({
    label: m.label,
    value: fmtValue(m.key, live.value[m.key] ?? "--"),
    unit: m.unit,
  }));
});

function fmtValue(key, val) {
  if (val === "--") return val;
  if (key === "weatherCloudAvg") return Math.round(Number(val) * 100) + "%";
  if (key === "tinyNN_preheat" || key === "tinyNN_buffer") return Math.round(Number(val) * 100) + "%";
  if (typeof val === "number") return val.toFixed(1);
  return val;
}

// ── Balkendiagramm-Werte (Energie-Zusammensetzung) ──
const pwmVal = computed(() => Number(live.value.L3) ?? 0);
const weatherVal = computed(() => Number(live.value.weatherBonus) ?? 0);
const weatherBarWidth = computed(() => Math.max(0, Math.min(100, (Number(live.value.weatherBonus) ?? 0) * 20)));
const tinyNNPreheatVal = computed(() => (Number(live.value.tinyNN_preheat) ?? 0) * 100);
const tinyNNBufferVal = computed(() => (Number(live.value.tinyNN_buffer) ?? 0) * 100);

// Logic: Errors interpretieren
const interpretErrors = (bitVektor) => {
  const errMsgs = [];
  if (bitVektor & (1 << 4)) errMsgs.push("SD-Kartenleser defekt");
  if (bitVektor & (1 << 5)) errMsgs.push("Modbus Fehler");
  // ... usw
  errors.value = errMsgs;
};

// WebSocket Setup
const initWebSocket = () => {
  const ws = new WebSocket(gateway);
  ws.onopen = () => (isConnected.value = true);
  ws.onclose = () => {
    isConnected.value = false;
    setTimeout(initWebSocket, 2000);
  };
  ws.onmessage = (event) => {
    isUpdating.value = true;
    const data = JSON.parse(event.data);
    live.value = data.live;
    logEntries.value = data.log.entries;
    interpretErrors(data.live.FE);
    setTimeout(() => (isUpdating.value = false), 500);
  };
};

const getRowStyle = (item) => {
  if (item.label === "Produktion" && item.value > 0)
    return { backgroundColor: "lightgreen" };
  return {};
};

onMounted(initWebSocket);
</script>
