<template>
    <div class="w-full h-[calc(100vh-120px)] px-[50px] py-4 bg-slate-50 flex flex-col">

        <div class="w-full bg-white p-4 rounded-t-3xl border border-b-0 flex justify-between items-center shadow-sm">
            <div class="flex items-center gap-4">
                <h2 class="font-black uppercase italic text-slate-800 text-lg">System Analysis</h2>
                <span
                    class="bg-blue-50 text-blue-600 px-3 py-1 rounded-full text-[10px] font-bold border border-blue-100">
                    {{ logs.length }} LOGS TOTAL
                </span>
            </div>
            <button @click="clearLogs"
                class="text-[10px] font-bold text-rose-500 hover:bg-rose-50 px-4 py-2 rounded-lg border border-rose-100 transition-colors">
                LOGS LEEREN
            </button>
        </div>

        <div class="bg-white border rounded-b-3xl shadow-xl overflow-hidden flex-grow flex flex-col w-full">
            <div class="overflow-y-auto flex-grow w-full">

                <table class="w-full table-fixed border-collapse font-mono text-[12px]">
                    <thead
                        class="bg-slate-100 text-slate-600 sticky top-0 z-10 shadow-sm uppercase text-[10px] tracking-wider">
                        <tr>
                            <th class="p-4 w-[20%] text-left">Timestamp</th>
                            <th class="p-4 w-[15%] text-center border-l border-slate-200">Phase L1</th>
                            <th class="p-4 w-[15%] text-center border-l border-slate-200">Phase L2</th>
                            <th class="p-4 w-[25%] text-center border-l border-slate-200">PWM (%)</th>
                            <th class="p-4 w-[25%] text-center border-l border-slate-200">Boiler Temp</th>
                        </tr>
                    </thead>

                    <tbody class="divide-y divide-slate-100">
                        <tr v-for="(log, i) in filteredLogs" :key="i" class="hover:bg-blue-50/50 transition-colors">

                            <td class="p-4 text-left text-slate-500 font-bold">
                                {{ formatUnixTime(log.firstSeen) }}
                                <div v-if="log.firstSeen !== log.lastSeen"
                                    class="text-[9px] text-emerald-500 font-black italic">
                                    bis {{ formatUnixTime(log.lastSeen) }}
                                </div>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <span :class="log.l1 ? 'text-emerald-500' : 'text-slate-300'" class="text-xl">●</span>
                                <span class="ml-2 font-black">{{ log.l1 }}</span>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <span :class="log.l2 ? 'text-emerald-500' : 'text-slate-300'" class="text-xl">●</span>
                                <span class="ml-2 font-black">{{ log.l2 }}</span>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <div class="flex items-center justify-center gap-3">
                                    <div
                                        class="w-full bg-slate-100 h-2 rounded-full overflow-hidden max-w-[100px] hidden md:block">
                                        <div class="bg-blue-500 h-full" :style="{ width: log.pwm + '%' }"></div>
                                    </div>
                                    <span class="font-black text-blue-600 text-sm">{{ log.pwm }}%</span>
                                </div>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <span class="text-orange-600 font-black text-sm">{{ log.temp }}°C</span>
                            </td>

                        </tr>
                    </tbody>
                </table>
            </div>

            <div
                class="p-3 bg-slate-50 border-t flex justify-between items-center text-[10px] font-black text-slate-400 uppercase tracking-widest px-6">
                <span>Speicher: {{ logs.length }}</span>
                <div class="flex items-center gap-2">
                    <div class="w-2 h-2 bg-emerald-500 rounded-full animate-pulse"></div>
                    Monitoring Aktiv
                </div>
            </div>
        </div>
    </div>
</template>


<script setup>
import { ref, computed } from 'vue'; // Sicherstellen, dass beides da ist

// HIER: 'const' statt 'onst'
const props = defineProps(['logs', 'maxLogs']);
const emit = defineEmits(['clear', 'update-max']); // Falls du das Limit nach oben funken willst

const filterTag = ref("");
const filterLevel = ref("");
const displayLimit = ref(100);

// Verfügbare Tags für das Dropdown
const availableTags = computed(() => {
    if (!props.logs) return [];
    return [...new Set(props.logs.map(l => l.tag))].filter(Boolean).sort();
});

// Die kombinierte Filter-Logik
const filteredLogs = computed(() => {
    if (!props.logs) return [];

    return props.logs
        .filter(l => {
            const tagMatch = !filterTag.value || l.tag === filterTag.value;
            const lvlMatch = !filterLevel.value || l.level === filterLevel.value;
            return tagMatch && lvlMatch;
        })
        .slice(0, displayLimit.value);
});

const getLevelClass = (lvl) => {
    switch (lvl) {
        case 'E': return 'bg-rose-100 text-rose-600'; // Error
        case 'W': return 'bg-amber-100 text-amber-600'; // Warning
        case 'D': return 'bg-slate-100 text-slate-500'; // Debug
        case 'I': return 'bg-emerald-100 text-emerald-600'; // Info
        default: return 'bg-slate-100 text-slate-400';
    }
};

const clearLogs = () => {
    if (confirm("Alle Logs im Browser löschen?")) {
        // Da props eigentlich nicht direkt verändert werden sollten:
        // Falls logEntries in App.vue ein ref ist, ist das hier ein schneller Hack.
        // Sauberer wäre: emit('clear');
        props.logs.splice(0, props.logs.length);
    }
};
/* const formatTimestamp = (ms) => {
    // Falls ms ein String ist, versuchen wir ihn zu konvertieren, 
    // ansonsten nehmen wir das aktuelle Datum als Fallback
    const date = new Date(ms || Date.now());

    return date.toLocaleString('de-DE', {
        day: '2-digit',
        month: '2-digit',
        year: '2-digit', // '2-digit' spart Platz (24 statt 2024)
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
}; */
const formatUnixTime = (unixSeconds) => {
    if (!unixSeconds) return "--:--:--";

    // Unix-Sekunden in JS-Datum umwandeln
    const date = new Date(unixSeconds * 1000);

    // Formatieren (HH:MM:SS)
    return date.toLocaleTimeString('de-DE', {
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
};
</script>


<style scoped>
/* Radikaler Fix gegen jegliche Zentrierung */
table {
    width: 100% !important;
    table-layout: fixed !important;
}

th,
td {
    text-align: left !important;
    /* Erzwingt Links-Ausrichtung */
    vertical-align: top !important;
}

/* Nur das Level-Icon darf zentriert sein */
th:nth-child(2),
td:nth-child(2) {
    text-align: center !important;
}
</style>