// app.js -- Flight Runway Management System
// Minimal JS: saari logic C++ engine mein hai, Python API serve karta hai, ye sirf DOM handle karta hai.

const API = 'http://localhost:5000';
let S = { flights: [], runways: [], history: [] };
let _filter = 'all', _delayId = null, _allocId = null;

// API helper -- ye aaise hota hai central fetch wrapper
async function api(path, body, method = 'POST') {
  try {
    const res = await fetch(`${API}/${path}`, {
      method,
      headers: { 'Content-Type': 'application/json' },
      ...(body ? { body: JSON.stringify(body) } : {})
    });
    return res.json();
  } catch {
    // server nahi mila -- ye error return karo
    return { error: 'Cannot reach server. Is api.py running?' };
  }
}

// Boot -- page load hone ke baad yahi sabse pehle chalta hai
window.addEventListener('DOMContentLoaded', async () => {
  await checkServer();
  await refresh();
  show('dashboard');
  setInterval(checkServer, 10000);  // har 10 sec mein server check karo
});

async function checkServer() {
  try {
    const r = await fetch(`${API}/status`);
    setBadge(r.ok);
    return r.ok;
  } catch {
    setBadge(false);
    return false;
  }
}

// server online/offline badge update karo -- indicator dot wala
function setBadge(online) {
  const el = document.getElementById('server-badge');
  el.textContent = online ? 'Server Online' : 'Server Offline';
  el.className = 'server-badge ' + (online ? 'online' : 'offline');
}

// Routing -- section show/hide karo navigation ke hisaab se
const SECTIONS = ['dashboard', 'flights', 'runways', 'allocate', 'history'];

function show(name) {
  SECTIONS.forEach(s => {
    document.getElementById('sec-' + s).style.display = s === name ? '' : 'none';
    document.getElementById('nav-' + s)?.classList.toggle('active', s === name);
  });
  document.getElementById('bc-page').textContent = name[0].toUpperCase() + name.slice(1);
  // kuch sections ko extra data chahiye -- ye aaise hota hai lazy load
  if (name === 'allocate') { buildAllocSelects(); buildQueueGrid(); }
  if (name === 'history')  buildHistory();
  if (name === 'runways')  buildRunwayDetail();
}

// Refresh -- server se fresh data lo aur saara UI update karo
async function refresh() {
  const d = await api('status', null, 'GET');
  if (d.error) return;
  S.flights = d.flights || [];
  S.runways = d.runways || [];
  S.history = d.history || [];
  buildStats();
  buildRunwayVisual();
  buildSidebarRunways();
  buildFlightTable('dash-flight-tbody', sorted(S.flights).slice(0, 10));
  buildFlightTable('flights-tbody', filtered());
  buildDelayedAlerts();
}

// Stats boxes -- top pe jo numbers dikhte hain woh yahan update hote hain
function buildStats() {
  const f = S.flights, r = S.runways;
  set('st-flights',   f.length);
  set('st-emergency', f.filter(x => x.type === 'emergency').length);
  set('st-allocated', f.filter(x => x.status === 'allocated').length);
  const dc = f.filter(x => x.status === 'delayed' || x.status === 'queued').length;
  set('st-delayed', dc);
  set('st-free', r.filter(x => x.available && !x.maintenance).length);
  // agar delayed flights hain toh box ko red highlight karo
  document.getElementById('st-delayed')?.closest('.stat-box')?.classList.toggle('has-delayed', dc > 0);
}

