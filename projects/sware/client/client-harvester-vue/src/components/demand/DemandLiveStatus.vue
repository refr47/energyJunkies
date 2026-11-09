<template>
  <section class="bg-white p-6 rounded-3xl shadow-sm space-y-3" aria-labelledby="demand-live">
    <h2 id="demand-live" class="text-lg font-bold">Aktueller Bezug</h2>
    <p class="font-semibold">{{ labels[demand?.status] || 'Warte auf Live-Daten' }}</p>
    <p v-if="!connected" class="text-amber-700 text-sm">WebSocket unterbrochen; Werte können veraltet sein.</p>
    <div v-if="demand?.enabled" class="grid sm:grid-cols-2 gap-2 text-sm">
      <p>Aktuelles Intervall: {{ interval(demand.current) }}</p>
      <p>Beobachteter Bezug: {{ wh(demand.current?.importEnergyWh) }}</p>
      <p>Messabdeckung: {{ Math.round(demand.current?.observedSeconds || 0) }} von 900 s</p>
      <p>Restbudget: {{ wh(demand.current?.remainingEnergyWh) }}</p>
      <p>Erlaubter Restbezug: {{ watt(demand.current?.allowedRemainingImportW) }}</p>
      <p>Heizleistungsgrenze: {{ watt(demand.maxHeaterPowerW) }}</p>
      <p>Begrenzung: {{ demand.enforceLimit ? (demand.limiting ? 'aktiv' : 'derzeit nicht aktiv') : 'aus' }}</p>
      <p>Messwertalter: {{ demand.measurementAgeMs == null ? '–' : `${Math.round(demand.measurementAgeMs / 1000)} s` }}</p>
    </div>
    <p class="text-xs text-slate-500">Viertelstunden beginnen zu UTC-Viertelstunden; Zeiten erscheinen hier lokal. Unvollständige Messungen sind keine Abrechnungswerte.</p>
  </section>
</template>

<script setup>
defineProps({ demand: Object, connected: Boolean });
const labels = { disabled: 'Überwachung aus', timeInvalid: 'Uhrzeit ungültig', night: 'Abfragepause', stale: 'Messung fehlt oder veraltet', collecting: 'Intervall unvollständig', ready: 'Messung läuft' };
const time = sec => sec ? new Date(sec * 1000).toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' }) : '–';
const interval = item => item?.start ? `${time(item.start)}–${time(item.end)}` : '–';
const wh = value => value == null ? '–' : `${Number(value).toLocaleString('de-DE', { maximumFractionDigits: 1 })} Wh`;
const watt = value => value == null ? '–' : `${Math.round(value).toLocaleString('de-DE')} W`;
</script>
