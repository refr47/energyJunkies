<template>
  <div class="min-h-screen bg-slate-50 p-4 md:p-8 font-sans text-slate-900">

    <div class="flex flex-col md:flex-row justify-between items-start md:items-center mb-8 gap-4">
      <div>
        <h1 class="text-3xl font-light tracking-tight text-slate-700">Energie-Zentrale</h1>
        <p class="text-slate-400 text-sm">E-Harvester Control v3.0</p>
      </div>
      <div class="flex gap-2">
        <div :class="[isConnected ? 'bg-emerald-100 text-emerald-700' : 'bg-rose-100 text-rose-700']"
          class="px-4 py-1.5 rounded-full text-xs font-bold uppercase tracking-wider flex items-center gap-2 shadow-sm border border-white">
          <span :class="[isConnected ? 'bg-emerald-500' : 'bg-rose-500']"
            class="w-2 h-2 rounded-full animate-pulse"></span>
          {{ isConnected ? 'Verbunden' : 'Getrennt' }}
        </div>
      </div>
    </div>

    <transition name="fade">
      <div v-if="hasErrors" class="mb-8 bg-rose-500 text-white p-2 rounded-2xl shadow-lg border-b-4 border-rose-700">
        <div class="flex items-center gap-3 font-black mb-3">
          <span class="text-2xl">⚠️</span>
          <h3 class="uppercase tracking-tighter">System-Alarm</h3>
        </div>
        <div class="grid grid-cols-1 md:grid-cols-2 gap-2">
          <div v-for="(err, index) in errorList" :key="index"
            class="bg-rose-600/50 px-3 py-2 rounded-lg flex items-center gap-2 text-sm border border-rose-400/30">
            <span class="w-1.5 h-1.5 bg-white rounded-full"></span>
            {{ err }}
          </div>
        </div>
      </div>
    </transition>

    <div class="grid grid-cols-1 md:grid-cols-2 gap-6" :class="hasBattery ? 'lg:grid-cols-3' : 'lg:grid-cols-2'">

      <div class="bg-white rounded-2xl shadow-sm border border-slate-100 p-6">
        <span class="text-slate-400 text-xs font-bold uppercase tracking-widest block mb-4">Netzanschluss</span>
        <div class="text-center py-4">
          <span class="text-5xl font-black tracking-tighter"
            :class="isExporting ? 'text-emerald-600' : 'text-rose-600'">
            {{ Math.abs(liveData.netzBezug || 0).toLocaleString('de-DE') }}<small class="text-xl ml-1">W</small>
          </span>
          <p class="text-slate-400 mt-2 font-medium">{{ isExporting ? 'Einspeisung' : 'Netzbezug' }}</p>
        </div>
      </div>

      <div class="bg-white rounded-2xl shadow-sm border border-slate-100 p-6">
        <span class="text-slate-400 text-xs font-bold uppercase tracking-widest block mb-4">Boiler</span>
        <div class="flex justify-between items-start mb-4">
          <span class="text-slate-400 text-xs font-bold uppercase tracking-widest">Temperatur</span>

          <div :class="chargeMode.class"
            class="px-2 py-1 rounded text-[10px] font-black tracking-tighter transition-all">
            {{ chargeMode.label }}
          </div>
        </div>

        <div class="flex items-center gap-6">
          <div class="relative w-16 h-28 bg-slate-100 rounded-xl overflow-hidden border-2 border-slate-50">
            <div class="absolute bottom-0 w-full bg-blue-500 transition-all duration-1000"
              :style="{ height: liveData.bTemp + '%' }"></div>
            <div class="absolute inset-0 flex items-center justify-center font-bold text-slate-700">{{ liveData.bTemp }}°
            </div>
          </div>
          <div class="flex-1 space-y-2 text-xs font-bold text-slate-500">
            <div class="flex justify-between bg-slate-50 p-2 rounded"><span>Phase L1</span><span
                :class="liveData.L1 ? 'text-amber-500' : 'text-slate-300'">●</span></div>
            <div class="flex justify-between bg-slate-50 p-2 rounded"><span>Phase L2</span><span
                :class="liveData.L2 ? 'text-amber-500' : 'text-slate-300'">●</span></div>
            <div class="flex justify-between bg-slate-50 p-2 rounded"><span>PWM L3</span><span class="text-blue-500">{{
                liveData.L3 }}%</span></div>
          </div>
        </div>
      </div>

      <div v-if="hasBattery" class="bg-emerald-600 rounded-2xl shadow-xl p-6 text-white relative overflow-hidden">
        <div class="absolute -right-4 -top-4 w-24 h-24 bg-white/10 rounded-full blur-2xl"></div>

        <span class="text-emerald-100 text-xs font-bold uppercase tracking-widest block mb-6">AKKU</span>
        <div class="flex flex-col items-center justify-center py-2">
          <div class="relative w-full h-16 bg-emerald-800/50 rounded-xl border-2 border-emerald-400/30 p-1.5 mb-4">
            <div class="h-full bg-white rounded-lg shadow-[0_0_15px_rgba(255,255,255,0.4)] transition-all duration-1000"
              :style="{ width: liveData.AkStat + '%' }"></div>
            <div class="absolute inset-0 flex items-center justify-center font-black text-xl mix-blend-difference">
              {{ liveData.aakStat }}%
            </div>
          </div>
          <div class="grid grid-cols-2 w-full gap-4 text-center">
            <div class="bg-emerald-700/50 rounded-lg p-2">
              <p class="text-[10px] text-emerald-200 uppercase">Leistung</p>
              <p class="text-lg font-bold">{{ liveData.aakPower || 0 }} W</p>
            </div>
            <div class="bg-emerald-700/50 rounded-lg p-2">
              <p class="text-[10px] text-emerald-200 uppercase">Status</p>
              <p class="text-xs font-bold">{{ (liveData.aakEntladen || 0) > 0 ? 'Laden' : 'Entladen' }}</p>
            </div>
          </div>
        </div>
      </div>

      <div class="bg-slate-400 rounded-2xl shadow-xl p-6 text-white transition-all duration-500" :class="[
        // Wenn kein Akku da ist, nimm auf großen Bildschirmen 2 Spalten ein
        !hasBattery ? 'md:col-span-2 lg:col-span-2' : '',
        // Wenn ein Akku da ist, aber das Fenster so schmal wird, dass die Bilanz 
        // in die nächste Zeile rutscht (Desktop), zentrieren wir sie optional:
        hasBattery ? 'lg:col-span-1' : ''
      ]">
        <span class="text-slate-400 text-xs font-bold uppercase tracking-widest block mb-6">Leistungsbilanz</span>
        <div class="grid grid-cols-1" :class="!hasBattery ? 'md:grid-cols-2 gap-8' : 'space-y-6'">
          <div class="flex justify-between items-end border-b border-slate-200 pb-4">
            <div>
              <p class="text-slate-400 text-xs mb-1">Solar-Ertrag</p>
              <p class="text-3xl font-bold text-emerald-400">{{ (liveData.solEr || 0).toLocaleString('de-DE') }} <small
                  class="text-sm">W</small></p>
            </div>
          </div>
          <div class="flex justify-between items-end border-b border-slate-700 pb-4">
            <div>
              <p class="text-slate-400 text-xs mb-1">Eigenverbrauch</p>
              <p class="text-3xl font-bold text-rose-400">{{ (liveData.ev || 0).toLocaleString('de-DE') }} <small
                  class="text-sm">W</small></p>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const errorDefinitions = {
  1: "Überspannung PV-Eingang",
  2: "Unterspannung Batterie",
  4: "Übertemperatur Wechselrichter",
  8: "Kommunikationsfehler BMS",
  16: "Isolationsfehler",
  32: "Not-Aus betätigt"
};

