export const dataMapping = [
  { key: "solEr", label: "Produktion", unit: "Watt" },
  { key: "ev", label: "Verbrauch", unit: "Watt" },
  { key: "netzBezug", label: "Einspeisung/Bezug", unit: "Watt" },
  { key: "bTemp", label: "Temperatur", unit: "Grad" },
  { key: "L1", label: "Heizstab L1", unit: "on/off" },
  { key: "L2", label: "Heizstab L2", unit: "on/off" },
  { key: "L3", label: "PWM L3", unit: "%" },
  { key: "hsPhase", label: "Heizstab/Phase", unit: "W" },
  { key: "weatherBonus", label: "Wetter-Bonus", unit: "°C" },
  { key: "weatherPvRatio", label: "PV-Verhältnis", unit: "--" },
  { key: "weatherCloudAvg", label: "Wolken", unit: "%" },
  { key: "weatherOutTemp", label: "Außentemperatur", unit: "°C" },
];

// TinyNN-Felder (optional, nur wenn TinyNN im ESP32 aktiv)
export const tinyNNMapping = [
  { key: "tinyNN_preheat", label: "NN Preheat", unit: "%" },
  { key: "tinyNN_buffer", label: "NN Energiebudget", unit: "%" },
];


export function formatRelay(value) {
  if (value === true || value === 1 || value === "1") return "Ein";
  if (value === false || value === 0 || value === "0") return "Aus";
  return "--";
}

export function finiteNumber(value) {
  const number = Number(value);
  return Number.isFinite(number) ? number : 0;
}

export function pwmPercent(value) {
  return Math.max(0, Math.min(100, finiteNumber(value) / 255 * 100));
}

export function formatValue(key, value) {
  if (value == null) return "--";
  if (key === "L1" || key === "L2") return formatRelay(value);
  if (key === "L3") return pwmPercent(value).toFixed(1);
  if (["weatherCloudAvg", "tinyNN_preheat", "tinyNN_buffer"].includes(key))
    return Math.round(finiteNumber(value) * 100);
  return typeof value === "number" ? value.toFixed(1) : value;
}

// Bitbelegung aus src/www/webSockets.cpp; Betriebsmodi sind keine Fehler.
export function interpretErrors(bits) {
  return [
    [1, "Flash-Speicher Fehler"],
    [3, "Temperatursensor Fehler"],
    [4, "Boilerheizung nicht bereit"],
    [5, "Energiequelle nicht verfügbar"],
    [6, "MQTT nicht verbunden"],
  ].filter(([bit]) => bits & (1 << bit)).map(([, message]) => message);
}

// Vollständige Klassennamen, damit Tailwind alle Farben beim Build erkennt.
const energyBarMapping = [
  { key: "L3", label: "Basis (PWM)", textClass: "text-blue-600", fillClass: "bg-blue-500" },
  { key: "weatherBonus", label: "Wetter-Bonus", textClass: "text-amber-600", fillClass: "bg-amber-400" },
  { key: "tinyNN_preheat", label: "NN Preheat", textClass: "text-violet-600", fillClass: "bg-violet-500", optional: true },
  { key: "tinyNN_buffer", label: "NN Energiebudget", textClass: "text-teal-600", fillClass: "bg-teal-400", optional: true },
];

export function energyBarsFor(live) {
  return energyBarMapping.map((bar) => {
    const raw = finiteNumber(live[bar.key]);
    const weather = bar.key === "weatherBonus";
    const value = bar.key === "L3" ? pwmPercent(raw) : weather ? raw : raw * 100;
    return {
      ...bar,
      value,
      display: weather ? `${value >= 0 ? "+" : ""}${value}°C` : `${Math.round(value)}%`,
      width: Math.max(0, Math.min(100, weather ? value * 20 : value)),
    };
  }).filter((bar) => !bar.optional || bar.value > 0);
}