// Delayed alerts -- dashboard pe conflict/delay boxes banao
function buildDelayedAlerts() {
  const el = document.getElementById('conflict-area');
  if (!el) return;
  const delayed = S.flights.filter(f => (f.status === 'delayed' || f.status === 'queued') && f.active);
  el.innerHTML = delayed.map(f => {
    const isQ = f.status === 'queued';
    const rwy  = S.runways.find(r => r.queue?.some(q => q.id === f.id));
    const qpos = rwy ? rwy.queue.findIndex(q => q.id === f.id) + 1 : null;
    return `<div class="conflict-box">
      <strong>Delayed -- ${f.name} (ID:${f.id})</strong>
      ${isQ
        ? `Queued${qpos ? ' at position <strong>' + qpos + '</strong>' : ''}.`
        : `ETA: <strong>${f.time} min</strong>.
           <button class="btn btn-warning btn-sm" style="margin-left:10px;" onclick="openDelay('${f.id}')">Set Time</button>`}
    </div>`;
  }).join('');
}

// conflict notification -- C++ engine se aayi preemption/conflict info dikhao
function notifyConflict(conflict) {
  const el = document.getElementById('conflict-area');
  if (!el || !conflict) return;
  const b = document.createElement('div');
  b.className = 'conflict-box emergency-conflict';
  b.innerHTML = `<strong>C++ Scheduler: ${conflict.type === 'PREEMPTION' ? 'Emergency Preemption' : 'Time Conflict Resolved'}</strong><br>
    ${conflict.message}<br><small style="color:var(--muted);"> handled automatically.</small>
    <button class="btn btn-sm" style="margin-left:10px;float:right;" onclick="this.parentElement.remove()">X</button>`;
  el.insertBefore(b, el.firstChild);
}

// auto allocation success notification
function notifyAutoAlloc(msg) {
  const el = document.getElementById('conflict-area');
  if (!el) return;
  const b = document.createElement('div');
  b.className = 'conflict-box';
  b.style.borderLeftColor = 'var(--green)';
  b.innerHTML = `<strong>Auto-Allocation</strong><br>${msg}
    <button class="btn btn-sm" style="margin-left:10px;float:right;" onclick="this.parentElement.remove()">X</button>`;
  el.insertBefore(b, el.firstChild);
}

// Runway visual -- dashboard ka main runway status grid
function buildRunwayVisual() {
  const el = document.getElementById('runway-visual');
  if (!el) return;
  if (!S.runways.length) {
    el.innerHTML = '<p style="color:var(--muted);font-size:.84rem;">No runways added yet. Use <strong>+ Add Runway</strong>.</p>';
    return;
  }
  el.innerHTML = S.runways.map(r => {
    const cf  = r.current_flight ? flightById(r.current_flight) : null;
    const cls = r.maintenance ? 'maintenance' : !r.available ? (cf?.type === 'emergency' ? 'emergency' : 'busy') : 'free';
    const sl  = r.maintenance ? 'MAINTENANCE' : r.available ? 'FREE' : 'BUSY';
    const bc  = r.maintenance ? 'badge-maint' : r.available ? 'badge-free' : 'badge-busy';

    // queue mein waiting flights ki list
    const qHtml = r.queue?.length
      ? '<div class="rwy-queue-list">' + r.queue.map((qf, i) =>
          `<div class="rwy-queue-item${qf.type === 'emergency' ? ' q-emerg' : ''}">
            <span class="q-num">${i+1}</span>
            <span>${qf.type === 'emergency' ? '[EMG] ' : '[NRM] '}${qf.name} (ETA:${qf.time}min)</span>
          </div>`).join('') + '</div>' : '';

    return `<div class="runway-card ${cls}">
      <div class="rwy-card-head"><span>${r.name}</span><span class="badge ${bc}">${sl}</span></div>
      <div class="rwy-card-body">
        ${cf
          ? `<div class="rwy-field">Flight: <span>${cf.name}</span></div>
             <div class="rwy-field">Type: <span>${cf.type === 'emergency' ? 'EMERGENCY' : 'Normal'}</span></div>
             <div class="rwy-field">ETA: <span>${cf.time} min</span></div>
             ${cf.departure_time >= 0 ? `<div class="rwy-field">Dep: <span>${cf.departure_time} min</span></div>` : ''}`
          : r.maintenance
            ? `<div class="rwy-field" style="color:var(--reserved);">Under Maintenance</div>`
            : `<div class="rwy-field" style="color:var(--green);">Available for landing</div>`}
        ${r.queue?.length ? `<div class="rwy-field" style="margin-top:4px;">Queue: <span>${r.queue.length} waiting</span></div>${qHtml}` : ''}
      </div>
    </div>`;
  }).join('');
}

