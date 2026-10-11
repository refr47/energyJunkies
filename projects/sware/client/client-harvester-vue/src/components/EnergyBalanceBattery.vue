<template>
  <div v-if="hasBattery" class="mt-6 bg-slate-900 rounded-2xl p-5 text-white shadow-2xl relative overflow-hidden">
    <div class="absolute inset-0 bg-gradient-to-br from-emerald-500/20 to-transparent"></div>

    <div class="relative z-10">
      <div class="flex justify-between items-center mb-4">
        <span class="text-[10px] font-black uppercase tracking-widest text-emerald-400">🔋 Battery Stack</span>
        <span class="text-xs font-bold text-slate-400">{{ aakPower }} W</span>
      </div>

      <div class="mb-4">
        <span class="text-5xl font-black tracking-tighter text-emerald-400">
          {{ aakStat }}<small class="text-xl text-emerald-500">%</small>
        </span>
      </div>

      <div class="h-3 bg-white/5 rounded-full p-0.5 border border-white/10 mb-3">
        <div class="h-full bg-gradient-to-r from-emerald-500 to-teal-400 rounded-full shadow-[0_0_20px_rgba(16,185,129,0.4)] transition-all duration-1000"
          :style="{ width: aakStat + '%' }">
        </div>
      </div>

      <!-- Batterieleistung mit Richtungsanzeige -->
      <div class="flex items-center justify-between mb-3">
        <span class="text-[9px] font-black uppercase text-slate-400 tracking-widest">Batterieleistung</span>
        <div class="flex items-center gap-2">
          <span v-if="isCharging" class="text-[10px] font-bold text-amber-400 flex items-center gap-1">
            ⚡<span>→</span><span>laden</span>
          </span>
          <span v-else-if="isDischarging" class="text-[10px] font-bold text-emerald-400 flex items-center gap-1">
            <span>←</span><span>→ Entladen</span>
          </span>
          <span v-else class="text-[10px] font-bold text-slate-500">Idle</span>
        </div>
      </div>

      <!-- Leistungswert groß -->
      <div class="text-center py-3">
        <span class="text-3xl font-black" :class="powerColor">
          {{ battPowerDisplay }}<small class="text-sm text-slate-400 ml-1">W</small>
        </span>
      </div>

      <!-- Kapazitätsanzeige -->
      <div class="flex justify-between text-[9px] font-black uppercase text-slate-500 tracking-widest pt-2 border-t border-white/5">
        <span>Kapazität: {{ aakPower }} kW</span>
        <span class="text-emerald-400">Verfügbar: {{ remainingKwh }} kWh</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  liveData: { type: Object, default: () => ({}) },
});

const hasBattery    = computed(() => !!props.liveData?.aakHasBattery);
const aakStat       = computed(() => Math.round(props.liveData?.aakStat ?? 0));
const aakPower      = computed(() => Math.round(props.liveData?.aakPower ?? 0));
const aakEntladen   = computed(() => Math.round(props.liveData?.aakEntladen ?? 0));

const isCharging    = computed(() => aakEntladen.value < 0);
const isDischarging = computed(() => aakEntladen.value > 0);

const battPowerDisplay = computed(() => {
  return aakEntladen.value; // positiv = entladen, negativ = laden
});

const powerColor = computed(() => {
  if (isCharging.value) return 'text-amber-400';     // laden = gelb
  if (isDischarging.value) return 'text-emerald-400'; // entladen = gruen
  return 'text-slate-400';
});

const remainingKwh = computed(() => {
  return ((aakStat.value / 100) * aakPower.value).toFixed(1);
});
</script>
