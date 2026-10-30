import { ref, watch } from "vue";

// Lokaler, optimistischer Zustand; eingehende Serverwerte bleiben maßgeblich.
export function useHeaterToggle(serverValue, setupKey) {
  const enabled = ref(false);
  watch(serverValue, (value) => { enabled.value = !!value; }, { immediate: true });

  const toggle = async () => {
    enabled.value = !enabled.value;
    try {
      await fetch("/storeSetup", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ [setupKey]: enabled.value ? 1 : 0 }),
      });
    } catch (error) {
      console.error(`${setupKey}: Toggle fehlgeschlagen`, error);
      enabled.value = !enabled.value;
    }
  };

  return { enabled, toggle };
}
