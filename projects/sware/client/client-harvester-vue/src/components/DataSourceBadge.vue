<template>
  <span
    class="inline-flex items-center gap-1.5 text-[9px] font-black uppercase tracking-widest px-3 py-1 rounded-full border shadow-sm transition-all duration-500"
    :class="badgeClasses"
  >
    <span class="w-1.5 h-1.5 rounded-full bg-current animate-pulse"></span>
    <span v-if="source === 'fronius'">☀️ Fronius Solar</span>
    <span v-else-if="source === 'modbus'">📡 Modbus</span>
    <span v-else-if="source === 'amis'">📊 AMIS Reader</span>
    <span v-else>⚠️ Keine Quelle</span>
  </span>
</template>

<script setup>
defineProps({
  source: { type: String, default: null },
});

const badgeClasses = () => {
  switch (source?.value ?? source) {
    case 'fronius': return 'bg-amber-50 text-amber-700 border-amber-200';
    case 'modbus':  return 'bg-sky-50 text-sky-700 border-sky-200';
    case 'amis':    return 'bg-violet-50 text-violet-700 border-violet-200';
    default:        return 'bg-rose-50 text-rose-600 border-rose-200';
  }
};
</script>
