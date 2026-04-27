<template>
    <Transition name="slide-fade">
        <div v-if="message" :class="[
            'fixed top-6 right-6 z-[100] flex items-center gap-4 px-6 py-4 rounded-2xl shadow-2xl border-2 transition-all duration-500 min-w-[300px]',
            type === 'success' ? 'bg-emerald-50 border-emerald-200 text-emerald-800' :
                type === 'error' ? 'bg-rose-50 border-rose-200 text-rose-800' :
                    'bg-blue-50 border-blue-200 text-blue-800'
        ]">

            <span class="text-2xl">
                {{ type === 'success' ? '✅' : type === 'error' ? '❌' : '⏳' }}
            </span>

            <div class="flex-grow">
                <p class="font-black text-[10px] uppercase tracking-widest opacity-50 mb-1">
                    {{ type === 'success' ? 'System-Meldung' : 'Status' }}
                </p>
                <p class="font-bold text-sm leading-tight">{{ message }}</p>
            </div>

            <button @click="$emit('close')" class="p-1 hover:bg-black/5 rounded-lg transition-colors">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path d="M6 18L18 6M6 6l12 12" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" />
                </svg>
            </button>
        </div>
    </Transition>
</template>

<script setup>
defineProps({
    message: String,
    type: {
        type: String,
        default: 'success'
    }
});

defineEmits(['close']);
</script>

<style scoped>
.slide-fade-enter-active {
    transition: all 0.3s ease-out;
}

.slide-fade-leave-active {
    transition: all 0.4s cubic-bezier(1, 0.5, 0.8, 1);
}

.slide-fade-enter-from,
.slide-fade-leave-to {
    transform: translateX(30px) scale(0.9);
    opacity: 0;
}
</style>