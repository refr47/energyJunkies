<template>
  <table class="stripe w-full">
    <caption v-if="caption" class="text-xs font-bold uppercase tracking-widest text-slate-400 mb-2">
      {{ caption }}
    </caption>
    <thead>
      <tr><th>Titel</th><th>Wert</th><th>Einheit</th></tr>
    </thead>
    <tbody>
      <tr v-for="item in items" :key="item.key" :class="rowClass" :style="item.style">
        <td>{{ item.label }}</td>
        <td>{{ item.value }}</td>
        <td>{{ item.unit }}</td>
      </tr>
    </tbody>
  </table>
</template>

<script setup>
defineProps({
  items: { type: Array, required: true },
  caption: { type: String, default: "" },
  rowClass: { type: String, default: "" },
});
</script>
