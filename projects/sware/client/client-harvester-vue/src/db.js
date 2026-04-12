import Dexie from 'dexie';

export const db = new Dexie('EnergieJunkiesDB');

// Wir definieren nur die Primärschlüssel (ts ist der Zeitstempel)
db.version(1).stores({
    logs: 'ts, state, power, temp'
});