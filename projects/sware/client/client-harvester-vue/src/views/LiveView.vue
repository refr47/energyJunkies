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
    </div>

    <LiveValueTable :items="liveDataItems" />

    <table id="logTable">
      <thead>
        <tr>
          <th>Zeit</th>
          <th>L1</th>
          <th>L2</th>
          <th>PWM (%)</th>
          <th>Temp</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="entry in logs" :key="entry.ts">
          <td>{{ new Date(entry.ts * 1000).toLocaleTimeString() }}</td>
          <td>{{ entry.state == null ? "--" : formatRelay(entry.state & 1) }}</td>
          <td>{{ entry.state == null ? "--" : formatRelay((entry.state >> 1) & 1) }}</td>
          <td>{{ formatValue("L3", entry.pwm) }}</td>
          <td>{{ formatValue("bTemp", entry.temp) }}</td>
        </tr>
      </tbody>
    </table>

    <LiveValueTable
      v-if="tinyNNItems.length"
      :items="tinyNNItems"
      caption="🧠 TinyNN-Prädiktion"
      row-class="bg-violet-50"
      class="mt-4"
    />
    <div class="w-full max-w-4xl mx-auto mt-6 p-4 border border-slate-200 rounded-lg bg-slate-50">
      <div class="text-xs font-bold uppercase tracking-widest text-slate-400 mb-3">⚡ Energie-Zusammensetzung</div>

      <div class="space-y-2">
        <div v-for="bar in energyBars" :key="bar.key">
          <div class="flex justify-between text-xs mb-1">
            <span class="text-slate-500">{{ bar.label }}</span>
            <span class="font-bold" :class="bar.textClass">{{ bar.display }}</span>
          </div>
          <div class="h-3 bg-slate-200 rounded-full overflow-hidden">
            <div class="h-full rounded-full transition-all duration-500"
              :class="bar.fillClass" :style="{ width: bar.width + '%' }"></div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from "vue";
import LiveValueTable from "../components/LiveValueTable.vue";
import { dataMapping, tinyNNMapping, formatValue, formatRelay, finiteNumber, energyBarsFor, interpretErrors } from "../utils/liveDisplay.js";

// App.vue besitzt die Verbindung und dekodiert die binären Log-Einträge.
const props = defineProps({
  liveData: { type: Object, default: () => ({}) },
  logs: { type: Array, default: () => [] },
  isConnected: Boolean,
});
const gateway = `ws://${window.location.hostname}/ws`;
const errors = computed(() => interpretErrors(props.liveData.errors));
const itemsFor = (mapping) => mapping.map((item) => ({
  ...item,
  value: formatValue(item.key, props.liveData[item.key]),
  style: item.key === "solEr" && finiteNumber(props.liveData.solEr) > 0
    ? { backgroundColor: "lightgreen" } : undefined,
}));
const liveDataItems = computed(() => itemsFor(dataMapping));
const tinyNNItems = computed(() =>
  "tinyNN_preheat" in props.liveData ? itemsFor(tinyNNMapping) : [],
);
const energyBars = computed(() => energyBarsFor(props.liveData));
</script>