// sidebar mein chhoti runway status list
function buildSidebarRunways() {
  const el = document.getElementById('sidebar-rwy-status');
  if (!el) return;
  if (!S.runways.length) { el.innerHTML = '<p style="color:var(--muted);font-size:.82rem;">No runways.</p>'; return; }
  el.innerHTML = S.runways.map(r =>
    `<div style="font-size:.82rem;padding:4px 0;border-bottom:1px solid #eef2f6;display:flex;justify-content:space-between;">
      <span>${r.maintenance ? '[M]' : r.available ? '[F]' : '[B]'} ${r.name}</span>
      <span style="color:var(--muted);font-size:.78rem;">${r.maintenance ? 'MAINT' : r.available ? 'FREE' : 'BUSY'}</span>
    </div>`
  ).join('');
}

// Runway detail page -- full info with queue tables
function buildRunwayDetail() {
  const el = document.getElementById('runway-detail-grid');
  if (!el) return;
  if (!S.runways.length) { el.innerHTML = '<p style="color:var(--muted);">No runways added. Click <strong>+ Add Runway</strong>.</p>'; return; }
  el.innerHTML = S.runways.map(r => {
    const cf  = r.current_flight ? flightById(r.current_flight) : null;
    const bc  = r.maintenance ? 'badge-maint' : r.available ? 'badge-free' : 'badge-busy';
    const st  = r.maintenance ? 'MAINTENANCE' : r.available ? 'FREE' : 'BUSY';
    const qRows = (r.queue || []).map((qf, i) =>
      `<tr><td><strong>#${i+1}</strong></td><td>${qf.name}</td><td>${typeBadge(qf.type)}</td>
       <td>${qf.time} min</td><td>${qf.departure_time >= 0 ? qf.departure_time + ' min' : '--'}</td>
       <td>${statusBadge(qf.status)}</td></tr>`
    ).join('');

    return `<div class="panel" style="margin-bottom:10px;">
      <div class="panel-head">
        <div class="panel-head-left">${r.name} (ID: ${r.id})</div>
        <div style="display:flex;gap:6px;align-items:center;">
          <span class="badge ${bc}">${st}</span>
          <button class="btn btn-sm" style="background:var(--panel-head);border:1px solid var(--border);color:var(--reserved);" onclick="toggleMaint(${r.id})">Maint</button>
          ${!r.available && cf ? `<button class="btn btn-success btn-sm" onclick="clearRwy(${r.id})">Clear</button>` : ''}
          ${r.available ? `<button class="btn btn-danger btn-sm" onclick="delRunway(${r.id})">Remove</button>` : ''}
        </div>
      </div>
      <div class="panel-body">
        ${cf
          ? `<table class="data-table" style="margin-bottom:${qRows ? '12px' : '0'}"><tbody>
               <tr><td><strong>Current Flight</strong></td><td>${cf.name}</td></tr>
               <tr><td><strong>Type</strong></td><td>${typeBadge(cf.type)}</td></tr>
               <tr><td><strong>ETA</strong></td><td>${cf.time} min</td></tr>
               ${cf.departure_time >= 0 ? `<tr><td><strong>Departure</strong></td><td>${cf.departure_time} min</td></tr>` : ''}
               <tr><td><strong>Status</strong></td><td>${statusBadge(cf.status)}</td></tr>
             </tbody></table>`
          : r.maintenance
            ? `<p style="color:var(--reserved);font-size:.86rem;">Runway is under maintenance.</p>`
            : `<p style="color:var(--green);font-size:.86rem;">Runway is free and ready for landing.</p>`}
        ${qRows ? `<div style="margin-top:8px;"><strong style="font-size:.82rem;color:var(--blue);">Queue -- EDF Schedule (${r.queue.length} flights):</strong>
          <table class="data-table" style="margin-top:6px;"><thead>
            <tr><th>Pos</th><th>Name</th><th>Type</th><th>ETA</th><th>Departure</th><th>Status</th></tr>
          </thead><tbody>${qRows}</tbody></table></div>` : ''}
      </div>
    </div>`;
  }).join('');
}

