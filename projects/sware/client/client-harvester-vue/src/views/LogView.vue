<template>
    <div class="w-full h-[calc(100vh-120px)] px-4 md:px-[50px] py-4 bg-slate-50 flex flex-col">

        <div class="w-full bg-white p-4 rounded-t-3xl border border-b-0 flex justify-between items-center shadow-sm">
            <div class="flex items-center gap-4">
                <h2 class="font-black uppercase italic text-slate-800 text-lg tracking-tighter">System Analysis</h2>
                <span
                    class="bg-blue-50 text-blue-600 px-3 py-1 rounded-full text-[10px] font-bold border border-blue-100">
                    {{ logs.length }} LOGS TOTAL
                </span>
            </div>
            <button @click="clearLogs"
                class="text-[10px] font-bold text-rose-500 hover:bg-rose-50 px-4 py-2 rounded-lg border border-rose-100 transition-all active:scale-95">
                LOGS LEEREN
            </button>
        </div>

        <div class="bg-white border rounded-b-3xl shadow-xl overflow-hidden flex-grow flex flex-col w-full">
            <div class="overflow-y-auto flex-grow w-full">
                <table class="w-full table-fixed border-collapse font-mono text-[14px]">
                    <thead
                        class="bg-slate-100 text-slate-600 sticky top-0 z-10 shadow-sm uppercase text-[11px] tracking-wider">
                        <tr>
                            <th class="p-4 w-[20%] text-left font-black">Zeitpunkt / Dauer</th>
                            <th class="p-4 w-[10%] text-center border-l border-slate-200 font-black">L1</th>
                            <th class="p-4 w-[10%] text-center border-l border-slate-200 font-black">L2</th>
                            <th class="p-4 w-[20%] text-center border-l border-slate-200 font-black">Leistung</th>
                            <th class="p-4 w-[20%] text-center border-l border-slate-200 font-black">PWM (%)</th>
                            <th class="p-4 w-[20%] text-center border-l border-slate-200 font-black">Boiler</th>
                        </tr>
                    </thead>

                    <tbody class="divide-y divide-slate-100">
                        <tr v-for="(log, i) in paginatedLogs" :key="i" class="hover:bg-blue-50/50 transition-colors">

                            <td class="p-4 text-left">
                                <div class="flex flex-col gap-1">
                                    <div class="text-slate-700 font-bold text-sm leading-tight">
                                        {{ formatUnixTime(log.firstSeen) }}
                                    </div>
                                    <div v-if="log.firstSeen !== log.lastSeen" class="flex flex-col gap-0.5">
                                        <div class="flex items-center gap-1.5">
                                            <span
                                                class="bg-emerald-500 text-white text-[9px] px-1 rounded font-black uppercase tracking-tighter">
                                                +{{ getDuration(log.firstSeen, log.lastSeen) }}
                                            </span>
                                            <span class="text-[9px] text-slate-400 italic">Dauer</span>
                                        </div>
                                    </div>
                                </div>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <span :class="(log.state & 1) ? 'text-emerald-500' : 'text-slate-300'"
                                    class="text-xl">●</span>
                                <div class="text-[9px] font-black"
                                    :class="(log.state & 1) ? 'text-slate-800' : 'text-slate-400'">
                                    {{ (log.state & 1) ? 'AKTIV' : 'AUS' }}
                                </div>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <span :class="(log.state & 2) ? 'text-emerald-500' : 'text-slate-300'"
                                    class="text-xl">●</span>
                                <div class="text-[9px] font-black"
                                    :class="(log.state & 2) ? 'text-slate-800' : 'text-slate-400'">
                                    {{ (log.state & 2) ? 'AKTIV' : 'AUS' }}
                                </div>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <div class="flex flex-col items-center gap-1">
                                    <span class="font-black text-slate-700 text-sm">{{ log.power }}W</span>
                                    <div
                                        class="w-20 h-1.5 bg-slate-100 rounded-full overflow-hidden border border-slate-200">
                                        <div class="bg-emerald-400 h-full rounded-full transition-all duration-500"
                                            :style="{ width: Math.min((log.power / 3000) * 100, 100) + '%' }"></div>
                                    </div>
                                </div>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <div class="flex items-center justify-center gap-3">
                                    <div
                                        class="w-full bg-slate-50 h-2 rounded-full overflow-hidden max-w-[80px] hidden lg:block border border-slate-200">
                                        <div class="bg-blue-500 h-full transition-all duration-300 shadow-[0_0_8px_rgba(59,130,246,0.4)]"
                                            :style="{ width: log.pwm + '%' }"></div>
                                    </div>
                                    <span class="font-black text-blue-600 text-[13px]">{{ log.pwm }}%</span>
                                </div>
                            </td>

                            <td class="p-4 text-center border-l border-slate-50">
                                <div
                                    class="inline-flex items-center px-3 py-1.5 bg-orange-50 rounded-lg border border-orange-100 shadow-sm">
                                    <span class="text-orange-600 font-black text-sm">{{ log.temp }}°C</span>
                                </div>
                            </td>

                        </tr>
                    </tbody>
                </table>
            </div>

            <div
                class="p-3 bg-slate-50 border-t flex justify-between items-center text-[10px] font-black text-slate-400 uppercase tracking-widest px-6">
                <div class="flex gap-4">
                    <span>Speicher: {{ logs.length }} / 1000</span>
                    <span v-if="logs.length > 0" class="text-blue-500">Live Datenstrom aktiv</span>
                </div>
                <div class="flex items-center gap-2">
                    <div
                        class="w-2 h-2 bg-emerald-500 rounded-full animate-pulse shadow-[0_0_8px_rgba(16,185,129,0.6)]">
                    </div>
                    Monitoring Live
                </div>
                <div class="flex items-center gap-3">
                    <button @click="prevPage" :disabled="currentPage === 1"
                        class="px-3 py-1 rounded border bg-white disabled:opacity-30">
                        ← Zurück
                    </button>

                    <span class="text-slate-600">
                        Seite {{ currentPage }} / {{ totalPages }}
                    </span>

                    <button @click="nextPage" :disabled="currentPage === totalPages"
                        class="px-3 py-1 rounded border bg-white disabled:opacity-30">
                        Weiter →
                    </button>
                </div>
            </div>
        </div>
    </div>
