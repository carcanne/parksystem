// Weboberflaeche des Schranken-Picos: simuliert die Kamera an Ein- und
// Ausfahrt und zeigt die Fahrzeugliste der Kasse.
#pragma once

const char SCHRANKEN_PAGE[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Schranken</title>
<style>
  body { font-family: system-ui, sans-serif; margin: 0; background: #f2f4f7; color: #1d2433; }
  header { background: #1d3557; color: #fff; padding: 14px 20px; display: flex; justify-content: space-between; align-items: baseline; flex-wrap: wrap; gap: 8px; }
  header h1 { margin: 0; font-size: 22px; }
  main { max-width: 900px; margin: 0 auto; padding: 16px; display: grid; gap: 16px; }
  .gates { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; }
  section { background: #fff; border-radius: 8px; padding: 16px; box-shadow: 0 1px 3px rgba(0,0,0,.08); }
  h2 { margin: 0 0 12px; font-size: 17px; display: flex; justify-content: space-between; align-items: center; }
  form { display: flex; gap: 8px; flex-wrap: wrap; }
  input { font-size: 16px; padding: 8px 10px; border: 1px solid #b8c0cc; border-radius: 6px; flex: 1; min-width: 140px; }
  button { font-size: 16px; padding: 8px 16px; border: 0; border-radius: 6px; background: #1d3557; color: #fff; cursor: pointer; }
  button:disabled { background: #9aa4b2; }
  .state { font-size: 13px; padding: 3px 10px; border-radius: 999px; background: #fde8e8; color: #8a1c1c; }
  .state.open { background: #e3f6ec; color: #135c36; }
  .gate { height: 60px; position: relative; margin: 6px 0 14px; }
  .post { position: absolute; left: 10px; bottom: 0; width: 14px; height: 46px; background: #5b6574; border-radius: 3px; }
  .boom { position: absolute; left: 17px; bottom: 38px; width: 200px; height: 8px; border-radius: 4px; transform-origin: 0 50%;
          background: repeating-linear-gradient(90deg, #d62828 0 20px, #fff 20px 40px); border: 1px solid #b0b0b0; transition: transform .6s; }
  .boom.open { transform: rotate(-75deg); }
  .msg { margin-top: 12px; padding: 10px 12px; border-radius: 6px; display: none; }
  .msg.ok { display: block; background: #e3f6ec; color: #135c36; }
  .msg.err { display: block; background: #fde8e8; color: #8a1c1c; }
  .msg.conn { display: block; background: #fff1d6; color: #7a4b00; border-left: 4px solid #e09f00; }
  table { width: 100%; border-collapse: collapse; font-size: 15px; }
  th, td { text-align: left; padding: 6px 8px; border-bottom: 1px solid #e3e7ec; }
  .paid { color: #135c36; font-weight: 600; }
  .open-fee { color: #8a1c1c; font-weight: 600; }
</style>
</head>
<body>
<header>
  <h1>Parkhaus Schranken</h1>
  <span>Uhrzeit Kasse: <span id="now">-</span></span>
</header>
<main>
  <div class="gates">
    <section>
      <h2>Einfahrt <span class="state" id="entryState">geschlossen</span></h2>
      <div class="gate"><div class="post"></div><div class="boom" id="entryBoom"></div></div>
      <form data-gate="entry">
        <input placeholder="Kennzeichen (Kamera)" autocomplete="off">
        <button type="submit">Einfahren</button>
      </form>
      <div class="msg" id="entryMsg"></div>
    </section>
    <section>
      <h2>Ausfahrt <span class="state" id="exitState">geschlossen</span></h2>
      <div class="gate"><div class="post"></div><div class="boom" id="exitBoom"></div></div>
      <form data-gate="exit">
        <input placeholder="Kennzeichen (Kamera)" autocomplete="off">
        <button type="submit">Ausfahren</button>
      </form>
      <div class="msg" id="exitMsg"></div>
    </section>
  </div>

  <section>
    <h2>Fahrzeugliste</h2>
    <table>
      <thead><tr><th>Kennzeichen</th><th>Einfahrt</th><th>Status</th></tr></thead>
      <tbody id="list"><tr><td colspan="3">Lade ...</td></tr></tbody>
    </table>
  </section>
</main>
<script>
async function getJson(url, options) {
  try {
    const res = await fetch(url, options);
    return await res.json();
  } catch (e) {
    return { ok: false, code: 'CONNECTION', message: 'Verbindungsfehler: Der Schranken-Pico ist nicht erreichbar.' };
  }
}

// Meldung bleibt stehen, bis an dieser Schranke die naechste Eingabe erfolgt (US-6)
function showMsg(gate, r) {
  const el = document.getElementById(gate + 'Msg');
  el.className = 'msg ' + (r.ok ? 'ok' : (r.code === 'CONNECTION' ? 'conn' : 'err'));
  el.textContent = r.message;
}

for (const form of document.querySelectorAll('form[data-gate]')) {
  form.addEventListener('submit', async e => {
    e.preventDefault();
    const gate = form.dataset.gate;
    const input = form.querySelector('input');
    const button = form.querySelector('button');
    const msg = document.getElementById(gate + 'Msg');
    msg.className = 'msg';
    button.disabled = true;
    const r = await getJson('/api/' + gate, { method: 'POST', body: new URLSearchParams({ plate: input.value }) });
    button.disabled = false;
    showMsg(gate, r);
    if (r.ok) input.value = '';
    refreshBarriers();
    refreshList();
  });
}

function setBarrier(gate, open) {
  document.getElementById(gate + 'Boom').classList.toggle('open', open);
  const state = document.getElementById(gate + 'State');
  state.classList.toggle('open', open);
  state.textContent = open ? 'offen' : 'geschlossen';
}

async function refreshBarriers() {
  const r = await getJson('/api/barriers');
  if (r.entry === undefined) return;
  setBarrier('entry', r.entry);
  setBarrier('exit', r.exit);
}

async function refreshList() {
  const r = await getJson('/api/vehicles');
  const tbody = document.getElementById('list');
  if (!r.ok) {
    tbody.innerHTML = '<tr><td colspan="3">Kasse nicht erreichbar, keine Fahrzeugliste verfügbar</td></tr>';
    return;
  }
  document.getElementById('now').textContent = r.now;
  tbody.innerHTML = '';
  if (r.vehicles.length === 0) {
    tbody.innerHTML = '<tr><td colspan="3">Keine Fahrzeuge im Parkhaus</td></tr>';
    return;
  }
  for (const v of r.vehicles) {
    const tr = document.createElement('tr');
    for (const text of [v.plate, v.entry]) {
      const td = document.createElement('td');
      td.textContent = text;
      tr.appendChild(td);
    }
    const status = document.createElement('td');
    status.className = v.paid ? 'paid' : 'open-fee';
    status.textContent = v.paid ? 'bezahlt' : 'offen ' + v.open;
    tr.appendChild(status);
    tbody.appendChild(tr);
  }
}

async function pollBarriers() { await refreshBarriers(); setTimeout(pollBarriers, 1000); }
async function pollList() { await refreshList(); setTimeout(pollList, 3000); }
pollBarriers();
pollList();
</script>
</body>
</html>
)HTML";
