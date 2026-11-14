<template>
  <div class="min-w-0 bg-gradient-to-br from-sky-50 to-blue-50 rounded-xl p-3 border border-sky-100">
    <div class="text-xs text-slate-500 uppercase tracking-wider">MQTT</div>
    <div class="flex items-center gap-2 font-bold" :class="connected ? 'text-sky-700' : 'text-rose-600'">
      <span class="h-2 w-2 rounded-full bg-current" aria-hidden="true"></span>
      {{ connected ? 'Verbunden' : isConnected ? 'Broker nicht verbunden' : 'Keine Live-Daten' }}
    </div>

    <div v-if="lastPublished || lastReceived" class="mt-2 space-y-1 text-xs text-slate-600">
      <div v-if="lastPublished" class="flex gap-2 min-w-0">
        <span class="shrink-0 font-semibold">Gesendet</span>
        <span class="truncate" :title="lastPublished.topic">{{ lastPublished.topic }}</span>
        <span class="shrink-0 text-slate-400">{{ eventTime(lastPublished) }}</span>
      </div>
      <div v-if="lastReceived" class="flex gap-2 min-w-0">
        <span class="shrink-0 font-semibold">Empfangen</span>
        <span class="truncate" :title="lastReceived.topic">{{ lastReceived.topic }}</span>
        <span class="shrink-0 text-slate-400">{{ eventTime(lastReceived) }}</span>
      </div>
    </div>
    <p v-else class="mt-2 text-xs text-slate-500">Noch keine MQTT-Nachrichten</p>

    <details v-if="events.length" class="mt-2 text-xs">
      <summary class="cursor-pointer font-semibold text-sky-700">Letzte Nachrichten ({{ events.length }})</summary>
      <ul class="mt-2 max-h-56 overflow-y-auto space-y-1">
        <li v-for="event in events" :key="event.sequence" class="rounded-lg bg-white/80 px-2 py-1.5">
          <div class="flex items-center gap-2">
            <span class="shrink-0 font-semibold" :class="event.direction === 'receive' ? 'text-violet-700' : 'text-sky-700'">
              {{ event.direction === 'receive' ? 'Empfang' : 'Publish' }}
            </span>
            <span class="ml-auto shrink-0 text-slate-400">{{ eventTime(event) }}</span>
          </div>
          <div class="break-all text-slate-700">{{ event.topic }}</div>
          <div v-if="event.payload" class="break-all text-slate-500">{{ event.payload }}</div>
        </li>
      </ul>
    </details>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  mqtt: { type: Object, required: true },
  isConnected: Boolean,
});

const connected = computed(() => props.isConnected && props.mqtt.connected);
const events = computed(() => Array.isArray(props.mqtt.events) ? props.mqtt.events : []);
const lastPublished = computed(() => props.mqtt.lastPublished);
const lastReceived = computed(() => props.mqtt.lastReceived);
const eventTime = (event) => new Date(Date.now() - Math.max(0, Number(event.ageMs) || 0))
  .toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
</script>
