<template>
  <div v-if="demand?.enabled" class="w-full sm:min-w-72 sm:w-auto rounded-xl border p-3"
       :class="risk ? 'bg-amber-50 border-amber-200' : 'bg-sky-50 border-sky-100'">
    <div class="text-xs text-slate-500 uppercase tracking-wider">Netzbezug · 15 Minuten</div>
    <div class="font-semibold text-slate-800">{{ statusText }}</div>
    <div v-if="demand.current?.start" class="text-sm text-slate-600">
      {{ clock(demand.current.start) }}–{{ clock(demand.current.end) }} ·
      {{ watts(demand.projectionValid ? demand.projectedAverageW : null) }} Prognose / {{ watts(demand.limitW) }} Grenze
    </div>
    <div v-if="demand.current?.start" class="text-xs text-slate-500">
      {{ Math.round(demand.current.importEnergyWh || 0) }} Wh erfasst ·
      {{ Math.round(demand.current.observedSeconds || 0) }} s Messabdeckung
    </div>
    <div v-if="demand.externalReductionW > 0" class="text-sm font-medium text-amber-800">
      Weitere Entlastung: {{ watts(demand.externalReductionW) }}
    </div>
    <div v-if="loads?.length" class="mt-2 pt-2 border-t border-slate-200 text-xs text-slate-600">
      <div v-for="load in loads" :key="load.id" class="flex justify-between gap-3">
        <span>{{ load.name }} <span v-if="load.pending">· schaltet</span><span v-else-if="load.shedByUs">· reduziert</span></span>
        <span>{{ load.fresh ? `${load.on ? 'Ein' : 'Aus'} · ${load.actualW} W${load.available ? '' : ' · nicht schaltbar'}` : 'Status unbekannt' }}</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';
const props = defineProps({ demand: Object, loads: Array });
const risk = computed(() => (props.demand?.externalReductionW || 0) > 0 || props.demand?.limiting);
const statusText = computed(() => {
  const d = props.demand;
  if (d?.status === 'timeInvalid') return 'Warte auf NTP-Zeit';
  if (d?.status === 'night') return 'Nachtruhe';
  if (d?.status === 'stale') return 'Netzmessung fehlt';
  if (!d?.current?.complete) return 'Teilintervall · Prognose noch unsicher';
  if (!d?.enforceLimit) return 'Monitoring ohne Begrenzung';
  return risk.value ? 'Leistung wird begrenzt' : 'Im Budget';
});
const watts = value => value == null ? '–' : `${Math.round(value).toLocaleString('de-AT')} W`;
const clock = epoch => new Date(epoch * 1000).toLocaleTimeString('de-AT', { hour: '2-digit', minute: '2-digit' });
</script>
