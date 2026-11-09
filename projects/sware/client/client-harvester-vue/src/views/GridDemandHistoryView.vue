<template>
  <div class="max-w-6xl mx-auto p-4 md:p-8 space-y-5">
    <header><h1 class="text-3xl font-bold">Lokaler Viertelstundenverlauf</h1><p class="text-slate-600">IndexedDB dieses Browsers. Bei geschlossenem Browser oder ohne Anmeldung entstehen Lücken; andere Geräte haben eigene Verläufe.</p></header>
    <router-link to="/netzbezug" class="text-indigo-700 underline">Zurück zum aktuellen Netzbezug</router-link>
    <div class="bg-white rounded-3xl p-5 flex flex-wrap gap-3 items-end">
      <label class="text-sm">Datum <input type="date" v-model="day" class="block mt-1 p-2 border rounded-xl" /></label>
      <button type="button" @click="exportCsv" class="px-4 py-2 rounded-xl bg-slate-100 font-semibold">CSV exportieren</button>
      <button type="button" @click="removeAll" class="px-4 py-2 rounded-xl bg-rose-50 text-rose-800 font-semibold">Gesamten lokalen Verlauf löschen</button>
    </div>
    <p role="status" class="text-sm text-slate-600">{{ message }}</p>
    <p class="text-xs text-slate-500">Importenergie ist gemessener Bezug. Ein Viertelstundenmittelwert erscheint nur bei vollständiger Messabdeckung.</p>
    <div v-if="rows.length" class="bg-white rounded-3xl p-5">
      <p class="font-semibold mb-3">Mittelwert je Intervall</p>
      <div class="flex items-end gap-1 h-36 overflow-x-auto" role="img" aria-label="Balkendiagramm des Viertelstundenbezugs">
        <div v-for="item in chronological" :key="item.start" :title="`${clock(item.start)}: ${item.complete ? Math.round(item.averageImportW) + ' W' : 'unvollständig'}`"
          class="min-w-2 rounded-t" :class="item.complete ? 'bg-indigo-500' : 'bg-amber-300'" :style="{ height: `${barHeight(item)}%` }"></div>
      </div>
    </div>
    <DemandHistoryTable :rows="rows" @remove="removeOne" />
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref, watch } from 'vue';
import { db } from '../db';
import { clearDemandHistory } from '../services/demandHistory';
import DemandHistoryTable from '../components/demand/DemandHistoryTable.vue';
const today = () => { const d = new Date(); return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`; };
const day = ref(today());
const rows = ref([]);
const message = ref('');
const chronological = computed(() => [...rows.value].reverse());
const maxW = computed(() => Math.max(1, ...rows.value.map(item => item.complete ? item.averageImportW || 0 : 0)));
const barHeight = item => item.complete ? Math.max(3, (item.averageImportW || 0) / maxW.value * 100) : 5;
const clock = sec => new Date(sec * 1000).toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' });
let refreshTimer;

async function refresh() {
  try {
    const [year, month, date] = day.value.split('-').map(Number);
    const begin = new Date(year, month - 1, date).getTime() / 1000;
    const end = new Date(year, month - 1, date + 1).getTime() / 1000;
    const ascending = await db.demandIntervals.where('start').between(begin, end, true, false).toArray();
    rows.value = ascending.map((item, index) => ({
      ...item,
      missingBefore: index ? Math.max(0, Math.round((item.start - ascending[index - 1].start) / 900) - 1)
        : item.missingBefore
    })).reverse();
  } catch (error) { message.value = `Verlauf nicht lesbar: ${error.message}`; }
}
async function removeOne(start) {
  if (!window.confirm('Dieses lokale Intervall löschen?')) return;
  await db.demandIntervals.delete(start);
  await refresh();
}
async function removeAll() {
  if (!window.confirm('Den gesamten lokalen Viertelstundenverlauf dieses Browsers löschen?')) return;
  await clearDemandHistory();
  await refresh();
  message.value = 'Lokaler Verlauf gelöscht.';
}
function exportCsv() {
  const lines = ['start_utc,end_utc,import_wh,average_import_w,observed_seconds,complete,limit_w,missing_before'];
  for (const item of chronological.value) lines.push([
    new Date(item.start * 1000).toISOString(), new Date(item.end * 1000).toISOString(),
    item.importEnergyWh ?? '', item.averageImportW ?? '', item.observedSeconds,
    item.complete, item.limitW, item.missingBefore
  ].join(','));
  const url = URL.createObjectURL(new Blob([lines.join('\n') + '\n'], { type: 'text/csv;charset=utf-8' }));
  const anchor = document.createElement('a');
  anchor.href = url; anchor.download = `netzbezug-${day.value}.csv`; anchor.click();
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}
watch(day, refresh);
onMounted(() => { refresh(); refreshTimer = window.setInterval(refresh, 15000); });
onUnmounted(() => window.clearInterval(refreshTimer));
</script>
