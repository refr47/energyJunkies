import { ref } from 'vue';

export function useSetupPatch(defaults) {
  const values = ref({ ...defaults });
  const loading = ref(false);
  const saving = ref(false);
  const message = ref('');
  const errors = ref([]);

  async function load() {
    loading.value = true;
    try {
      const response = await fetch('/getSetup');
      const data = await response.json();
      if (!response.ok || data.error?.code !== 1) throw new Error(data.error?.msg || 'Laden fehlgeschlagen');
      values.value = Object.fromEntries(Object.keys(defaults).map(key =>
        [key, data.result?.[key] ?? defaults[key]]));
      message.value = data.warning || '';
    } catch (error) { message.value = error.message; }
    finally { loading.value = false; }
  }

  async function save() {
    saving.value = true;
    errors.value = [];
    try {
      const response = await fetch('/storeSetup', {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(values.value)
      });
      const data = await response.json();
      if (!response.ok || !data.done) {
        errors.value = data.errors || [];
        throw new Error(data.error || 'Speichern fehlgeschlagen');
      }
      message.value = data.apply === 'restart_required'
        ? 'Gespeichert; Gerät startet neu.' : 'Gespeichert; Übernahme läuft.';
      return true;
    } catch (error) { message.value = error.message; return false; }
    finally { saving.value = false; }
  }

  return { values, loading, saving, message, errors, load, save };
}