// Flight table -- flights ki list render karo
function buildFlightTable(tbodyId, list) {
  const tbody = document.getElementById(tbodyId);
  if (!tbody) return;
  if (!list.length) {
    tbody.innerHTML = '<tr><td colspan="8" style="text-align:center;color:var(--muted);padding:18px;">No flights found.</td></tr>';
    return;
  }
  tbody.innerHTML = list.map(f => {
    const rwy = S.runways.find(r => r.id === f.runway) || S.runways.find(r => r.current_flight === f.id);
    const dep = (f.departure_time !== null && f.departure_time >= 0) ? `${f.departure_time} min` : `<span style="color:var(--muted)">--</span>`;
    return `<tr>
      <td><strong>${f.id}</strong></td><td>${f.name}</td><td>${typeBadge(f.type)}</td>
      <td>${f.time} min</td><td>${dep}</td><td>${statusBadge(f.status)}</td>
      <td>${rwy ? rwy.name : '<span style="color:var(--muted)">Unassigned</span>'}</td>
      <td>${actionBtns(f)}</td>
    </tr>`;
  }).join('');
}

// tab switch -- all/pending/allocated/delayed filter
function flightTab(tab) {
  _filter = tab;
  ['all', 'pending', 'allocated', 'delayed', 'departed'].forEach(t =>
    document.getElementById('ftab-' + t)?.classList.toggle('active', t === tab));
  buildFlightTable('flights-tbody', filtered());
}

function filtered() {
  const s = sorted(S.flights);
  if (_filter === 'all') return s;
  if (_filter === 'delayed') return s.filter(f => f.status === 'delayed' || f.status === 'queued');
  return s.filter(f => f.status === _filter);
}

// har flight ke liye action buttons -- status ke hisaab se alag alag
function actionBtns(f) {
  const b = [];
  if (f.status === 'pending')
    b.push(`<button class="btn ${f.type === 'emergency' ? 'btn-danger' : 'btn-primary'} btn-sm" onclick="openQuickAlloc('${f.id}')">${f.type === 'emergency' ? '[EMG]' : '[FLT]'} Allocate</button>`);
  if (f.status === 'delayed')
    b.push(`<button class="btn btn-warning btn-sm" onclick="openDelay('${f.id}')">Set Time</button>`);
  if (f.status === 'queued')
    b.push(`<span class="badge badge-delayed" style="font-size:.75rem;">Queued</span>`);
  if (f.status !== 'departed')
    b.push(`<button class="btn btn-danger btn-sm" onclick="delFlight(${f.id})">X</button>`);
  return b.join(' ');
}

// Modals -- open/close helpers
function openModal(id) { document.getElementById(id)?.classList.add('open'); }
function closeModal(id) { document.getElementById(id)?.classList.remove('open'); }

// modal ke bahar click karne pe close ho jaata hai
window.addEventListener('click', e => {
  ['modal-add-flight', 'modal-add-runway', 'modal-delay', 'modal-quick-alloc'].forEach(id => {
    if (e.target.id === id) closeModal(id);
  });
});

