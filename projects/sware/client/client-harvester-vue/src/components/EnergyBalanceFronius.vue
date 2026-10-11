<template>
  <div class="h-full">
    <!-- 3-Spalten: PV | Haus | Netz -->
    <div class="grid grid-cols-3 gap-3 mb-5">

      <!-- PV-Ertrag -->
      <div class="bg-white rounded-xl p-4 border border-slate-200 text-center">
        <p class="text-[9px] font-bold text-slate-400 uppercase tracking-wider mb-3">PV-Ertrag</p>
        <p class="text-3xl font-black text-red-600">
          {{ pv }}<span class="text-sm font-bold text-slate-300 ml-0.5">W</span>
        </p>
        <p class="text-[9px] text-slate-300 mt-2">produziert</p>
      </div>

      <!-- Hauslast -->
      <div class="bg-white rounded-xl p-4 border border-slate-200 text-center">
        <p class="text-[9px] font-bold text-slate-400 uppercase tracking-wider mb-3">Hauslast</p>
        <p class="text-3xl font-black text-red-600">
          {{ hauslast }}<span class="text-sm font-bold text-slate-300 ml-0.5">W</span>
        </p>
        <p class="text-[9px] text-slate-300 mt-2">verbraucht</p>
      </div>

      <!-- Netzbezug / Einspeisung -->
      <div class="bg-white rounded-xl p-4 border text-center" :class="borderColor">
        <p class="text-[9px] font-bold uppercase tracking-wider mb-3" :class="labelColor">{{ netzLabel }}</p>
        <p class="text-3xl font-black" :class="wertColor">
          {{ netzaenderung >= 0 ? '+' : ''}}{{ netzaenderung }}<span class="text-sm font-bold text-slate-300 ml-0.5">W</span>
        </p>
        <p class="text-[9px] text-slate-300 mt-2">Netz-Saldo</p>
      </div>

    </div>

    <!-- Energietabelle: klare Übersicht -->
    <div class="bg-white rounded-xl border border-slate-200 overflow-hidden">
      <table class="w-full text-sm">
        <tbody>
          <tr class="border-b border-slate-50">
            <td class="px-4 py-2.5 text-xs font-bold text-slate-400 uppercase">Überschuss</td>
            <td class="px-4 py-2.5 text-right font-mono font-bold" :class="signColor(ueberschuss)">
              {{ ueberschuss >= 0 ? '+' : ''}}{{ Math.round(ueberschuss) }} W
            </td>
          </tr>
          <tr class="border-b border-slate-50">
            <td class="px-4 py-2.5 text-xs font-bold text-slate-400 uppercase">Eigenverbrauch</td>
            <td class="px-4 py-2.5 text-right font-mono font-bold text-slate-700">
              {{ hauslast }} W
            </td>
          </tr>
          <tr>
            <td class="px-4 py-2.5 text-xs font-bold text-slate-400 uppercase">Autarkie</td>
            <td class="px-4 py-2.5 text-right font-mono font-bold" :class="autonomieColor">
              {{ autonomie }}%
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  liveData: { type: Object, default: () => ({}) },
});

const pv            = computed(() => Math.round(props.liveData?.solEr ?? 0));
const hauslast      = computed(() => Math.round(props.liveData?.ev ?? 0));
const netzaenderung = computed(() => Math.round(props.liveData?.netzBezug ?? 0));

// ├── Fronius API Konvention: p_grid < 0 = Einspeisung, p_grid > 0 = Bezug
// ├── negative Werte = rot, positive Werte = blau
const ueberschuss   = computed(() => pv.value - hauslast.value);
const autonomie     = computed(() => {
  if (hauslast.value === 0) return pv.value > 0 ? 100 : 0;
  // p_grid > 0 = Bezug → Eigenverbrauch = pv - netzBezug = hauslast - netzaenderung
  // p_grid < 0 = Einspeisung → Eigenverbrauch = pv - (-einspeisung)
  // vereinfacht: eigenverbrauch = hauslast - netz (netz>0=Bezug)
  const eigenverbrauch = hauslast.value - netzaenderung.value;
  return Math.min(100, Math.max(0, Math.round((eigenverbrauch / hauslast.value) * 100)));
});

// ── Farben: negativ = ROT, positiv = BLAU ──
// Fronius P_Grid: < 0 = Einspeisung, > 0 = Bezug
const netzLabel     = computed(() => netzaenderung.value < 0 ? 'Einspeisung' : 'Netzbezug');
const borderColor   = computed(() => netzaenderung.value < 0 ? 'border-red-300' : 'border-blue-300');
const labelColor    = computed(() => netzaenderung.value < 0 ? 'text-red-500' : 'text-blue-500');
const wertColor     = computed(() => netzaenderung.value < 0 ? 'text-red-600' : 'text-blue-600');

function signColor(v) {
  return v < 0 ? 'text-red-600' : 'text-blue-600';  // neg=rot, pos=blau
}


const autonomieColor = computed(() =>
  autonomie.value >= 80 ? 'text-emerald-500'
  : autonomie.value >= 50 ? 'text-amber-500'
  : 'text-red-500'
);
</script>