</template>

<script setup>
import { ref, computed } from 'vue'; // Sicherstellen, dass beides da ist

// HIER: 'const' statt 'onst'
const props = defineProps({
    liveData: {
        type: Object,
        default: () => ({})
    },
    logs: {
        type: Array,
        required: true,
        default: () => []
    },
    isConnected: {
        type: Boolean,
        default: false
    },
    maxLogs: {
        type: Number,
        default: 1000
    }
});

const emit = defineEmits(['clear', 'update-max']); // Falls du das Limit nach oben funken willst

const filterTag = ref("");
const filterLevel = ref("");
const displayLimit = ref(100);

const currentPage = ref(1)
const itemsPerPage = 20

// Verfügbare Tags für das Dropdown
const availableTags = computed(() => {
    if (!props.logs) return [];
    return [...new Set(props.logs.map(l => l.tag))].filter(Boolean).sort();
});

// Die kombinierte Filter-Logik


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

const formatUnixTime = (unixSeconds) => {
    if (!unixSeconds) return "--:--:--";

    // Unix-Sekunden in JS-Datum umwandeln
    const date = new Date(unixSeconds * 1000);

    // Formatieren (HH:MM:SS)
    return date.toLocaleString('de-DE', {
        day: '2-digit',
        month: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
};

const getDuration = (first, last) => {
    const diff = last - first;
    if (diff <= 0) return null;
    if (diff < 60) return `${diff}s`;
    const mins = Math.floor(diff / 60);
    const secs = diff % 60;
    return secs > 0 ? `${mins}m ${secs}s` : `${mins}m`;
};
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

const paginatedLogs = computed(() => {
    const start = (currentPage.value - 1) * itemsPerPage
    const end = start + itemsPerPage

    return filteredLogs.value.slice(start, end)
})
const totalPages = computed(() => {
    return Math.ceil(filteredLogs.value.length / itemsPerPage)
})
function nextPage() {
    if (currentPage.value < totalPages.value) {
        currentPage.value++
    }
}

function prevPage() {
    if (currentPage.value > 1) {
        currentPage.value--
    }
}


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