<template>
  <div class="h-full">
    <!-- Saldo -->
    <div class="bg-white rounded-xl p-4 border text-center" :class="borderColor">
      <p class="text-[9px] font-bold uppercase tracking-wider mb-3 text-slate-400">Momentaner Saldo</p>
      <p class="text-4xl font-black" :class="wertColor">
        {{ saldo >= 0 ? '+' : ''}}{{ saldo }}<span class="text-lg font-bold text-slate-300 ml-0.5">W</span>
      </p>
      <p class="text-[9px] font-bold mt-2" :class="labelColor">{{ saldoLabel }}</p>
    </div>

    <!-- Export / Import Tabelle -->
    <div class="mt-4 bg-white rounded-xl border border-slate-200 overflow-hidden">
      <table class="w-full text-sm">
        <tbody>
          <tr class="border-b border-slate-50">
            <td class="px-4 py-3 text-xs font-bold text-slate-400 uppercase">Export Gesamt</td>
            <td class="px-4 py-3 text-right font-mono font-bold text-blue-600">
              {{ Math.round(exportKwh) }} <span class="text-xs text-slate-300">kWh</span>
            </td>
          </tr>
          <tr class="border-b border-slate-50">
            <td class="px-4 py-3 text-xs font-bold text-slate-400 uppercase">Import Gesamt</td>
            <td class="px-4 py-3 text-right font-mono font-bold text-red-600">
              {{ Math.round(importKwh) }} <span class="text-xs text-slate-300">kWh</span>
            </td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- Hinweis -->
    <div class="mt-4 flex items-center gap-2 px-2">
      <span class="w-1.5 h-1.5 rounded-full bg-amber-400"></span>
      <p class="text-[9px] text-slate-400">AMIS Smartmeter — nur Saldo-Daten vom Zaehler</p>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  liveData: { type: Object, default: () => ({}) },
});

const saldo     = computed(() => Math.round(props.liveData?.ev ?? 0));
const isExport  = computed(() => saldo.value <= 0);
const exportKwh = computed(() => props.liveData?.SEI ?? 0);
const importKwh = computed(() => props.liveData?.SII ?? 0);

// ── Farben: negativ = ROT, positiv = BLAU ──
const saldoLabel  = computed(() => saldo.value < 0 ? 'Einspeisung' : 'Netzbezug');
const borderColor = computed(() => saldo.value < 0 ? 'border-red-300' : 'border-blue-300');
const labelColor  = computed(() => saldo.value < 0 ? 'text-red-500' : 'text-blue-500');
const wertColor   = computed(() => saldo.value < 0 ? 'text-red-600' : 'text-blue-600');
</script>
