<template>
  <div class="overflow-x-auto bg-white rounded-3xl shadow-sm">
    <table class="min-w-full text-sm text-left">
      <thead class="bg-slate-100 text-slate-700"><tr><th class="p-3">Intervall</th><th class="p-3">Bezug</th><th class="p-3">Mittelwert</th><th class="p-3">Abdeckung</th><th class="p-3">Limit</th><th class="p-3">Aktion</th></tr></thead>
      <tbody><tr v-for="item in rows" :key="item.start" class="border-t border-slate-100">
        <td class="p-3 whitespace-nowrap">{{ stamp(item.start) }}–{{ clock(item.end) }}<span v-if="item.missingBefore" class="block text-amber-700">{{ item.missingBefore }} Intervall(e) fehlen davor</span></td>
        <td class="p-3">{{ number(item.importEnergyWh) }} Wh</td>
        <td class="p-3">{{ item.complete ? `${number(item.averageImportW)} W` : 'unvollständig' }}</td>
        <td class="p-3">{{ Math.round(item.observedSeconds) }} / 900 s</td>
        <td class="p-3">{{ number(item.limitW) }} W</td>
        <td class="p-3"><button type="button" @click="$emit('remove', item.start)" class="text-rose-700 underline">Löschen</button></td>
      </tr></tbody>
    </table>
    <p v-if="!rows.length" class="p-5 text-slate-500">Für diesen Tag sind keine lokalen Intervalle gespeichert.</p>
  </div>
</template>

<script setup>
defineProps({ rows: { type: Array, default: () => [] } });
defineEmits(['remove']);
const stamp = sec => new Date(sec * 1000).toLocaleString('de-DE', { day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit' });
const clock = sec => new Date(sec * 1000).toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' });
const number = value => value == null ? '–' : Number(value).toLocaleString('de-DE', { maximumFractionDigits: 1 });
</script>
