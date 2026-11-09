import { db } from '../db';

const CURSOR = 'demandLastStoredStart';
let seenStart = 0;

export async function storeDemandSnapshot(demand) {
  const interval = demand?.lastCompleted;
  const start = Number(interval?.start);
  if (!demand?.enabled || !Number.isInteger(start) || start <= 0 || start <= seenStart) return;
  await db.transaction('rw', db.demandIntervals, db.meta, async () => {
    const previous = Number((await db.meta.get(CURSOR))?.value || 0);
    if (start <= previous) { seenStart = Math.max(seenStart, start); return; }
    await db.demandIntervals.put({
      start,
      end: start + 900,
      complete: interval.complete === true,
      observedSeconds: Number(interval.observedSeconds || 0),
      importEnergyWh: interval.importEnergyWh == null ? null : Number(interval.importEnergyWh),
      averageImportW: interval.complete && interval.averageImportW != null
        ? Number(interval.averageImportW) : null,
      limitW: Number(demand.limitW),
      missingBefore: previous > 0 ? Math.max(0, Math.round((start - previous) / 900) - 1) : 0
    });
    await db.meta.put({ key: CURSOR, value: start });
    seenStart = start;
  });
}

export async function clearDemandHistory() {
  // Keep the cursor: otherwise the server's repeated lastCompleted snapshot
  // would immediately recreate a record the user just deleted.
  await db.demandIntervals.clear();
}
