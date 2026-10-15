// Weboberflaeche fuer Kasse (/) und Automat (/automat). Beide Pfade liefern
// dieselbe Seite, das Testfeld erscheint nur an der Kasse.
#pragma once

const char KASSE_PAGE[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kasse</title>
<style>
  body { font-family: system-ui, sans-serif; margin: 0; background: #f2f4f7; color: #1d2433; }
  header { background: #1d3557; color: #fff; padding: 14px 20px; display: flex; justify-content: space-between; align-items: baseline; flex-wrap: wrap; gap: 8px; }
  header h1 { margin: 0; font-size: 22px; }
  main { max-width: 820px; margin: 0 auto; padding: 16px; display: grid; gap: 16px; }
  section { background: #fff; border-radius: 8px; padding: 16px; box-shadow: 0 1px 3px rgba(0,0,0,.08); }
  h2 { margin: 0 0 12px; font-size: 17px; }
  form { display: flex; gap: 8px; flex-wrap: wrap; }
  input { font-size: 16px; padding: 8px 10px; border: 1px solid #b8c0cc; border-radius: 6px; flex: 1; min-width: 140px; }
  button { font-size: 16px; padding: 8px 16px; border: 0; border-radius: 6px; background: #1d3557; color: #fff; cursor: pointer; }
  button:disabled { background: #9aa4b2; cursor: not-allowed; }
  button.pay { background: #2a9d8f; }
  .msg { margin-top: 12px; padding: 10px 12px; border-radius: 6px; display: none; }
  .msg.ok { display: block; background: #e3f6ec; color: #135c36; }
  .msg.err { display: block; background: #fde8e8; color: #8a1c1c; }
  dl { display: grid; grid-template-columns: max-content 1fr; gap: 6px 16px; margin: 12px 0 0; }
  dt { color: #5b6574; }
  dd { margin: 0; font-weight: 600; }
  table { width: 100%; border-collapse: collapse; font-size: 15px; }
  th, td { text-align: left; padding: 6px 8px; border-bottom: 1px solid #e3e7ec; }
  .paid { color: #135c36; font-weight: 600; }
  .open { color: #8a1c1c; font-weight: 600; }
  .hint { color: #5b6574; font-size: 14px; margin: 0 0 10px; }
</style>
</head>
<body>
<header>
  <h1 id="title">Kasse</h1>
  <span>Uhrzeit Kasse: <span id="now">-</span></span>
</header>
<main>
  <section>
    <h2>Parkgebühr abfragen und bezahlen</h2>
    <form id="feeForm">
      <input id="feePlate" placeholder="Kennzeichen, z.B. AG 123456" autocomplete="off">
      <button type="submit">Abfragen</button>
    </form>
    <dl id="details" hidden>
      <dt>Kennzeichen</dt><dd id="dPlate"></dd>
      <dt>Einfahrt</dt><dd id="dEntry"></dd>
      <dt>Aufenthalt</dt><dd id="dMinutes"></dd>
      <dt>Status</dt><dd id="dStatus"></dd>
      <dt>Bezahlt</dt><dd id="dPaid"></dd>
      <dt>Offener Betrag</dt><dd id="dOpen"></dd>
    </dl>
    <div style="margin-top:12px"><button class="pay" id="payBtn" hidden>Bezahlen</button></div>
    <div class="msg" id="feeMsg"></div>
  </section>

  <section id="testSection">
    <h2>Testfeld Einfahrtszeit</h2>
    <p class="hint">Setzt die Einfahrtszeit eines Fahrzeugs auf einen Wert in Minuten vor der aktuellen Zeit (für TF-12 bis TF-16).</p>
    <form id="testForm">
      <input id="testPlate" placeholder="Kennzeichen" autocomplete="off">
      <input id="testMinutes" type="number" min="0" step="1" placeholder="Minuten vorher">
      <button type="submit">Einfahrtszeit setzen</button>
    </form>
    <div class="msg" id="testMsg"></div>
  </section>

  <section>
    <h2>Fahrzeuge im Parkhaus</h2>
    <table>
      <thead><tr><th>Kennzeichen</th><th>Einfahrt</th><th>Minuten</th><th>Offen</th><th>Status</th></tr></thead>
      <tbody id="list"><tr><td colspan="5">Lade ...</td></tr></tbody>
    </table>
  </section>
</main>
<script>
const isAutomat = location.pathname.startsWith('/automat');
const role = isAutomat ? 'Automat' : 'Kasse';
document.title = role;
document.getElementById('title').textContent = role;
if (isAutomat) document.getElementById('testSection').hidden = true;

let current = null;  // aktuell abgefragtes Kennzeichen

function showMsg(id, ok, text) {
  const el = document.getElementById(id);
  el.className = 'msg ' + (ok ? 'ok' : 'err');
  el.textContent = text;
}

async function api(method, path, params) {
  const body = new URLSearchParams(params);
  const url = method === 'GET' ? path + '?' + body : path;
  try {
    const res = await fetch(url, method === 'GET' ? {} : { method, body });
    return await res.json();
  } catch (e) {
    return { ok: false, message: 'Die Kasse ist nicht erreichbar. Bitte Seite neu laden.' };
  }
}

function showVehicle(v) {
  document.getElementById('details').hidden = !v;
  const btn = document.getElementById('payBtn');
  if (!v) { btn.hidden = true; current = null; return; }
  current = v.plate;
  document.getElementById('dPlate').textContent = v.plate;
  document.getElementById('dEntry').textContent = v.entry;
  document.getElementById('dMinutes').textContent = v.minutes + ' Minuten';
  document.getElementById('dStatus').innerHTML = v.paid ? '<span class="paid">bezahlt</span>' : '<span class="open">offen</span>';
  document.getElementById('dPaid').textContent = v.paid ? v.paidAmount : '-';
  document.getElementById('dOpen').textContent = v.open;
  btn.hidden = false;
  btn.disabled = v.paid;
  btn.textContent = v.paid ? 'Bereits bezahlt' : 'Bezahlen (' + v.open + ')';
}

async function queryFee(plate) {
  const r = await api('GET', '/api/fee', { plate });
  showVehicle(r.vehicle);
  showMsg('feeMsg', r.ok, r.message);
}

document.getElementById('feeForm').addEventListener('submit', e => {
  e.preventDefault();
  queryFee(document.getElementById('feePlate').value);
});

document.getElementById('payBtn').addEventListener('click', async () => {
  if (!current) return;
  const r = await api('POST', '/api/pay', { plate: current });
  if (r.vehicle) showVehicle(r.vehicle);
  showMsg('feeMsg', r.ok, r.message);
  refreshList();
});

document.getElementById('testForm').addEventListener('submit', async e => {
  e.preventDefault();
  const plate = document.getElementById('testPlate').value;
  const minutes = document.getElementById('testMinutes').value;
  const r = await api('POST', '/api/testtime', { plate, minutes });
  showMsg('testMsg', r.ok, r.message);
  if (r.ok && current === r.vehicle.plate) showVehicle(r.vehicle);
  refreshList();
});

async function refreshList() {
  const r = await api('GET', '/api/vehicles', {});
  const tbody = document.getElementById('list');
  if (!r.ok) { tbody.innerHTML = '<tr><td colspan="5">Kasse nicht erreichbar</td></tr>'; return; }
  document.getElementById('now').textContent = r.now;
  tbody.innerHTML = '';
  if (r.vehicles.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5">Keine Fahrzeuge im Parkhaus</td></tr>';
    return;
  }
  for (const v of r.vehicles) {
    const tr = document.createElement('tr');
    for (const text of [v.plate, v.entry, v.minutes, v.open]) {
      const td = document.createElement('td');
      td.textContent = text;
      tr.appendChild(td);
    }
    const status = document.createElement('td');
    status.className = v.paid ? 'paid' : 'open';
    status.textContent = v.paid ? 'bezahlt' : 'offen';
    tr.appendChild(status);
    tbody.appendChild(tr);
  }
}

async function poll() {
  await refreshList();
  setTimeout(poll, 3000);
}
poll();
</script>
</body>
</html>
)HTML";
