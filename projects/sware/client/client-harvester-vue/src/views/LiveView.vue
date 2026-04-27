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

    <table class="stripe" style="width: 80%">
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
    </table>

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
    </table>
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
  // ... ergänze hier die restlichen Keys
];

const liveDataItems = computed(() => {
  return dataMapping.map((m) => ({
    label: m.label,
    value: live.value[m.key] || "0",
    unit: m.unit,
  }));
});

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