// Add flight form submit
async function addFlight() {
  const id        = parseInt(document.getElementById('f-id').value);
  const name      = document.getElementById('f-name').value.trim();
  const type      = document.getElementById('f-type').value;
  const time      = parseInt(document.getElementById('f-time').value);
  const depVal    = document.getElementById('f-departure').value;
  const departure = depVal !== '' ? parseInt(depVal) : -1;

  if (!id || !name || isNaN(time) || time < 0) {
    showMsg('add-flight-msg', 'ID, name and Arrival ETA are required. ETA must be >= 0.', 'error');
    return;
  }
  const r = await api('add_flight', { id, name, type, time, departure_time: departure });
  if (r.error) { showMsg('add-flight-msg', r.error, 'error'); return; }
  showMsg('add-flight-msg', r.message, 'success');
  clearFields(['f-id', 'f-name', 'f-time', 'f-departure']);
  await refresh();
  if (r.alloc_msg) notifyAutoAlloc(r.alloc_msg);
  setTimeout(() => closeModal('modal-add-flight'), 900);
}

// Add runway form submit
async function addRunway() {
  const id   = parseInt(document.getElementById('r-id').value);
  const name = document.getElementById('r-name').value.trim();
  if (!id || !name) { showMsg('add-runway-msg', 'ID and name are required.', 'error'); return; }
  const r = await api('add_runway', { id, name });
  if (r.error) { showMsg('add-runway-msg', r.error, 'error'); return; }
  showMsg('add-runway-msg', r.message, 'success');
  clearFields(['r-id', 'r-name']);
  await refresh();
  setTimeout(() => closeModal('modal-add-runway'), 900);
}

async function delRunway(rid) {
  if (!confirm(`Remove Runway ${rid}?`)) return;
  const r = await api('delete_runway', { id: rid });
  alert(r.message || r.error);
  await refresh();
  buildRunwayDetail();
}

async function toggleMaint(rid) {
  const r = await api('maintenance_runway', { id: rid });
  alert(r.message || r.error);
  await refresh();
  buildRunwayDetail();
}

async function clearRwy(rid) {
  const r = await api('clear_runway', { runway_id: rid });
  alert(r.message || r.error);
  await refresh();
  buildRunwayDetail();
  buildQueueGrid();
}

async function delFlight(fid) {
  if (!confirm(`Remove flight ${fid}?`)) return;
  const r = await api('delete_flight', { id: fid });
  alert(r.message || r.error);
  await refresh();
}

// Auto Allocate All -- ek button se saare pending flights allocate karo
async function autoAllocateAll() {
  const btn = document.getElementById('btn-auto-alloc-all');
  if (btn) { btn.disabled = true; btn.textContent = 'Allocating...'; }

  const r = await api('auto_allocate_all', {});

  if (btn) { btn.disabled = false; btn.textContent = 'Auto-Allocate All Pending'; }
  if (r.error) { alert(r.error); return; }

  await refresh();

  // summary banner dikhao -- kitne allocated, kitne queued
  const el = document.getElementById('conflict-area');
  if (el) {
    const b = document.createElement('div');
    b.className = 'conflict-box';
    b.style.borderLeftColor = 'var(--green)';
    const logHtml = r.log && r.log.length
      ? `<ul style="margin:6px 0 0 0;padding-left:16px;font-size:.82rem;">${r.log.map(l => `<li>${l}</li>`).join('')}</ul>`
      : '';
    b.innerHTML = `<strong>Auto-Allocation Complete</strong><br>${r.message}${logHtml}
      <button class="btn btn-sm" style="margin-left:10px;float:right;" onclick="this.parentElement.remove()">X</button>`;
    el.insertBefore(b, el.firstChild);
  }
  buildAllocSelects();
  buildQueueGrid();
}

