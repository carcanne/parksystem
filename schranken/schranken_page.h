#pragma once

const char SCHRANKEN_PAGE[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Schranken</title>
<style>
  * { box-sizing: border-box; }
  body { font-family: system-ui, sans-serif; margin: 0; background: #f2f4f7; color: #1d2433; }
  header { background: #1d3557; color: #fff; padding: 12px 20px; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 8px; }
  header h1 { margin: 0; font-size: 22px; }
  nav { display: flex; gap: 4px; }
  nav a { color: #fff; text-decoration: none; padding: 6px 12px; border-radius: 6px; background: rgba(255,255,255,.12); }
  nav a.active { background: #fff; color: #1d3557; font-weight: 600; }
  .conn { font-size: 14px; padding: 4px 12px; border-radius: 999px; background: #9aa4b2; }
  .conn.ok { background: #2a9d8f; }
  .conn.down { background: #e09f00; color: #1d2433; }
  main { max-width: 960px; margin: 0 auto; padding: 16px; display: grid; gap: 16px; }
  .intro { margin: 0; color: #5b6574; }
  .gates { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 16px; }
  section { background: #fff; border-radius: 10px; padding: 18px; box-shadow: 0 1px 3px rgba(0,0,0,.08); }
  h2 { margin: 0 0 12px; font-size: 20px; }
  .state { text-align: center; font-size: 22px; font-weight: 800; letter-spacing: 1px; padding: 10px; border-radius: 8px;
           background: #fde8e8; color: #8a1c1c; transition: background .3s; }
  .state.open { background: #e3f6ec; color: #135c36; }
  .gate { height: 64px; position: relative; margin: 10px 0 14px; }
  .post { position: absolute; left: 10px; bottom: 0; width: 14px; height: 48px; background: #5b6574; border-radius: 3px; }
  .boom { position: absolute; left: 17px; bottom: 40px; width: 220px; height: 9px; border-radius: 4px; transform-origin: 0 50%;
          background: repeating-linear-gradient(90deg, #d62828 0 20px, #fff 20px 40px); border: 1px solid #b0b0b0; transition: transform .6s; }
  .boom.open { transform: rotate(-75deg); }
  form { display: flex; gap: 8px; flex-wrap: wrap; }
  input { font-size: 18px; padding: 10px 12px; border: 1px solid #b8c0cc; border-radius: 8px; flex: 1; min-width: 150px; }
  button { font-size: 17px; padding: 10px 18px; border: 0; border-radius: 8px; background: #1d3557; color: #fff; cursor: pointer; }
  button:disabled { background: #9aa4b2; }
  .msg { margin-top: 12px; padding: 12px 14px; border-radius: 8px; display: none; font-size: 16px; }
  .msg.ok { display: block; background: #e3f6ec; color: #135c36; }
  .msg.err { display: block; background: #fde8e8; color: #8a1c1c; }
  .msg.conn { display: block; background: #fff1d6; color: #7a4b00; border-left: 5px solid #e09f00; }
  .msg.wait { display: block; background: #eef1f5; color: #5b6574; }
  .hint { color: #5b6574; font-size: 14px; margin: 0 0 12px; }
  table { width: 100%; border-collapse: collapse; font-size: 15px; }
  th, td { text-align: left; padding: 8px; border-bottom: 1px solid #e3e7ec; }
  tbody tr.vehicle { cursor: pointer; }
  tbody tr.vehicle:hover { background: #f0f4fa; }
  .paid { color: #135c36; font-weight: 600; }
  .open-fee { color: #8a1c1c; font-weight: 600; }
</style>
</head>
<body>
<header>
  <h1>Schranken</h1>
  <nav>
    <a href="/" class="active">Schranken</a>
    <a href="http://192.168.42.2/">Kasse</a>
    <a href="http://192.168.42.2/automat">Automat</a>
  </nav>
  <span class="conn" id="conn">Verbinde ...</span>
</header>
<main>
  <p class="intro">Hier wird die Kamera simuliert: Kennzeichen eintippen und auf <b>Einfahren</b> oder
    <b>Ausfahren</b> klicken. Die Schranke öffnet nur, wenn die Kasse zustimmt, und schliesst nach
    5 Sekunden von selbst.</p>

  <div class="gates">
    <section>
      <h2>Einfahrt</h2>
      <div class="state" id="entryState">GESCHLOSSEN</div>
      <div class="gate"><div class="post"></div><div class="boom" id="entryBoom"></div></div>
      <form data-gate="entry">
        <input placeholder="z.B. AG 123456" autocomplete="off">
        <button type="submit">Einfahren</button>
      </form>
      <div class="msg" id="entryMsg"></div>
    </section>
    <section>
      <h2>Ausfahrt</h2>
      <div class="state" id="exitState">GESCHLOSSEN</div>
      <div class="gate"><div class="post"></div><div class="boom" id="exitBoom"></div></div>
      <form data-gate="exit">
        <input placeholder="z.B. AG 123456" autocomplete="off" id="exitInput">
        <button type="submit">Ausfahren</button>
      </form>
      <div class="msg" id="exitMsg"></div>
    </section>
  </div>

  <section>
    <h2>Fahrzeuge im Parkhaus <span id="count"></span></h2>
    <p class="hint">Daten der Kasse, Uhrzeit Kasse: <span id="now">-</span>. Klick auf ein Fahrzeug
      übernimmt das Kennzeichen in die Ausfahrt.</p>
    <table>
      <thead><tr><th>Kennzeichen</th><th>Eingefahren</th><th>Status</th></tr></thead>
      <tbody id="list"><tr><td colspan="3">Lade ...</td></tr></tbody>
    </table>
  </section>
</main>
<script>
async function getJson(url, options = {}) {
  try {
    const res = await fetch(url, { ...options, signal: AbortSignal.timeout(9000) });
    return await res.json();
  } catch (e) {
    return { ok: false, code: 'CONNECTION', message: 'Verbindungsfehler: Der Schranken-Pico ist nicht erreichbar. Ist das Notebook im WLAN "Parksystem"?' };
  }
}

function showMsg(gate, r) {
  const el = document.getElementById(gate + 'Msg');
  el.className = 'msg ' + (r.ok ? 'ok' : (r.code === 'CONNECTION' ? 'conn' : 'err'));
  el.textContent = (r.ok ? '✔ ' : r.code === 'CONNECTION' ? '⚠ ' : '✖ ') + r.message;
}

for (const form of document.querySelectorAll('form[data-gate]')) {
  form.addEventListener('submit', async e => {
    e.preventDefault();
    const gate = form.dataset.gate;
    const input = form.querySelector('input');
    const button = form.querySelector('button');
    const msg = document.getElementById(gate + 'Msg');
    msg.className = 'msg wait';
    msg.textContent = 'Frage bei der Kasse nach ...';
    button.disabled = true;
    const r = await getJson('/api/' + gate, { method: 'POST', body: new URLSearchParams({ plate: input.value, epoch: Math.floor(Date.now() / 1000) }) });
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
  state.textContent = open ? 'OFFEN' : 'GESCHLOSSEN';
}

async function refreshBarriers() {
  const r = await getJson('/api/barriers');
  setBarrier('entry', r.entry === true);
  setBarrier('exit', r.exit === true);
}

function setConn(ok) {
  const el = document.getElementById('conn');
  el.className = 'conn ' + (ok ? 'ok' : 'down');
  el.textContent = ok ? 'Kasse verbunden' : 'Kasse nicht erreichbar';
}

async function refreshList() {
  const r = await getJson('/api/vehicles');
  const tbody = document.getElementById('list');
  setConn(r.ok);
  if (!r.ok) {
    tbody.innerHTML = '<tr><td colspan="3">Kasse nicht erreichbar, keine Fahrzeugliste verfügbar</td></tr>';
    return;
  }
  document.getElementById('now').textContent = r.now;
  document.getElementById('count').textContent = '(' + r.vehicles.length + ')';
  tbody.innerHTML = '';
  if (r.vehicles.length === 0) {
    tbody.innerHTML = '<tr><td colspan="3">Keine Fahrzeuge im Parkhaus</td></tr>';
    return;
  }
  for (const v of r.vehicles) {
    const tr = document.createElement('tr');
    tr.className = 'vehicle';
    for (const text of [v.plate, v.entry]) {
      const td = document.createElement('td');
      td.textContent = text;
      tr.appendChild(td);
    }
    const status = document.createElement('td');
    status.className = v.paid ? 'paid' : 'open-fee';
    status.textContent = v.paid ? 'bezahlt' : 'offen ' + v.open;
    tr.appendChild(status);
    tr.addEventListener('click', () => {
      const input = document.getElementById('exitInput');
      input.value = v.plate;
      input.focus();
    });
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
