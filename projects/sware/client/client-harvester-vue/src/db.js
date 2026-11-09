import Dexie from 'dexie';

export const db = new Dexie('EnergieJunkiesDB');

// Wir definieren nur die Primärschlüssel (ts ist der Zeitstempel)
db.version(1).stores({
    logs: 'ts, state, power, temp'
});

db.version(2).stores({
    logs: 'ts, state, power, temp',
    demandIntervals: '&start, complete',
    meta: '&key'
});