// Quick alloc modal -- flight table ke Allocate button se khulta hai
function openQuickAlloc(fid) {
  _allocId = parseInt(fid);
  const f = S.flights.find(x => x.id === parseInt(fid));
  if (!f) return;
  document.getElementById('qa-flight-label').textContent =
    `${f.name} (ID:${f.id}) -- ${f.type.toUpperCase()} -- ETA ${f.time}min`;

  // runway dropdown populate karo
  const sel = document.getElementById('qa-rwy-sel');
  sel.innerHTML = '<option value="-1">AUTO -- Let engine pick best runway</option>';
  S.runways.filter(r => !r.maintenance).forEach(r => {
    const o = document.createElement('option');
    o.value = r.id;
    o.textContent = `${r.name} -- ${r.available ? 'Free' : 'Busy (queue pos ' + (r.queue.length + 1) + ')'}`;
    sel.appendChild(o);
  });

  // runway select hone pe info update karo
  sel.onchange = () => {
    const rid  = parseInt(sel.value);
    const info = document.getElementById('qa-rwy-info');
    if (rid === -1) {
      const f2 = S.flights.find(x => x.id === _allocId);
      info.innerHTML = f2?.type === 'emergency'
        ? `<span style="color:var(--orange);">AUTO: engine will find a free runway or preempt a normal flight.</span>`
        : `<span style="color:var(--green);">AUTO: engine will pick the shortest queue runway.</span>`;
      return;
    }
    const r = S.runways.find(x => x.id === rid);
    if (!r) { info.textContent = ''; return; }
    const f2 = S.flights.find(x => x.id === _allocId);
    info.innerHTML = r.available
      ? `<span style="color:var(--green);">Free -- flight will land immediately.</span>`
      : f2?.type === 'emergency'
        ? `<span style="color:var(--red);">Busy -- C++ emergency preemption will displace the normal flight.</span>`
        : `<span style="color:var(--orange);">Busy -- flight will be queued (EDF order).</span>`;
  };
  sel.dispatchEvent(new Event('change'));

  showMsg('quick-alloc-msg', '', '');
  openModal('modal-quick-alloc');
}

async function confirmQuickAlloc() {
  const ridVal = document.getElementById('qa-rwy-sel').value;
  const rid    = ridVal === '' ? -1 : parseInt(ridVal);
  const r = await api('allocate_runway', { flight_id: _allocId, runway_id: rid });
  if (r.error) { showMsg('quick-alloc-msg', r.error, 'error'); return; }
  showMsg('quick-alloc-msg', r.message, 'success');
  await refresh();
  if (r.conflict) notifyConflict(r.conflict);
  setTimeout(() => closeModal('modal-quick-alloc'), 1000);
}

// Allocate section -- manual allocation page ke selects banao
function buildAllocSelects() {
  const fsel = document.getElementById('alloc-flight-sel');
  if (!fsel) return;
  fsel.innerHTML = '<option value="">-- Select a pending flight --</option>';
  sorted(S.flights.filter(f => ['pending', 'delayed', 'queued'].includes(f.status))).forEach(f => {
    const o = document.createElement('option');
    o.value = f.id;
    o.textContent = `${f.type === 'emergency' ? '[EMG]' : '[FLT]'} ID:${f.id} | ${f.name} | ETA:${f.time}min | ${f.status.toUpperCase()}`;
    fsel.appendChild(o);
  });

  const rsel = document.getElementById('alloc-runway-sel');
  rsel.innerHTML = '<option value="-1">AUTO -- Engine picks best runway</option>';
  S.runways.filter(r => !r.maintenance).forEach(r => {
    const o = document.createElement('option');
    o.value = r.id;
    o.textContent = `${r.name} -- ${r.available ? 'Free' : 'Busy (queue: ' + r.queue.length + ')'}`;
    rsel.appendChild(o);
  });

  // flight select preview
  fsel.onchange = () => {
    const f  = S.flights.find(x => x.id === parseInt(fsel.value));
    const el = document.getElementById('alloc-flight-preview');
    if (!f) { el.style.display = 'none'; return; }
    el.style.display = '';
    el.innerHTML = `<strong>Flight:</strong> ${f.name} &nbsp;|&nbsp;
      <strong>Type:</strong> ${f.type === 'emergency' ? 'EMERGENCY' : 'Normal'} &nbsp;|&nbsp;
      <strong>ETA:</strong> ${f.time} min &nbsp;|&nbsp; <strong>Status:</strong> ${f.status.toUpperCase()}`;
  };

  // runway select preview
  rsel.onchange = () => {
    const rid = parseInt(rsel.value);
    const el  = document.getElementById('alloc-runway-preview');
    if (rid === -1) {
      el.style.display = '';
      const f = S.flights.find(x => x.id === parseInt(fsel.value));
      el.innerHTML = f?.type === 'emergency'
        ? `AUTO: Engine will find a free runway or preempt a normal flight (Algorithm 9 + 7)`
        : `AUTO: Engine will pick the shortest-queue runway (Algorithm 9)`;
      return;
    }
    const r = S.runways.find(x => x.id === rid);
    if (!r) { el.style.display = 'none'; return; }
    el.style.display = '';
    const cf = r.current_flight ? flightById(r.current_flight) : null;
    el.innerHTML = `<strong>Runway:</strong> ${r.name} &nbsp;|&nbsp;
      <strong>Status:</strong> ${r.available ? 'Free' : 'Busy'} &nbsp;|&nbsp;
      <strong>Current:</strong> ${cf ? cf.name : '--'} &nbsp;|&nbsp;
      <strong>Queued:</strong> ${r.queue.length} flights`;
  };
}

