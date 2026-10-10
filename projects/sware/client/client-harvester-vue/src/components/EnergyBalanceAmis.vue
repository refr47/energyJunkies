<template>
  <div class="h-full">
    <!-- Saldo als Großanzeige zentral -->
    <div class="bg-white rounded-2xl border border-slate-200 p-6 text-center mb-6 shadow-sm">
      <p class="text-[9px] font-black text-slate-400 uppercase tracking-widest mb-2">Momentaner Saldo</p>

      <div class="relative w-32 h-32 mx-auto mb-3">
        <svg class="w-32 h-32 transform -rotate-90" viewBox="0 0 120 120">
          <circle cx="60" cy="60" r="52" stroke="currentColor" stroke-width="8" fill="transparent" class="text-slate-100" />
          <circle cx="60" cy="60" r="52" stroke="currentColor" stroke-width="8" fill="transparent"
            :class="isExport ? 'text-emerald-500' : 'text-rose-500'"
            stroke-linecap="round"
            :stroke-dasharray="327"
            :stroke-dashoffset="327 - (327 * saldoAbs / 5) / 100"
            class="transition-all duration-1000 ease-out"
          />
        </svg>
        <div class="absolute inset-0 flex flex-col items-center justify-center">
          <span class="text-2xl font-black" :class="isExport ? 'text-emerald-600' : 'text-rose-600'">
            {{ saldo >= 0 ? '+' : '' }}{{ Math.round(saldo) }}<small class="text-xs text-slate-400">W</small>
          </span>
          <span class="text-[9px] font-black uppercase text-slate-400 mt-1"
            :class="isExport ? 'text-emerald-500' : 'text-rose-500'">
            {{ isExport ? 'Einspeisung' : 'Bezug' }}
          </span>
        </div>
      </div>

      <div class="flex justify-center gap-1.5">
        <span v-for="i in 3" :key="i" class="w-2 h-2 rounded-full animate-bounce"
          :class="isExport ? 'bg-emerald-400' : 'bg-rose-400'"></span>
      </div>
    </div>

    <!-- KumulierteExport / Import als Vergleich -->
    <div class="grid grid-cols-2 gap-4 mb-6">
      <div class="bg-gradient-to-br from-blue-50 to-cyan-50 rounded-2xl p-4 border border-blue-100 text-center shadow-sm">
        <div class="text-lg mb-1">📤</div>
        <p class="text-[9px] font-black text-blue-600 uppercase tracking-widest">Export Gesamt</p>
        <p class="text-xl font-black text-blue-700 mt-1">
          {{ Math.round(exportKwh) }}<small class="text-xs text-slate-400 ml-0.5">kWh</small>
        </p>
      </div>

      <div class="bg-gradient-to-br from-rose-50 to-red-50 rounded-2xl p-4 border border-rose-100 text-center shadow-sm">
        <div class="text-lg mb-1">📥</div>
        <p class="text-[9px] font-black text-rose-600 uppercase tracking-widest">Import Gesamt</p>
        <p class="text-xl font-black text-rose-700 mt-1">
          {{ Math.round(importKwh) }}<small class="text-xs text-slate-400 ml-0.5">kWh</small>
        </p>
      </div>
    </div>

    <!-- Hinweis: keine PV-Hauslast-Aufteilung -->
    <div class="bg-amber-50 rounded-2xl p-4 border border-amber-200 text-center shadow-sm">
      <div class="text-lg mb-1">📊</div>
      <p class="text-[10px] font-black text-amber-700 uppercase tracking-widest">AMIS Smartmeter</p>
      <p class="text-[9px] text-amber-600 mt-1">
        PV-Hauslast-Aufteilung nicht verfügbar – nur Saldo-Daten vom Zähler
      </p>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  liveData: { type: Object, default: () => ({}) },
});

const saldo     = computed(() => Math.round(props.liveData?.ev ?? 0));
const saldoAbs  = computed(() => Math.min(100, Math.abs(saldo.value) / 5)); // Skalierung
const isExport  = computed(() => saldo.value <= 0);
const exportKwh = computed(() => props.liveData?.SEI ?? 0);
const importKwh = computed(() => props.liveData?.SII ?? 0);
</script>
