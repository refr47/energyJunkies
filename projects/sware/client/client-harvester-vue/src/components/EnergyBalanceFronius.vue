<template>
  <div class="h-full">
    <!-- PV → Hauslast → Netz als fließendes Diagramm -->
    <div class="grid grid-cols-3 gap-3 mb-6">
      <!-- PV -->
      <div class="bg-gradient-to-br from-amber-50 to-orange-50 rounded-2xl p-4 border border-amber-100 text-center shadow-sm">
        <div class="text-2xl mb-1">☀️</div>
        <p class="text-[9px] font-black text-amber-600 uppercase tracking-widest">PV-Ertrag</p>
        <p class="text-2xl font-black text-slate-800 mt-1">
          {{ pv }}<small class="text-xs ml-0.5 text-slate-400">W</small>
        </p>
      </div>

      <!-- Hauslast -->
      <div class="bg-gradient-to-br from-slate-50 to-slate-100 rounded-2xl p-4 border border-slate-200 text-center shadow-sm">
        <div class="text-2xl mb-1">🏠</div>
        <p class="text-[9px] font-black text-slate-500 uppercase tracking-widest">Hauslast</p>
        <p class="text-2xl font-black text-slate-800 mt-1">
          {{ hauslast }}<small class="text-xs ml-0.5 text-slate-400">W</small>
        </p>
      </div>

      <!-- Netz -->
      <div
        class="rounded-2xl p-4 border text-center shadow-sm transition-colors duration-500"
        :class="isExport ? 'bg-gradient-to-br from-blue-50 to-cyan-50 border-blue-200' : 'bg-gradient-to-br from-rose-50 to-red-50 border-rose-200'"
      >
        <div class="text-2xl mb-1">{{ isExport ? '🔵' : '🔴' }}</div>
        <p class="text-[9px] font-black uppercase tracking-widest"
           :class="isExport ? 'text-blue-600' : 'text-rose-600'">
          {{ isExport ? 'Einspeisung' : 'Netzbezug' }}
        </p>
        <p class="text-2xl font-black mt-1"
           :class="isExport ? 'text-blue-700' : 'text-rose-700'">
          {{ netzAbs }}<small class="text-xs ml-0.5 text-slate-400">W</small>
        </p>
      </div>
    </div>

    <!-- Energiefluss-Pfeile als visuelle Verbindung -->
    <div class="flex items-center justify-center gap-2 mb-6 text-slate-300">
      <svg class="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 7l5 5m0 0l-5 5m5-5H6" /></svg>
      <span class="text-[9px] font-black uppercase tracking-widest text-slate-400">Energiefluss</span>
      <svg class="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11 17l-5-5m0 0l5-5m-5 5h12" /></svg>
    </div>

    <!-- Netz-Saldo Box -->
    <div class="bg-slate-50 rounded-2xl p-5 border border-slate-100 text-center shadow-inner mb-6">
      <p class="text-[9px] font-black text-slate-400 uppercase mb-1">Netz-Saldo</p>
      <p class="text-3xl font-black transition-colors duration-500"
         :class="netzaenderung <= 0 ? 'text-blue-600' : 'text-rose-600'">
        {{ netzaenderung >= 0 ? netzaenderung : '' }}{{ Math.abs(netzaenderung) }}<small class="text-sm ml-1 text-slate-400">W</small>
      </p>

      <div class="mt-4 flex justify-center gap-1.5">
        <span v-for="i in 3" :key="i" class="w-2 h-2 rounded-full"
          :class="netzaenderung <= 0 ? 'bg-blue-400 animate-bounce' : 'bg-rose-400 animate-bounce'">
        </span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  liveData: { type: Object, default: () => ({}) },
});

const pv       = computed(() => Math.round(props.liveData?.solEr ?? 0));
const hauslast = computed(() => Math.round(props.liveData?.ev ?? 0));
const netzaenderung = computed(() => Math.round(props.liveData?.netzBezug ?? 0));
const netzAbs    = computed(() => Math.abs(netzaenderung.value));
const isExport   = computed(() => netzaenderung.value <= 0);
</script>
