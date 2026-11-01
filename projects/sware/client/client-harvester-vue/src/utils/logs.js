// Gepacktes ESP32-Format: uint32 ts, uint8 state, int16 power, uint8 pwm, int16 temp.
const LOG_ENTRY_BYTES = 10;

export function parseLogBlob(base64String, count) {
  const bytes = Uint8Array.from(atob(base64String), (char) => char.charCodeAt(0));
  const view = new DataView(bytes.buffer);
  const safeCount = Math.min(count, Math.floor(bytes.byteLength / LOG_ENTRY_BYTES));
  const entries = [];

  for (let i = 0; i < safeCount; i++) {
    const offset = i * LOG_ENTRY_BYTES;
    entries.push({
      ts: view.getUint32(offset, true),
      state: view.getUint8(offset + 4),
      power: view.getInt16(offset + 5, true),
      pwm: view.getUint8(offset + 7),
      temp: view.getInt16(offset + 8, true),
    });
  }
  return entries;
}
