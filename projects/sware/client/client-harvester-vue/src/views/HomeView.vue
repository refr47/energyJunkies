<template>
  <div class="min-h-screen bg-slate-50 p-4 md:p-8 font-sans text-slate-900">

    <div class="flex flex-col md:flex-row justify-between items-start md:items-center mb-8 gap-4">
      <div>
        <h1 class="text-3xl font-light tracking-tight text-slate-700">Energie-Zentrale</h1>
        <p class="text-slate-400 text-sm">E-Harvester Control v3.0</p>
      </div>

      <div class="flex gap-2">
        <div :class="[isConnected ? 'bg-emerald-100 text-emerald-700' : 'bg-rose-100 text-rose-700']"
          class="px-4 py-1.5 rounded-full text-xs font-bold uppercase tracking-wider flex items-center gap-2 shadow-sm">
          <span :class="[isConnected ? 'bg-emerald-500' : 'bg-rose-500']"
            class="w-2 h-2 rounded-full animate-pulse"></span>
          {{ isConnected ? 'Verbunden' : 'Getrennt' }}
        </div>
      </div>
    </div>

    <transition name="fade">
      <div v-if="hasErrors" class="mb-8 bg-rose-50 border-l-4 border-rose-500 p-4 rounded-r-lg shadow-md">
        <div class="flex items-center gap-3 text-rose-800 font-bold mb-2">
          <span class="text-xl">⚠️</span>
          <h3>Systemkritische Fehler</h3>
        </div>
        <ul class="text-rose-700 text-sm space-y-1 ml-8 list-disc">
          <li v-for="(err, index) in errorList" :key="index">{{ err }}</li>
        </ul>
      </div>
    </transition>

    <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">

      <div class="bg-white rounded-2xl shadow-sm border border-slate-100 p-6 transition-all hover:shadow-md">
        <div class="flex justify-between items-start mb-4">
          <span class="text-slate-400 text-xs font-bold uppercase tracking-widest">Netzanschluss</span>
          <div :class="isExporting ? 'text-emerald-500' : 'text-rose-500'">
            <svg v-if="isExporting" class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path d="M13 7h8m0 0v8m0-8l-8 8-4-4-6 6"></path>
            </svg>
            <svg v-else class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path d="M13 17h8m0 0v-8m0 8l-8-8-4 4-6-6"></path>
            </svg>
          </div>
        </div>
        <div class="text-center py-4">
          <span class="text-5xl font-black tracking-tighter"
            :class="isExporting ? 'text-emerald-600' : 'text-rose-600'">
            {{ Math.abs(liveData.EINS || 0).toLocaleString('de-DE') }}<small class="text-xl ml-1">W</small>
          </span>
          <p class="text-slate-400 mt-2 font-medium">{{ isExporting ? 'Einspeisung' : 'Netzbezug' }}</p>
        </div>
        <div class="w-full bg-slate-100 h-1.5 rounded-full mt-4 overflow-hidden">
          <div :class="isExporting ? 'bg-emerald-500' : 'bg-rose-500'" class="h-full transition-all duration-1000"
            :style="{ width: energyFlowPercentage + '%' }"></div>
        </div>
      </div>

      <div class="bg-white rounded-2xl shadow-sm border border-slate-100 p-6">
        <span class="text-slate-400 text-xs font-bold uppercase tracking-widest block mb-4">Wasserspeicher</span>
        <div class="flex items-center gap-8">
          <div
            class="relative w-20 h-32 bg-slate-100 rounded-2xl overflow-hidden border-4 border-slate-50 shadow-inner">
            <div
              class="absolute bottom-0 w-full bg-gradient-to-t from-blue-600 to-cyan-400 transition-all duration-1000"
              :style="{ height: liveData.TPS + '%' }"></div>
            <div class="absolute inset-0 flex items-center justify-center">
              <span class="text-xl font-bold text-slate-700 drop-shadow-sm">{{ liveData.TPS || 0 }}°</span>
            </div>
          </div>
          <div class="flex-1 space-y-3">
            <div class="flex justify-between items-center bg-slate-50 p-2 rounded-lg">
              <span class="text-xs font-bold text-slate-500">PHASE L1</span>
              <span :class="liveData.L1 ? 'bg-amber-400 text-white' : 'bg-slate-200 text-slate-400'"
                class="text-[10px] px-2 py-0.5 rounded">ACTIVE</span>
            </div>
            <div class="flex justify-between items-center bg-slate-50 p-2 rounded-lg">
              <span class="text-xs font-bold text-slate-500">PHASE L2</span>
              <span :class="liveData.L2 ? 'bg-amber-400 text-white' : 'bg-slate-200 text-slate-400'"
                class="text-[10px] px-2 py-0.5 rounded">ACTIVE</span>
            </div>
            <div class="bg-slate-50 p-2 rounded-lg">
              <div class="flex justify-between items-center mb-1">
                <span class="text-xs font-bold text-slate-500">PWM CONTROL</span>
                <span class="text-xs font-mono text-blue-600">{{ liveData.L3 || 0 }}%</span>
              </div>
              <div class="w-full bg-slate-200 h-1 rounded-full overflow-hidden">
                <div class="bg-blue-500 h-full" :style="{ width: liveData.L3 + '%' }"></div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <div class="bg-slate-800 rounded-2xl shadow-xl p-6 text-white">
        <span class="text-slate-400 text-xs font-bold uppercase tracking-widest block mb-6">Leistungsbilanz</span>
        <div class="space-y-6">
          <div class="flex justify-between items-end">
            <div>
              <p class="text-slate-400 text-xs mb-1">Solar-Ertrag</p>
              <p class="text-2xl font-bold text-emerald-400">{{ (liveData.PR || 0).toLocaleString('de-DE') }} W</p>
            </div>
            <div class="h-10 w-1 bg-emerald-500/20 rounded-full overflow-hidden">
              <div class="w-full bg-emerald-400" :style="{ height: '60%' }"></div>
            </div>
          </div>
          <div class="flex justify-between items-end">
            <div>
              <p class="text-slate-400 text-xs mb-1">Eigenverbrauch</p>
              <p class="text-2xl font-bold text-rose-400">{{ (liveData.EV || 0).toLocaleString('de-DE') }} W</p>
            </div>
            <div class="h-10 w-1 bg-rose-500/20 rounded-full overflow-hidden">
              <div class="w-full bg-rose-400" :style="{ height: '40%' }"></div>
            </div>
          </div>
          <div v-if="liveData.AKKU_AVAIL" class="pt-4 border-t border-slate-700 flex justify-between items-center">
            <span class="text-sm text-slate-400">Hausbatterie</span>
            <span class="text-lg font-mono tracking-widest">{{ liveData.AkStat || 0 }}%</span>
          </div>
        </div>
      </div>

    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps(['liveData', 'logs', 'isConnected']);

const isExporting = computed(() => props.liveData.EINS <= 0);

const energyFlowPercentage = computed(() => {
  const maxW = 6000; // Skalierung bis 6kW
  return Math.min(100, (Math.abs(props.liveData.EINS || 0) / maxW) * 100);
});

const errorList = computed(() => {
  const v = props.liveData.FE || 0;
  const errs = [];
  if (v & (1 << 4)) errs.push("SD-Speicherfehler erkannt");
  if (v & (1 << 5)) errs.push("Kommunikation zum Wechselrichter gestört");
  if (v & (1 << 7)) errs.push("Temperatursensor liefert unplausible Werte");
  return errs;
});
const hasErrors = computed(() => errorList.value.length > 0);
</script>