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

      <div class="h-3 bg-white/5 rounded-full p-0.5 border border-white/10 mb-2">
        <div class="h-full bg-gradient-to-r from-emerald-500 to-teal-400 rounded-full shadow-[0_0_20px_rgba(16,185,129,0.4)] transition-all duration-1000"
          :style="{ width: aakStat + '%' }">
        </div>
      </div>

      <div class="flex justify-between text-[9px] font-black uppercase text-slate-500 tracking-widest">
        <span>Kapazität</span>
        <span class="text-emerald-400">{{ aakEntladen > 0 ? 'Charging' : 'Idle' }}</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  liveData: { type: Object, default: () => ({}) },
});

const hasBattery  = computed(() => !!props.liveData?.aakHasBattery);
const aakStat     = computed(() => props.liveData?.aakStat ?? 0);
const aakPower    = computed(() => props.liveData?.aakPower ?? 0);
const aakEntladen = computed(() => props.liveData?.aakEntladen ?? 0);
</script>