async function doAllocate() {
  const fidVal = document.getElementById('alloc-flight-sel').value;
  const ridVal = document.getElementById('alloc-runway-sel').value;
  const msg    = document.getElementById('alloc-msg');
  if (!fidVal) { msgEl(msg, 'Please select a flight.', 'error'); return; }

  const rid = (ridVal === '' || ridVal === '-1') ? -1 : parseInt(ridVal);
  const r = await api('allocate_runway', { flight_id: parseInt(fidVal), runway_id: rid });
  if (r.error) { msgEl(msg, r.error, 'error'); return; }
  msgEl(msg, r.message, 'success');
  await refresh();
  buildAllocSelects();
  buildQueueGrid();
  if (r.conflict) notifyConflict(r.conflict);
}

// Queue grid -- har runway ki current queue dikhao
function buildQueueGrid() {
  const el = document.getElementById('queue-grid');
  if (!el) return;
  if (!S.runways.length) { el.innerHTML = '<p style="color:var(--muted);">No runways added yet.</p>'; return; }
  el.innerHTML = S.runways.map(r => {
    const cf = r.current_flight ? flightById(r.current_flight) : null;
    const sb = r.maintenance ? '<span class="badge badge-maint">MAINT</span>'
             : r.available   ? '<span class="badge badge-free">FREE</span>'
             : '<span class="badge badge-busy">BUSY</span>';
    let items = '';

    if (cf) {
      const dep = cf.departure_time >= 0 ? ` | Dep:${cf.departure_time}min` : '';
      items += `<div class="queue-item">
        <div class="queue-pos current${cf.type === 'emergency' ? ' emerg' : ''}">*</div>
        <div><strong>${cf.name}</strong> -- ${cf.type === 'emergency' ? 'Emergency' : 'Normal'} -- ETA ${cf.time}min${dep}
          <span class="badge badge-allocated" style="margin-left:4px;font-size:.7rem;">LANDING</span></div>
      </div>`;
    }

    (r.queue || []).forEach((qf, i) => {
      const dep = qf.departure_time >= 0 ? ` | Dep:${qf.departure_time}min` : '';
      items += `<div class="queue-item">
        <div class="queue-pos${qf.type === 'emergency' ? ' emerg' : ''}">${i+1}</div>
        <div><strong>${qf.name}</strong> -- ${qf.type === 'emergency' ? 'Emergency' : 'Normal'} -- ETA ${qf.time}min${dep}</div>
      </div>`;
    });

    if (!cf && !r.queue?.length) items = `<p style="color:var(--green);font-size:.82rem;padding:6px 0;">Runway is free.</p>`;

    return `<div class="queue-card">
      <div class="queue-card-head"><span>${r.name}</span>
        <div style="display:flex;gap:6px;align-items:center;">${sb}
          ${!r.available && cf ? `<button class="btn btn-success btn-sm" onclick="clearRwy(${r.id})">Clear</button>` : ''}</div>
      </div>
      <div class="queue-card-body">${items}</div>
    </div>`;
  }).join('');
}

