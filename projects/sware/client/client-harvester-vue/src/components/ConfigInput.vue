<template>
    <div class="flex flex-col gap-1.5">
        <label class="text-[11px] font-bold text-slate-500 uppercase tracking-wider ml-1">
            {{ label }}
        </label>

        <div class="relative group">
            <input :type="type || 'text'" :value="modelValue" @input="$emit('update:modelValue', $event.target.value)"
                class="w-full bg-slate-50 border border-slate-200 rounded-xl px-4 py-2.5 
               text-slate-700 font-medium transition-all duration-200
               outline-none focus:ring-2 focus:ring-blue-500/20 focus:border-blue-500 
               hover:border-slate-300 focus:bg-white" :placeholder="placeholder" />

            <div v-if="unit"
                class="absolute right-4 top-1/2 -translate-y-1/2 text-slate-400 text-xs font-semibold pointer-events-none">
                {{ unit }}
            </div>
        </div>
    </div>
</template>

<script setup>
/**
 * props:
 * - label: Der Text über dem Feld
 * - modelValue: Der Wert (v-model Bindung)
 * - type: text, number, password etc.
 * - unit: Optionale Anzeige am rechten Rand (z.B. "W")
 * - placeholder: Grauer Hinweistext
 */
defineProps({
    label: String,
    modelValue: [String, Number],
    type: { type: String, default: 'text' },
    unit: String,
    placeholder: String
});

// Emits für das v-model Binding
defineEmits(['update:modelValue']);
</script>