const props = defineProps(['liveData', 'logs', 'isConnected']);

const isExporting = computed(() => props.liveData.EINS <= 0);
const hasBattery = computed(() => props.liveData.config?.hasBattery);
const errorVektor = computed(() => props.liveData.errors > 0);

console.log("LiveData in HomeView:", props.liveData);


// In HomeView.vue <script setup>
const chargeMode = computed(() => {
  // 0 = Standby/Aus, 1 = Überschuss (Normal), 2 = Force (Erzwungen)
  const mode = props.liveData?.netzBezug || 0;

  if (mode === 2) {
    return {
      label: 'FORCE LOAD',
      class: 'bg-rose-500 text-white animate-pulse',
      desc: 'Netzbezug aktiv'
    };
  }
  if (mode === 1) {
    return {
      label: 'SOLAR ONLY',
      class: 'bg-emerald-500 text-white',
      desc: 'Nur Überschuss'
    };
  }
  return {
    label: 'STANDBY',
    class: 'bg-slate-200 text-slate-500',
    desc: 'Wartet auf Solarstrom'
  };
});

const errorList = computed(() => {
  // 1. Sicherstellen, dass props.liveData existiert, sonst leere Liste
  const data = errorVektor.value ? props.liveData.errors : null;
  //console.log("Fehlervektor:", data);
  if (!data === undefined) return [];
  const bitvector = parseInt(data);
  if (isNaN(bitvector) || bitvector === 0) return [];
  return Object.keys(errorDefinitions)
    .filter(bitKey => {
      const bitValue = parseInt(bitKey);
      return (bitvector & bitValue) === bitValue;
    })
    .map(bitKey => errorDefinitions[bitKey]);
});

const hasErrors = computed(() => errorList.value.length > 0);

</script>