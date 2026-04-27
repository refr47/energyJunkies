<template>
  <div v-if="liveData" class="min-h-screen bg-[#f8fafc] p-4 md:p-6 font-sans text-slate-900">

    <header class="flex justify-between items-center mb-6">
      <div class="flex items-center gap-3">
        <div class="bg-blue-600 p-2 rounded-lg text-white shadow-lg">⚡</div>
        <div>
          <h1 class="text-xl font-bold tracking-tight text-slate-800">Energie-Zentrale</h1>
          <p class="text-slate-400 text-[10px] uppercase font-bold tracking-widest">System Live</p>
        </div>
      </div>
      <div :class="[isConnected ? 'bg-emerald-50 text-emerald-600' : 'bg-rose-50 text-rose-600']"
        class="px-3 py-1 rounded-full text-[10px] font-black border border-current flex items-center gap-2">
        <span class="w-1.5 h-1.5 rounded-full bg-current animate-pulse"></span>
        {{ isConnected ? 'ONLINE' : 'OFFLINE' }}
      </div>
    </header>

    <div v-if="hasErrors" class="mb-6 overflow-hidden rounded-xl bg-rose-500 text-white shadow-md">
      <div class="flex items-center gap-3 px-4 py-2 bg-rose-600 text-[10px] font-black uppercase">
        <span>⚠️ System-Alarm</span>
      </div>
      <div class="p-3 text-xs grid grid-cols-1 md:grid-cols-3 gap-2">
        <div v-for="err in errorList" :key="err" class="flex items-center gap-2">
          <span class="w-1 h-1 bg-white rounded-full"></span> {{ err }}
        </div>
      </div>
    </div>

    <div class="grid grid-cols-1 md:grid-cols-12 gap-4">

      <div class="md:col-span-8 bg-white rounded-2xl shadow-sm border border-slate-100 p-4">
        <div class="text-[10px] font-black text-slate-400 uppercase tracking-widest mb-4">Leistungsbilanz</div>
        <div class="grid grid-cols-3 gap-2 text-center">
          <div class="p-2">
            <p class="text-xs text-slate-400 mb-1">Solar</p>
            <p class="text-2xl font-black text-emerald-500">{{ liveData.solEr }}<small class="text-xs ml-0.5">W</small>
            </p>
          </div>
          <div class="p-2 border-x border-slate-100">
            <p class="text-xs text-slate-400 mb-1">{{ isExporting ? 'Einspeisung' : 'Bezug' }}</p>
            <p class="text-2xl font-black" :class="isExporting ? 'text-blue-500' : 'text-rose-500'">
              {{ Math.abs(liveData.netzBezug) }}<small class="text-xs ml-0.5">W</small>
            </p>
          </div>
          <div class="p-2">
            <p class="text-xs text-slate-400 mb-1">Haus</p>
            <p class="text-2xl font-black text-slate-700">{{ liveData.ev }}<small class="text-xs ml-0.5">W</small></p>
          </div>
        </div>
      </div>

      <div v-if="hasBattery" class="md:col-span-4 bg-emerald-600 rounded-2xl shadow-lg p-4 text-white relative">
        <div class="flex justify-between items-center mb-4">
          <span class="text-[10px] font-black uppercase tracking-widest opacity-80">Speicher</span>
          <span class="text-xs font-bold">{{ liveData.aakPower }} W</span>
        </div>
        <div class="relative pt-1">
          <div class="flex items-center justify-between mb-2">
            <div class="text-2xl font-black">{{ liveData.aakStat }}%</div>
            <div class="text-[10px] uppercase font-bold bg-emerald-500 px-2 py-0.5 rounded">{{ (liveData.aakEntladen >
              0) ? 'Laden' : 'Entladen' }}</div>
          </div>
          <div class="overflow-hidden h-2 text-xs flex rounded bg-emerald-800">
            <div :style="{ width: liveData.aakStat + '%' }"
              class="shadow-none flex flex-col text-center whitespace-nowrap text-white justify-center bg-white transition-all duration-1000">
            </div>
          </div>
        </div>
      </div>

      <div class="md:col-span-12 bg-white rounded-2xl shadow-sm border border-slate-100 p-4">
        <div class="flex justify-between items-center mb-6">
          <div class="text-[10px] font-black text-slate-400 uppercase tracking-widest">Boiler Management</div>
          <div :class="chargeMode.class" class="px-2 py-0.5 rounded text-[10px] font-black uppercase">{{
            chargeMode.label }}</div>
        </div>

        <div class="grid grid-cols-1 md:grid-cols-4 gap-6 items-center">
          <div class="flex flex-col items-center border-r border-slate-50">
            <div class="text-4xl font-black text-slate-800">{{ liveData.bTemp }}°</div>
            <div class="text-[10px] text-slate-400 font-bold uppercase mt-1">Temperatur</div>
          </div>

          <div class="md:col-span-3 grid grid-cols-3 gap-4">
            <div v-for="i in [1, 2]" :key="i"
              class="flex flex-col items-center p-3 rounded-xl border border-slate-50 transition-all"
              :class="liveData['L' + i] ? 'bg-amber-50 border-amber-100' : 'bg-slate-50'">
              <span class="text-[10px] font-bold text-slate-400 mb-2">Phase {{ i }}</span>
              <div :class="liveData['L' + i] ? 'bg-amber-500 shadow-[0_0_10px_rgba(245,158,11,0.5)]' : 'bg-slate-200'"
                class="w-3 h-3 rounded-full"></div>
              <span class="text-[10px] font-black mt-2" :class="liveData['L' + i] ? 'text-amber-700' : 'text-slate-300'">
                {{ liveData['L' + i] ? '2000W' : 'OFF' }}
              </span>
            </div>

            <div class="flex flex-col items-center p-3 rounded-xl bg-blue-50 border border-blue-100">
              <span class="text-[10px] font-bold text-blue-400 mb-2">Regelung (L3)</span>
              <div class="w-full bg-blue-200 h-1.5 rounded-full overflow-hidden mt-1">
                <div class="bg-blue-500 h-full transition-all" :style="{ width: liveData.L3 + '%' }"></div>
              </div>
              <span class="text-[10px] font-black text-blue-700 mt-2">{{ liveData.L3 }}%</span>
            </div>
          </div>
        </div>
      </div>

    </div>
  </div>