// History -- allocation history table banao
function buildHistory() {
  const tbody = document.getElementById('history-tbody');
  if (!tbody) return;
  if (!S.history.length) {
    tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;color:var(--muted);padding:18px;">No allocation history yet.</td></tr>';
    return;
  }
  tbody.innerHTML = [...S.history].reverse().map((h, i) => `
    <tr><td>${S.history.length - i}</td><td><strong>${h.flight}</strong></td><td>${typeBadge(h.type)}</td>
    <td>${h.runway || '--'}</td><td>${h.time} min</td>
    <td>${h.departure_time >= 0 ? h.departure_time + ' min' : '--'}</td>
    <td>${statusBadge(h.status || 'pending')}</td></tr>`).join('');
}

// Delay modal -- ETA update karne ka dialog
function openDelay(fid) {
  _delayId = parseInt(fid);
  const f = S.flights.find(x => x.id === parseInt(fid));
  if (!f) return;
  document.getElementById('delay-info').textContent =
    `Flight ${f.name} (ID:${f.id}) -- Current ETA: ${f.time} min. Set a new arrival time below.`;
  document.getElementById('delay-newtime').value = '';
  openModal('modal-delay');
}

async function confirmDelay() {
  const t = parseInt(document.getElementById('delay-newtime').value);
  if (isNaN(t) || t < 0) return;
  const r = await api('set_delay_time', { flight_id: _delayId, new_time: t });
  if (r.error) { alert(r.error); return; }
  await refresh();
  // agar auto allocation hua toh notification dikhao
  if (r.auto_alloc) {
    const label = r.alloc_type === 'MOVED_TO_FREE' ? 'Moved to Free Runway'
                : r.alloc_type === 'REQUEUED'      ? 'Re-queued'
                :                                    'Auto-Allocated';
    notifyAutoAlloc(label + ': ' + r.auto_alloc);
  }
  closeModal('modal-delay');
}

// Utilities -- chhote chhote helper functions

// type badge -- normal ya emergency ka colored label
function typeBadge(t) {
  return t === 'emergency'
    ? '<span class="badge badge-emergency">Emergency</span>'
    : '<span class="badge badge-normal">Normal</span>';
}

// status badge -- pending/allocated/delayed etc ka colored label
function statusBadge(s) {
  const map = {
    pending:   'badge-pending',
    allocated: 'badge-allocated',
    delayed:   'badge-delayed',
    queued:    'badge-delayed',
    departed:  'badge-departed'
  };
  return `<span class="badge ${map[s] || 'badge-normal'}">${s === 'queued' ? 'DELAYED' : s.toUpperCase()}</span>`;
}

// emergency pehle, phir ETA ke order mein sort karo
function sorted(list) {
  return [...list].sort((a, b) => a.type !== b.type ? (a.type === 'emergency' ? -1 : 1) : a.time - b.time);
}

function flightById(fid) { return S.flights.find(f => f.id === fid) || null; }
function set(id, val) { const el = document.getElementById(id); if (el) el.textContent = val; }
function clearFields(ids) { ids.forEach(id => { const el = document.getElementById(id); if (el) el.value = ''; }); }
function showMsg(id, msg, type) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = msg;
  el.className = 'msg' + (type ? ' ' + type : '');
}
function msgEl(el, msg, type) {
  el.textContent = msg;
  el.className = 'msg ' + type;
  el.style.display = '';
  if (type === 'success') setTimeout(() => { el.style.display = 'none'; }, 3000);
}
