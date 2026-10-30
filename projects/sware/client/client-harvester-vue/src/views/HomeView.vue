<template>
  <div
    v-if="liveData"
    class="min-h-screen bg-[#f1f5f9] p-4 md:p-8 font-sans text-slate-900"
  >
    <SystemStatusHeader :is-connected="isConnected" :error-list="errorList" />

    <WeatherHeatingStatus :live-data="liveData" />

    <div v-if="errorList.length > 0" class="lg:hidden flex flex-col gap-2 mb-6">
      <div
        v-for="err in errorList"
        :key="err"
        class="bg-rose-500 text-white p-3 rounded-xl text-xs font-bold flex items-center gap-3"
      >
        <span>⚠️</span> {{ err }}
      </div>
    </div>

    <div class="grid grid-cols-1 md:grid-cols-12 gap-6">
      <!-- Live Energie-Bilanz (conditional nach dataSource) -->
      <div
        class="md:col-span-12 lg:col-span-8 bg-white rounded-[2.5rem] p-8 border border-white shadow-xl shadow-slate-200/60"
      >
        <div class="flex justify-between items-center mb-6">
          <span
            class="text-[10px] font-black text-slate-400 uppercase tracking-widest"
            >Live Energie-Bilanz</span
          >
          <DataSourceBadge :source="dataSource" />
        </div>

        <EnergyBalanceFronius v-if="isRichSource" :liveData="liveData" />
        <EnergyBalanceAmis v-else-if="isAmis" :liveData="liveData" />
        <EnergyBalanceFallback v-else />
      </div>

      <div v-if="isRichSource" class="md:col-span-12 lg:col-span-4">
        <EnergyBalanceBattery :liveData="liveData" />
      </div>

      <EnergyForecast :live-data="liveData" />
    </div>

    <BoilerPanel :live-data="liveData" />
  </div>
</template>

<script setup>
import { computed } from "vue";
import DataSourceBadge from "../components/DataSourceBadge.vue";
import EnergyBalanceFronius from "../components/EnergyBalanceFronius.vue";
import EnergyBalanceAmis from "../components/EnergyBalanceAmis.vue";
import EnergyBalanceFallback from "../components/EnergyBalanceFallback.vue";
import EnergyBalanceBattery from "../components/EnergyBalanceBattery.vue";

import SystemStatusHeader from "../components/home/SystemStatusHeader.vue";
import WeatherHeatingStatus from "../components/home/WeatherHeatingStatus.vue";
import EnergyForecast from "../components/home/EnergyForecast.vue";
import BoilerPanel from "../components/home/BoilerPanel.vue";

const props = defineProps({
  liveData: Object,
  isConnected: Boolean,
});

// Fehlertexte der Übersicht
const errorDefinitions = {
  1: "MicroCard",
  2: "Flash Speicher",
  4: "Modbus/Wechselrichter",
  8: "Temperatur Sensorik",
  16: "Boiler Heizung nicht aktiv",
  32: "Amis Reader",
  64: "MQTT Schnittstelle",
  128: "InfluxDB Schnittstelle",
  256: "Watt Bias AKTIV !! ",
  512: "Manuelle Heizung",
};

// --- Datenquelle aus WebSocket ---
const dataSource = computed(() => props.liveData?.dataSource ?? null);
const isRichSource = computed(() =>
  ["fronius", "modbus", "huawei", "loxone"].includes(dataSource.value),
);
const isAmis = computed(() => dataSource.value === "amis");

// Dieselbe Fehlerliste für Header und mobile Anzeige.
const errorList = computed(() => {
  // Sicherstellen, dass liveData und liveData.errors existieren
  const errorCode = props.liveData?.errors;

  if (!errorCode || isNaN(parseInt(errorCode))) return [];

  const bitvector = parseInt(errorCode);

  return Object.keys(errorDefinitions)
    .filter((bitKey) => {
      const bitValue = parseInt(bitKey);
      return (bitvector & bitValue) === bitValue;
    })
    .map((bitKey) => errorDefinitions[bitKey]);
});
</script>