</template>

<script setup>
import { computed, onMounted } from 'vue';

const props = defineProps(['liveData', 'logs', 'isConnected']);

// 1. Definiere die Fehlertexte
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
  512: "Manuelle Heizung"

}


// 2. Computed Properties mit "Optional Chaining" (?.) absichern
const isExporting = computed(() => (props.liveData?.netzBezug || 0) <= 0);
const hasBattery = computed(() => !!props.liveData?.aakHasBattery); // !! erzwingt Boolean

// 3. Fehlerliste berechnen
const errorList = computed(() => {
  // Sicherstellen, dass liveData und liveData.errors existieren
  const errorCode = props.liveData?.errors;

  if (!errorCode || isNaN(parseInt(errorCode))) return [];

  const bitvector = parseInt(errorCode);

  return Object.keys(errorDefinitions)
    .filter(bitKey => {
      const bitValue = parseInt(bitKey);
      return (bitvector & bitValue) === bitValue;
    })
    .map(bitKey => errorDefinitions[bitKey]);
});

const hasErrors = computed(() => errorList.value.length > 0);

// Lademodus Logik
const chargeMode = computed(() => {
  const mode = props.liveData?.forceHeizung || 0;
  if (mode === 2) return { label: 'FORCE LOAD', class: 'bg-rose-500 text-white animate-pulse' };
  if (mode === 1) return { label: 'SOLAR ONLY', class: 'bg-emerald-500 text-white' };
  return { label: 'STANDBY', class: 'bg-slate-200 text-slate-500' };
});
onMounted(() => {
  console.log("Props Check:");
  console.log("liveData vorhanden:", !!props.liveData);
  console.log("logs vorhanden:", !!props.logs);
  console.log("isConnected Status:", props.isConnected);

  if (props.liveData === undefined) {
    console.error("KRITISCH: liveData wurde nicht an HomeView übergeben!");
  }
});
</script>