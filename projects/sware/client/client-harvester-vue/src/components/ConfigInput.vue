<template>
    <div class="relative group">
        <label 
            class="text-[10px] font-bold text-slate-400 uppercase tracking-tight ml-1 group-focus-within:text-blue-500 transition-colors">
            {{ label }}
        </label>
        <div class="relative mt-1">
            <input :type="type || 'text'" :value="modelValue" @input="$emit('update:modelValue', $event.target.value)"
                class="w-full bg-slate-50 border border-slate-200 rounded-xl px-4 py-3 text-slate-700 font-medium focus:bg-white focus:ring-2 focus:ring-blue-500/20 focus:border-blue-500 outline-none transition-all placeholder:text-slate-300"
                :placeholder="placeholder" />
            <div v-if="unit"
                class="absolute right-3 top-1/2 -translate-y-1/2 text-[10px] font-black text-slate-300 bg-white border border-slate-100 px-2 py-1 rounded-md">
                {{ unit }}
            </div>
        </div>
    </div>
</template>

<script setup>
defineProps(['label', 'modelValue', 'type', 'placeholder', 'unit']);
defineEmits(['update:modelValue']);
</script>