<template>
  <div v-if="hasBattery" class="mt-5 bg-white rounded-xl border border-slate-200 overflow-hidden">
    
    <!-- Kopfzeile -->
    <div class="px-5 pt-4 pb-3 flex items-center justify-between">
      <span class="text-[10px] font-bold text-slate-400 uppercase tracking-widest">Batterie-Status</span>
      <span class="text-[10px] text-slate-300 font-medium">{{ aakPower }} kW Kapazität</span>
    </div>

    <!-- Akkusymbol -->
    <div class="flex items-center justify-center gap-6 py-5">

      <!-- SVG Batterie -->
      <div class="relative">
        <svg width="120" height="36" viewBox="0 0 120 36" fill="none" xmlns="http://www.w3.org/2000/svg">
          <!-- Batterie-Plus-Kappe -->
          <rect x="107" y="13" width="6" height="10" rx="1" fill="#94a3b8"/>
          <!-- Batterie-Aussen-Frame -->
          <rect x="1" y="1" width="104" height="34" rx="5" stroke="#cbd5e1" stroke-width="2" fill="none"/>
          <!-- Fill-Zellen (4 Segmente) -->
          <rect x="5" y="5" :width="cellWidth" height="26" rx="3" :fill="fillColor"/>
          <!-- Trennstriche -->
          <line x1="36" y1="5" x2="36" y2="31" stroke="rgba(255,255,255,0.3)" stroke-width="1"/>
          <line x1="72" y1="5" x2="72" y2="31" stroke="rgba(255,255,255,0.3)" stroke-width="1"/>
        </svg>
        <!-- SOC ueber Batterie -->
        <div class="text-center mt-2">
          <span class="text-2xl font-black" :class="socColor">{{ aakStat }}%</span>
        </div>
      </div>

      <!-- Leistungsinfo -->
      <div class="space-y-3">
        <!-- Ladestatus -->
        <div class="flex items-center gap-2">
          <span class="w-2 h-2 rounded-full" :class="statusDot"></span>
          <span class="text-xs font-bold text-slate-500">{{ statusText }}</span>
        </div>
        <!-- Leistung -->
        <div>
          <p class="text-[9px] font-bold text-slate-400 uppercase">Leistung</p>
          <p class="text-2xl font-black" :class="pwrColor">
            {{ battPowerDisplay }}<span class="text-xs text-slate-300 font-bold ml-0.5">W</span>
          </p>
        </div>
        <!-- Verfuegbar -->
        <div class="pt-2 border-t border-slate-100">
          <p class="text-[9px] font-bold text-slate-400 uppercase">Verfuegbar</p>
          <p class="text-sm font-bold text-slate-600">{{ remainingKwh }} <span class="text-xs text-slate-400">kWh</span></p>
        </div>
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

// ├── p_batt: negative = laden → aakEntladen = -p_batt > 0 bei Laden
const isCharging    = computed(() => aakEntladen.value > 0);
const isDischarging = computed(() => aakEntladen.value < 0);

// ── Akkufuellstand: Breite + Farbe ──
const cellWidth = computed(() => {
  // 96 von 104 Pixel total, proportional zum SOC
  return (aakStat.value / 100) * 96;
});

const fillColor = computed(() => {
  if (aakStat.value >= 70) return '#10b981';     // emerald-500
  if (aakStat.value >= 35) return '#f59e0b';     // amber-500
  return '#ef4444';                              // red-500
});

const socColor = computed(() => {
  if (aakStat.value >= 70) return 'text-emerald-500';
  if (aakStat.value >= 35) return 'text-amber-500';
  return 'text-red-500';
});

// ── Leistungsanzeige ──
const battPowerDisplay = computed(() => aakEntladen.value);

const pwrColor = computed(() => {
  if (isCharging.value) return 'text-blue-600';       // laden = pos = BLAU
  if (isDischarging.value) return 'text-red-600';     // entladen = neg = ROT
  return 'text-slate-400';
});

// ── Status-Label ──
const statusText = computed(() => {
  if (isCharging.value) return 'Laden';
  if (isDischarging.value) return 'Entladen';
  return 'Idle';
});

const statusDot = computed(() => {
  if (isCharging.value) return 'bg-red-400';
  if (isDischarging.value) return 'bg-blue-400';
  return 'bg-slate-300';
});

const remainingKwh = computed(() =>
  ((aakStat.value / 100) * aakPower.value).toFixed(1)
);
</script>
