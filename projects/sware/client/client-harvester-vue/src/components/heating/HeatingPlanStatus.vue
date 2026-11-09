<template>
  <section class="bg-white p-6 rounded-3xl shadow-sm space-y-3" aria-labelledby="plan-status">
    <h2 id="plan-status" class="font-bold text-lg">Aktueller Heizplan</h2>
    <p class="font-semibold">{{ labels[plan?.status] || 'Warte auf Live-Daten' }}</p>
    <p v-if="!connected" class="text-amber-700 text-sm">Verbindung unterbrochen; Livewerte können veraltet sein.</p>
    <p v-if="plan?.pollingOverride" class="text-sm text-indigo-700">Die nächtliche Abfragepause ist derzeit ausgesetzt; Netzwerte werden für Tarifintervall oder Notfallheizung benötigt.</p>
    <p v-if="plan?.windowActive && plan.loadHeadroomW == null" class="text-sm text-amber-700">Keine nutzbare Hauslastmessung; lastabhängiges Verschieben ist derzeit nicht möglich.</p>
    <div v-if="plan?.windowActive" class="grid sm:grid-cols-2 gap-2 text-sm">
      <p>Fenster endet: {{ time(plan.windowEnd) }}</p>
      <p>Spätester geschätzter Start: {{ time(plan.latestStart) }}</p>
      <p>Geschätzter Wärmebedarf: {{ plan.requiredWh?.toFixed(0) }} Wh</p>
      <p>Geschätzte Heizdauer: {{ Math.ceil(plan.estimatedSeconds / 60) }} min</p>
      <p v-if="plan.loadHeadroomW != null">Geschätzte freie Netzleistung: {{ Math.round(plan.loadHeadroomW) }} W</p>
    </div>
    <p class="text-xs text-slate-500">Schätzung aus Boilerinhalt und gemessener Temperatur, mit 20 % Zeitreserve. Wasserschichtung und Wärmeverluste können die tatsächliche Dauer verändern.</p>
    <p v-if="plan?.status === 'blockedDemand'" class="text-sm text-rose-700">Auch die Notfallheizung hält das Bezugslimit ein. Ohne zulässige Leistung bleibt sie aus.</p>
  </section>
</template>

<script setup>
defineProps({ plan: Object, connected: Boolean });
const time = seconds => seconds ? new Date(seconds * 1000).toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' }) : '–';
const labels = {
  disabled: 'Heizplan ausgeschaltet', sensorInvalid: 'Temperatursensor ungültig',
  timeInvalid: 'Uhrzeit ungültig; nur Notfallheizung möglich',
  outsideWindow: 'Außerhalb des Tarifintervalls', atTarget: 'Zieltemperatur erreicht',
  waitingLoad: 'Hohe Hauslast; Heizen wird verschoben', heating: 'Heizleistung angefordert',
  deadline: 'Spätester Start erreicht; Heizen angefordert',
  emergency: 'Notfallgrenze unterschritten; Heizen angefordert',
  blockedDemand: 'Heizen durch Viertelstundenlimit blockiert'
};
</script>
