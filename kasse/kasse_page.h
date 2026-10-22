#pragma once

const char KASSE_PAGE[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kasse</title>
<style>
  * { box-sizing: border-box; }
  body { font-family: system-ui, sans-serif; margin: 0; background: #f2f4f7; color: #1d2433; }
  header { background: #1d3557; color: #fff; padding: 12px 20px; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 8px; }
  header h1 { margin: 0; font-size: 22px; }
  nav { display: flex; gap: 4px; }
  nav a { color: #fff; text-decoration: none; padding: 6px 12px; border-radius: 6px; background: rgba(255,255,255,.12); }
  nav a.active { background: #fff; color: #1d3557; font-weight: 600; }
  .clock { font-size: 14px; opacity: .85; }
  main { max-width: 860px; margin: 0 auto; padding: 16px; display: grid; gap: 16px; }
  section { background: #fff; border-radius: 10px; padding: 18px; box-shadow: 0 1px 3px rgba(0,0,0,.08); }
  h2 { margin: 0 0 6px; font-size: 18px; }
  .hint { color: #5b6574; font-size: 14px; margin: 0 0 12px; }
  form { display: flex; gap: 8px; flex-wrap: wrap; }
  input { font-size: 18px; padding: 10px 12px; border: 1px solid #b8c0cc; border-radius: 8px; flex: 1; min-width: 150px; }
  button { font-size: 16px; padding: 10px 18px; border: 0; border-radius: 8px; background: #1d3557; color: #fff; cursor: pointer; }
  button:disabled { background: #9aa4b2; cursor: not-allowed; }
  .quick { display: flex; gap: 6px; flex-wrap: wrap; margin-top: 10px; align-items: center; }
  .quick button { background: #e6ebf2; color: #1d3557; padding: 6px 12px; font-size: 14px; }
  .result { margin-top: 16px; border: 2px solid #e3e7ec; border-radius: 10px; padding: 16px; display: grid; gap: 12px; }
  .result[hidden] { display: none; }
  .result.open { border-color: #e9a3a3; background: #fff8f8; }
  .result.paid { border-color: #9fd8b8; background: #f5fcf8; }
  .amount { display: flex; justify-content: space-between; align-items: baseline; flex-wrap: wrap; gap: 8px; }
  .amount .plate { font-size: 22px; font-weight: 700; letter-spacing: .5px; }
  .amount .sum { font-size: 30px; font-weight: 800; }
  .facts { display: flex; gap: 24px; flex-wrap: wrap; color: #5b6574; font-size: 15px; }
  .facts b { color: #1d2433; }
  .badge { display: inline-block; padding: 3px 12px; border-radius: 999px; font-size: 14px; font-weight: 600; }
  .badge.open { background: #fde8e8; color: #8a1c1c; }
  .badge.paid { background: #e3f6ec; color: #135c36; }
  button.pay { background: #2a9d8f; font-size: 20px; padding: 14px; width: 100%; }
  .msg { margin-top: 12px; padding: 12px 14px; border-radius: 8px; display: none; font-size: 16px; }
  .msg.ok { display: block; background: #e3f6ec; color: #135c36; }
  .msg.err { display: block; background: #fde8e8; color: #8a1c1c; }
  .msg.warn { display: block; background: #fff1d6; color: #7a4b00; }
  table { width: 100%; border-collapse: collapse; font-size: 15px; }
  th, td { text-align: left; padding: 8px; border-bottom: 1px solid #e3e7ec; }
  tbody tr.vehicle { cursor: pointer; }
  tbody tr.vehicle:hover { background: #f0f4fa; }
  tbody tr.selected { background: #e6eefb; }
  .paid-text { color: #135c36; font-weight: 600; }
  .open-text { color: #8a1c1c; font-weight: 600; }
  body.automat main { max-width: 560px; }
  body.automat .kasse-only { display: none; }
</style>
</head>
<body>
<header>
  <h1 id="title">Kasse</h1>
  <nav>
    <a href="http://192.168.42.1/">Schranken</a>
    <a href="/" id="navKasse">Kasse</a>
    <a href="/automat" id="navAutomat">Automat</a>
  </nav>
  <span class="clock">Uhrzeit Kasse: <span id="now">-</span></span>
</header>
<main>
  <div class="msg" id="timeMsg"></div>

  <section>
    <h2>Parkgebühr bezahlen</h2>
    <p class="hint" id="payHint">Kennzeichen eingeben oder unten in der Liste anklicken.</p>
    <form id="feeForm">
      <input id="feePlate" placeholder="z.B. AG 123456" autocomplete="off">
      <button type="submit">Gebühr anzeigen</button>
    </form>

    <div class="result" id="result" hidden>
      <div class="amount">
        <span class="plate" id="rPlate"></span>
        <span class="badge" id="rBadge"></span>
      </div>
      <div class="facts">
        <span>Eingefahren: <b id="rEntry"></b></span>
        <span>Parkdauer: <b id="rMinutes"></b></span>
        <span id="rPaidLine">Bezahlt: <b id="rPaid"></b></span>
      </div>
      <div class="amount">
        <span>Offener Betrag</span>
        <span class="sum" id="rOpen"></span>
      </div>
      <button class="pay" id="payBtn">Bezahlen</button>
    </div>
    <div class="msg" id="feeMsg"></div>
  </section>

  <section class="kasse-only">
    <h2>Testfeld: Parkdauer simulieren</h2>
    <p class="hint">Setzt die Einfahrtszeit eines Fahrzeugs auf einige Minuten vor jetzt, damit die
      Gebühr ohne Warten geprüft werden kann (TF-12 bis TF-16).</p>
    <form id="testForm">
      <input id="testPlate" placeholder="Kennzeichen" autocomplete="off">
      <input id="testMinutes" type="number" min="0" step="1" placeholder="Minuten" style="max-width:130px">
      <button type="submit">Setzen</button>
    </form>
    <div class="quick">
      <span class="hint" style="margin:0">Schnellwahl:</span>
      <button type="button" data-min="0">0 Min.</button>
      <button type="button" data-min="40">40 Min.</button>
      <button type="button" data-min="60">60 Min.</button>
      <button type="button" data-min="61">61 Min.</button>
      <button type="button" data-min="121">121 Min.</button>
    </div>
    <div class="msg" id="testMsg"></div>
  </section>

  <section class="kasse-only">
    <h2>Fahrzeuge im Parkhaus <span id="count"></span></h2>
    <p class="hint">Klick auf ein Fahrzeug zeigt oben seine Gebühr an.</p>
    <table>
      <thead><tr><th>Kennzeichen</th><th>Eingefahren</th><th>Dauer</th><th>Offen</th><th>Status</th></tr></thead>
      <tbody id="list"><tr><td colspan="5">Lade ...</td></tr></tbody>
    </table>
  </section>
</main>
<script>
const isAutomat = location.pathname.startsWith('/automat');
const role = isAutomat ? 'Bezahlautomat' : 'Kasse';
document.title = role;
document.getElementById('title').textContent = role;
document.getElementById(isAutomat ? 'navAutomat' : 'navKasse').classList.add('active');
if (isAutomat) {
  document.body.classList.add('automat');
  document.getElementById('payHint').textContent = 'Kennzeichen eingeben und Gebühr anzeigen lassen.';
}

let current = null;

function showMsg(id, ok, text) {
  const el = document.getElementById(id);
  el.className = 'msg ' + (ok ? 'ok' : 'err');
  el.textContent = text;
}

async function api(method, path, params) {
  const body = new URLSearchParams(params);
  const url = method === 'GET' ? path + '?' + body : path;
  const options = { signal: AbortSignal.timeout(5000) };
  if (method !== 'GET') Object.assign(options, { method, body });
  try {
    const res = await fetch(url, options);
    return await res.json();
  } catch (e) {
    return { ok: false, message: 'Die Kasse ist nicht erreichbar. Ist das Notebook im WLAN "Parksystem"?' };
  }
}

function showVehicle(v) {
  const box = document.getElementById('result');
  box.hidden = !v;
  if (!v) { current = null; markSelected(); return; }
  current = v.plate;
  box.className = 'result ' + (v.paid ? 'paid' : 'open');
  document.getElementById('rPlate').textContent = v.plate;
  const badge = document.getElementById('rBadge');
  badge.className = 'badge ' + (v.paid ? 'paid' : 'open');
  badge.textContent = v.paid ? 'bezahlt' : 'nicht bezahlt';
  document.getElementById('rEntry').textContent = v.entry;
  document.getElementById('rMinutes').textContent = v.minutes + ' Min.';
  document.getElementById('rPaidLine').hidden = !v.paid;
  document.getElementById('rPaid').textContent = v.paidAmount;
  document.getElementById('rOpen').textContent = v.open;
  const btn = document.getElementById('payBtn');
  btn.disabled = v.paid;
  btn.textContent = v.paid ? 'Bereits bezahlt' : v.open + ' bezahlen';
  document.getElementById('testPlate').value = v.plate;
  markSelected();
}

async function queryFee(plate) {
  document.getElementById('feePlate').value = plate;
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

async function setTestTime(minutes) {
  const plate = document.getElementById('testPlate').value;
  const r = await api('POST', '/api/testtime', { plate, minutes });
  showMsg('testMsg', r.ok, r.message);
  if (r.ok) {
    showVehicle(r.vehicle);
    document.getElementById('feePlate').value = r.vehicle.plate;
  }
  refreshList();
}

document.getElementById('testForm').addEventListener('submit', e => {
  e.preventDefault();
  setTestTime(document.getElementById('testMinutes').value);
});

for (const b of document.querySelectorAll('.quick button')) {
  b.addEventListener('click', () => {
    document.getElementById('testMinutes').value = b.dataset.min;
    setTestTime(b.dataset.min);
  });
}

function markSelected() {
  for (const tr of document.querySelectorAll('#list tr.vehicle')) {
    tr.classList.toggle('selected', tr.dataset.plate === current);
  }
}

async function refreshList() {
  const r = await api('GET', '/api/vehicles', {});
  const tbody = document.getElementById('list');
  if (!r.ok) {
    tbody.innerHTML = '<tr><td colspan="5">Kasse nicht erreichbar</td></tr>';
    return;
  }
  document.getElementById('now').textContent = r.now;
  document.getElementById('count').textContent = '(' + r.vehicles.length + ')';
  tbody.innerHTML = '';
  if (r.vehicles.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5">Keine Fahrzeuge im Parkhaus</td></tr>';
  }
  for (const v of r.vehicles) {
    const tr = document.createElement('tr');
    tr.className = 'vehicle';
    tr.dataset.plate = v.plate;
    for (const text of [v.plate, v.entry, v.minutes + ' Min.', v.open]) {
      const td = document.createElement('td');
      td.textContent = text;
      tr.appendChild(td);
    }
    const status = document.createElement('td');
    status.className = v.paid ? 'paid-text' : 'open-text';
    status.textContent = v.paid ? 'bezahlt' : 'offen';
    tr.appendChild(status);
    tr.addEventListener('click', () => queryFee(v.plate));
    tbody.appendChild(tr);
    if (v.plate === current) showVehicle(v);
  }
  markSelected();
}

async function setClock() {
  const r = await api('POST', '/api/time', { epoch: Math.floor(Date.now() / 1000) });
  const el = document.getElementById('timeMsg');
  if (r.ok) { el.className = 'msg'; return; }
  el.className = 'msg warn';
  el.textContent = 'Uhrzeit konnte nicht gestellt werden: ' + r.message;
}

async function poll() {
  await refreshList();
  setTimeout(poll, 3000);
}
setClock().then(poll);
</script>
</body>
</html>
)HTML";
