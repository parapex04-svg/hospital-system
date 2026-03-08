/* ============================================================
 * script.js — MediCore Hospital Management System
 * Handles:
 *   • WebAssembly bridge (or JS-only demo mode)
 *   • Navigation / view switching
 *   • CRUD operations via C-exported functions
 *   • Real-time analytics rendering
 *   • Form validation (mirrors C validation in JS)
 * ============================================================ */

'use strict';

/* ─── WASM Bridge ─────────────────────────────────────────── */
let HS = null;   /* The loaded Emscripten module, or null in demo mode */

/* When Emscripten module loads it calls this */
window.Module = {
  onRuntimeInitialized: function () {
    HS = {
      init:                Module.cwrap('hs_init',                 null,   []),
      add_patient:         Module.cwrap('hs_add_patient',          'number',
                             ['string','number','number','string','string',
                              'string','number','string','string']),
      update_patient:      Module.cwrap('hs_update_patient',       'number',
                             ['number','string','number','number','string',
                              'string','string','number','string']),
      delete_patient:      Module.cwrap('hs_delete_patient',       'number', ['number']),
      search_patient:      Module.cwrap('hs_search_patient',       'string', ['string']),
      get_all_patients:    Module.cwrap('hs_get_all_patients_json','string', []),
      add_visit:           Module.cwrap('hs_add_visit',            'number',
                             ['number','string','string','string',
                              'string','string','number','number']),
      get_visits:          Module.cwrap('hs_get_visits_json',      'string', ['number']),
      add_appointment:     Module.cwrap('hs_add_appointment',      'number',
                             ['number','string','string','string','string','number']),
      get_appointments:    Module.cwrap('hs_get_appointments_json','string', []),
      cancel_appointment:  Module.cwrap('hs_cancel_appointment',   'number', ['number']),
      get_dashboard:       Module.cwrap('hs_get_dashboard_json',   'string', []),
      triage_queue:        Module.cwrap('hs_triage_queue_json',    'string', []),
      save_all:            Module.cwrap('hs_save_all',             'number', []),
      backup:              Module.cwrap('hs_backup',               'number', []),
    };
    HS.init();
    hideLoadingScreen();
    initApp();
  }
};

/* ─── Demo/JS-only store (fallback when WASM not available) ── */
const DemoStore = {
  patients:     [],
  visits:       [],
  appointments: [],
  nextPatientId: 1000,
  nextVisitId:   1,
  nextApptId:    1,

  addPatient(p) {
    p.id = this.nextPatientId++;
    p.is_active = 1;
    p.visit_count = 0;
    this.patients.push({...p});
    return p.id;
  },
  getActivePatients() {
    return this.patients.filter(p => p.is_active);
  },
  findPatientById(id) {
    return this.patients.find(p => p.id === id && p.is_active);
  },
  updatePatient(id, data) {
    const idx = this.patients.findIndex(p => p.id === id);
    if (idx < 0) return -1;
    Object.assign(this.patients[idx], data);
    return 0;
  },
  deletePatient(id) {
    const p = this.patients.find(p => p.id === id);
    if (!p) return -1;
    p.is_active = 0;
    return 0;
  },
  addVisit(v) {
    v.visit_id = this.nextVisitId++;
    this.visits.push({...v});
    const p = this.findPatientById(v.patient_id);
    if (p) { p.visit_count++; p.severity = v.severity_at_visit; }
    return v.visit_id;
  },
  getVisitsByPatient(pid) {
    return this.visits.filter(v => v.patient_id === pid);
  },
  addAppointment(a) {
    /* Basic conflict check */
    const conflict = this.appointments.some(x =>
      x.doctor === a.doctor && x.date === a.date &&
      x.time_slot === a.time_slot && x.status !== 'CANCELLED'
    );
    if (conflict) return -2;
    a.appt_id = this.nextApptId++;
    a.status = 'SCHEDULED';
    this.appointments.push({...a});
    return a.appt_id;
  },
  getDashboard() {
    const active = this.getActivePatients();
    const high   = active.filter(p => p.severity === 3 || p.severity === '3').length;
    const medium = active.filter(p => p.severity === 2 || p.severity === '2').length;
    const low    = active.filter(p => p.severity === 1 || p.severity === '1').length;
    const today  = new Date().toISOString().split('T')[0];
    const appts_today = this.appointments.filter(a =>
      a.date === today && a.status === 'SCHEDULED').length;

    /* Disease frequency */
    const disMap = {};
    active.forEach(p => {
      if (p.disease) disMap[p.disease] = (disMap[p.disease]||0)+1;
    });
    const top_diseases = Object.entries(disMap)
      .sort((a,b)=>b[1]-a[1])
      .slice(0,10)
      .map(([disease,count])=>({disease,count}));

    /* Doctor workload */
    const docMap = {};
    active.forEach(p => {
      if (!p.doctor) return;
      if (!docMap[p.doctor]) docMap[p.doctor] = {doctor:p.doctor, patients:0, appointments:0, visits:0};
      docMap[p.doctor].patients++;
    });
    this.appointments.forEach(a => {
      if (docMap[a.doctor]) docMap[a.doctor].appointments++;
    });
    this.visits.forEach(v => {
      if (docMap[v.doctor]) docMap[v.doctor].visits++;
    });
    const doctor_workload = Object.values(docMap);

    return {
      total_patients: this.patients.length,
      active_patients: active.length,
      total_visits: this.visits.length,
      total_appointments: this.appointments.length,
      appointments_today: appts_today,
      high_severity: high,
      medium_severity: medium,
      low_severity: low,
      top_diseases,
      doctor_workload
    };
  },
  getTriageQueue() {
    return this.getActivePatients()
      .sort((a,b) => (parseInt(b.severity)||1) - (parseInt(a.severity)||1));
  }
};

/* ─── Seed demo data ─────────────────────────────────────── */
function seedDemoData() {
  const today = todayStr();
  const patients = [
    { name:'Priya Sharma',    age:45, gender:'F', phone:'9876543210', address:'12 MG Road, Chennai',     disease:'Hypertension',      severity:2, doctor:'Dr. Arjun Mehta',    reg_date:'2025-01-15' },
    { name:'Rajan Krishnan',  age:67, gender:'M', phone:'8765432109', address:'34 Anna Nagar, Chennai',  disease:'Diabetes Type 2',   severity:3, doctor:'Dr. Ananya Iyer',    reg_date:'2025-02-20' },
    { name:'Sunita Devi',     age:32, gender:'F', phone:'7654321098', address:'56 T Nagar, Chennai',     disease:'Asthma',            severity:1, doctor:'Dr. Karthik Rao',    reg_date:'2025-03-01' },
    { name:'Mohammed Farouk', age:55, gender:'M', phone:'6543210987', address:'78 Adyar, Chennai',       disease:'Coronary Disease',  severity:3, doctor:'Dr. Arjun Mehta',    reg_date:'2025-03-10' },
    { name:'Lakshmi Menon',   age:28, gender:'F', phone:'9543216780', address:'90 Velachery, Chennai',   disease:'Migraine',          severity:1, doctor:'Dr. Ananya Iyer',    reg_date:'2025-04-05' },
    { name:'Venkat Subramanian',age:72,gender:'M',phone:'9432107658', address:'2 Mylapore, Chennai',     disease:'Arthritis',         severity:2, doctor:'Dr. Karthik Rao',    reg_date:'2025-04-18' },
    { name:'Aarti Singh',     age:38, gender:'F', phone:'8321076543', address:'44 Kilpauk, Chennai',     disease:'Thyroid Disorder',  severity:1, doctor:'Dr. Arjun Mehta',    reg_date:'2025-05-02' },
    { name:'Dinesh Kumar',    age:50, gender:'M', phone:'7210654329', address:'66 Nungambakkam',          disease:'Hypertension',      severity:2, doctor:'Dr. Ananya Iyer',    reg_date:'2025-05-20' },
  ];

  patients.forEach(p => DemoStore.addPatient(p));

  DemoStore.addVisit({ patient_id:1000, patient_name:'Priya Sharma', doctor:'Dr. Arjun Mehta',
    visit_date:'2025-06-01', diagnosis:'Hypertension', prescription:'Amlodipine 5mg daily',
    notes:'BP: 150/95. Monitor weekly.', severity_at_visit:2, follow_up_days:7 });
  DemoStore.addVisit({ patient_id:1001, patient_name:'Rajan Krishnan', doctor:'Dr. Ananya Iyer',
    visit_date:'2025-06-03', diagnosis:'Diabetes Type 2', prescription:'Metformin 500mg BD',
    notes:'HbA1c: 9.2%. Dietary counseling given.', severity_at_visit:3, follow_up_days:14 });
  DemoStore.addVisit({ patient_id:1003, patient_name:'Mohammed Farouk', doctor:'Dr. Arjun Mehta',
    visit_date:'2025-06-05', diagnosis:'Coronary Disease', prescription:'Aspirin 75mg + Atorvastatin',
    notes:'ECG normal. Stress test ordered.', severity_at_visit:3, follow_up_days:3 });

  DemoStore.addAppointment({ patient_id:1000, doctor:'Dr. Arjun Mehta',
    date:today, time_slot:'09:00', reason:'Follow-up BP check', duration_mins:30, patient_name:'Priya Sharma' });
  DemoStore.addAppointment({ patient_id:1001, doctor:'Dr. Ananya Iyer',
    date:today, time_slot:'10:30', reason:'Diabetes management', duration_mins:45, patient_name:'Rajan Krishnan' });
  DemoStore.addAppointment({ patient_id:1002, doctor:'Dr. Karthik Rao',
    date:today, time_slot:'11:00', reason:'Asthma review', duration_mins:30, patient_name:'Sunita Devi' });
}

/* ─── App State ───────────────────────────────────────────── */
let editingPatientId = null;
let allPatients      = [];
let allVisits        = [];
let allAppts         = [];
let dashboardData    = null;

/* ─── Helpers ─────────────────────────────────────────────── */
function todayStr() {
  return new Date().toISOString().split('T')[0];
}

function severityBadge(s) {
  const n = parseInt(s);
  if (n === 3) return '<span class="badge badge-high">🔴 High</span>';
  if (n === 2) return '<span class="badge badge-medium">🟡 Medium</span>';
  return '<span class="badge badge-low">🟢 Low</span>';
}

function statusBadge(s) {
  const map = {
    SCHEDULED:'badge-scheduled', COMPLETED:'badge-completed',
    CANCELLED:'badge-cancelled', 'NO-SHOW':'badge-no-show'
  };
  return `<span class="badge ${map[s]||'badge-blue'}">${s}</span>`;
}

function toast(msg, type = 'success') {
  const el = document.createElement('div');
  el.className = `toast ${type}`;
  el.innerHTML = (type==='success'?'✓ ':type==='error'?'✕ ':'⚠ ') + msg;
  document.getElementById('toast-container').appendChild(el);
  setTimeout(() => el.remove(), 3500);
}

function closeModal(id) {
  document.getElementById(id).classList.remove('open');
}

function openModal(id) {
  document.getElementById(id).classList.add('open');
}

/* Close modal on overlay click */
document.querySelectorAll('.modal-overlay').forEach(ov => {
  ov.addEventListener('click', e => {
    if (e.target === ov) ov.classList.remove('open');
  });
});

/* ─── JS Validation (mirrors C validation) ────────────────── */
function validatePhone(p)    { return /^\d{10}$/.test(p); }
function validateAge(a)      { const n=parseInt(a); return n>=1 && n<=120; }
function validateNonEmpty(s) { return s && s.trim().length > 0; }
function validateDate(d)     { return /^\d{4}-\d{2}-\d{2}$/.test(d); }

function clearFormErrors() {
  document.querySelectorAll('.form-group.has-error').forEach(el => {
    el.classList.remove('has-error');
  });
  document.querySelectorAll('.field-error').forEach(el => { el.textContent = ''; });
}

function showError(fieldId, errId, msg) {
  const fg = document.getElementById(fieldId)?.closest('.form-group');
  if (fg) fg.classList.add('has-error');
  const er = document.getElementById(errId);
  if (er) er.textContent = msg;
  return false;
}

/* ─── Navigation ──────────────────────────────────────────── */
function switchView(viewId) {
  document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
  document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));

  const view = document.getElementById('view-' + viewId);
  if (view) view.classList.add('active');

  const btn = document.querySelector(`[data-view="${viewId}"]`);
  if (btn) btn.classList.add('active');

  const titles = {
    dashboard:    ['Dashboard', 'Hospital overview'],
    patients:     ['Patient Registry', 'All registered patients'],
    triage:       ['Triage Queue', 'Patients sorted by severity'],
    visits:       ['Visit Records', 'Patient encounter history'],
    appointments: ['Appointments', 'Scheduled appointments'],
    analytics:    ['Analytics', 'Disease trends & metrics'],
  };
  const [title, sub] = titles[viewId] || ['', ''];
  document.getElementById('topbar-title').textContent    = title;
  document.getElementById('topbar-subtitle').textContent = sub;

  /* Load data for view */
  if (viewId === 'dashboard')    loadDashboard();
  if (viewId === 'patients')     loadPatients();
  if (viewId === 'triage')       loadTriage();
  if (viewId === 'visits')       loadVisits();
  if (viewId === 'appointments') loadAppointments();
  if (viewId === 'analytics')    loadAnalytics();
}

document.querySelectorAll('.nav-item[data-view]').forEach(btn => {
  btn.addEventListener('click', () => switchView(btn.dataset.view));
});

/* ─── Data loading ────────────────────────────────────────── */
function getPatients() {
  if (HS) return JSON.parse(HS.get_all_patients());
  return DemoStore.getActivePatients();
}

function getVisits(patientId = null) {
  if (HS) {
    if (patientId) return JSON.parse(HS.get_visits(patientId));
    /* collect all visits from all patients — JS fallback merges */
    return allVisits;
  }
  return patientId
    ? DemoStore.getVisitsByPatient(patientId)
    : DemoStore.visits;
}

function getAppointments() {
  if (HS) return JSON.parse(HS.get_appointments());
  return DemoStore.appointments;
}

function getDashboard() {
  if (HS) return JSON.parse(HS.get_dashboard());
  return DemoStore.getDashboard();
}

function getTriageQueue() {
  if (HS) return JSON.parse(HS.triage_queue());
  return DemoStore.getTriageQueue();
}

/* ─── Dashboard ───────────────────────────────────────────── */
function loadDashboard() {
  dashboardData = getDashboard();
  const d = dashboardData;

  document.getElementById('stat-active-patients').textContent = d.active_patients;
  document.getElementById('stat-total-visits').textContent    = d.total_visits;
  document.getElementById('stat-appts-today').textContent     = d.appointments_today;
  document.getElementById('stat-high-severity').textContent   = d.high_severity ?? d.high_severity_count ?? 0;
  document.getElementById('stat-total-appts').textContent     = d.total_appointments;

  renderSeverityChart(d);
  renderDoctorChart('doctor-chart', d.doctor_workload);
  renderDiseaseChart('disease-chart', d.top_diseases);
  updateTrigeBadge();
}

document.getElementById('btn-refresh-dashboard').addEventListener('click', loadDashboard);

function updateTrigeBadge() {
  const high = dashboardData?.high_severity ?? dashboardData?.high_severity_count ?? 0;
  const el   = document.getElementById('triage-badge');
  el.textContent = high;
  el.style.display = high > 0 ? '' : 'none';
}

/* ─── Severity donut (canvas) ─────────────────────────────── */
function renderSeverityChart(d) {
  const high   = d.high_severity   ?? d.high_severity_count   ?? 0;
  const medium = d.medium_severity ?? d.medium_severity_count ?? 0;
  const low    = d.low_severity    ?? d.low_severity_count    ?? 0;
  const total  = high + medium + low || 1;

  const canvas = document.getElementById('severity-chart');
  const ctx    = canvas.getContext('2d');
  const cx = 55, cy = 55, r = 46, inner = 26;

  ctx.clearRect(0, 0, 110, 110);

  const slices = [
    { val: high,   color: '#e5424c' },
    { val: medium, color: '#f59f00' },
    { val: low,    color: '#2dbe6c' },
  ];

  let startAngle = -Math.PI / 2;
  slices.forEach(s => {
    const angle = (s.val / total) * 2 * Math.PI;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, startAngle, startAngle + angle);
    ctx.closePath();
    ctx.fillStyle = s.color;
    ctx.fill();
    startAngle += angle;
  });

  /* Inner circle (donut hole) */
  ctx.beginPath();
  ctx.arc(cx, cy, inner, 0, 2 * Math.PI);
  ctx.fillStyle = '#ffffff';
  ctx.fill();

  /* Centre text */
  ctx.fillStyle = '#0f172a';
  ctx.font = 'bold 14px DM Sans, sans-serif';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(total, cx, cy);

  /* Legend */
  const legend = document.getElementById('severity-legend');
  legend.innerHTML = `
    <div class="legend-item"><div class="legend-dot" style="background:#e5424c"></div>
      <span>Critical (High)</span><strong>${high}</strong></div>
    <div class="legend-item"><div class="legend-dot" style="background:#f59f00"></div>
      <span>Urgent (Medium)</span><strong>${medium}</strong></div>
    <div class="legend-item"><div class="legend-dot" style="background:#2dbe6c"></div>
      <span>Routine (Low)</span><strong>${low}</strong></div>
  `;
}

/* ─── Bar chart render ────────────────────────────────────── */
function renderDoctorChart(containerId, workloads = []) {
  const el = document.getElementById(containerId);
  if (!workloads?.length) {
    el.innerHTML = '<div style="color:var(--text-muted);font-size:13px;">No data yet</div>';
    return;
  }
  const max = Math.max(...workloads.map(w => w.patients || w.patient_count || 0)) || 1;
  el.innerHTML = workloads.map(w => {
    const cnt = w.patients ?? w.patient_count ?? 0;
    const pct = Math.round((cnt / max) * 100);
    return `<div class="bar-row">
      <div class="bar-label" title="${w.doctor}">${w.doctor}</div>
      <div class="bar-track"><div class="bar-fill" style="width:${pct}%;background:#0d6efd"></div></div>
      <div class="bar-count">${cnt}</div>
    </div>`;
  }).join('');
}

function renderDiseaseChart(containerId, diseases = []) {
  const el = document.getElementById(containerId);
  if (!diseases?.length) {
    el.innerHTML = '<div style="color:var(--text-muted);font-size:13px;">No data yet</div>';
    return;
  }
  const max = Math.max(...diseases.map(d => d.count)) || 1;
  el.innerHTML = diseases.slice(0,8).map(d => {
    const pct = Math.round((d.count / max) * 100);
    return `<div class="bar-row">
      <div class="bar-label" title="${d.disease}">${d.disease}</div>
      <div class="bar-track"><div class="bar-fill" style="width:${pct}%;background:#00b4a0"></div></div>
      <div class="bar-count">${d.count}</div>
    </div>`;
  }).join('');
}

/* ─── Patient table ───────────────────────────────────────── */
function loadPatients(filter = '') {
  allPatients = getPatients();
  const rows  = filter
    ? allPatients.filter(p => p.name.toLowerCase().includes(filter.toLowerCase()))
    : allPatients;

  document.getElementById('patient-table-count').textContent =
    `${rows.length} patient${rows.length !== 1 ? 's' : ''}`;

  const tbody = document.getElementById('patient-tbody');
  const empty = document.getElementById('patient-empty');

  if (!rows.length) {
    tbody.innerHTML = '';
    empty.style.display = '';
    return;
  }
  empty.style.display = 'none';

  tbody.innerHTML = rows.map(p => `
    <tr>
      <td class="td-id">#${p.id}</td>
      <td class="td-name">${escHtml(p.name)}</td>
      <td>${p.age}</td>
      <td>${p.gender === 'M' ? '♂ Male' : p.gender === 'F' ? '♀ Female' : '⚧ Other'}</td>
      <td><span style="font-family:var(--font-mono);font-size:12px;">${p.phone}</span></td>
      <td>${escHtml(p.disease||'—')}</td>
      <td>${severityBadge(p.severity)}</td>
      <td><span style="font-size:12.5px;">${escHtml(p.doctor||'—')}</span></td>
      <td><span class="badge badge-blue">${p.visit_count}</span></td>
      <td style="font-size:12px;color:var(--text-muted)">${p.reg_date}</td>
      <td>
        <div style="display:flex;gap:4px;">
          <button class="btn btn-ghost btn-sm btn-icon" title="View" onclick="viewPatient(${p.id})">👁</button>
          <button class="btn btn-ghost btn-sm btn-icon" title="Edit" onclick="editPatient(${p.id})">✏️</button>
          <button class="btn btn-ghost btn-sm btn-icon" title="New Visit" onclick="openNewVisitModal(${p.id})">📋</button>
          <button class="btn btn-ghost btn-sm btn-icon" title="Delete" onclick="deletePatient(${p.id},'${escHtml(p.name)}')">🗑</button>
        </div>
      </td>
    </tr>`).join('');
}

document.getElementById('patient-filter').addEventListener('input', function() {
  loadPatients(this.value);
});

/* ─── Triage queue ────────────────────────────────────────── */
function loadTriage() {
  const queue = getTriageQueue();
  const el    = document.getElementById('triage-list');

  if (!queue.length) {
    el.innerHTML = '<div class="empty-state"><div class="emoji">🟢</div><h3>No active patients</h3></div>';
    return;
  }

  el.innerHTML = queue.map((p, i) => {
    const sev = parseInt(p.severity);
    const cls = sev === 3 ? 'critical' : sev === 2 ? 'urgent' : 'routine';
    const lbl = sev === 3 ? '🔴 CRITICAL' : sev === 2 ? '🟡 URGENT' : '🟢 ROUTINE';
    return `<div class="triage-item ${cls}">
      <div class="triage-rank">${i+1}</div>
      <div class="triage-left">
        <div class="triage-name">${escHtml(p.name)} <small style="color:var(--text-muted);font-weight:400;">#${p.id}</small></div>
        <div class="triage-meta">Age ${p.age} · ${escHtml(p.disease||'Unspecified')} · Dr. ${escHtml(p.doctor||'—')}</div>
      </div>
      <div class="triage-priority-label">${lbl}</div>
      <button class="btn btn-outline btn-sm" onclick="openNewVisitModal(${p.id})">Record Visit</button>
    </div>`;
  }).join('');
}

/* ─── Visit records ───────────────────────────────────────── */
function loadVisits(patientIdFilter = null) {
  /* Collect all visits from all patients if WASM */
  if (HS) {
    const pts = getPatients();
    allVisits = [];
    pts.forEach(p => {
      const pv = JSON.parse(HS.get_visits(p.id));
      allVisits.push(...pv);
    });
  }

  const visits = patientIdFilter
    ? getVisits(patientIdFilter)
    : (HS ? allVisits : DemoStore.visits);

  const tbody = document.getElementById('visits-tbody');
  const empty = document.getElementById('visits-empty');

  if (!visits.length) {
    tbody.innerHTML = '';
    empty.style.display = '';
    return;
  }
  empty.style.display = 'none';

  tbody.innerHTML = visits.map(v => `
    <tr>
      <td class="td-id">#${v.visit_id}</td>
      <td class="td-name">${escHtml(v.patient_name||v.patient_id)}</td>
      <td>${escHtml(v.doctor||'—')}</td>
      <td style="font-size:12px;">${v.visit_date}</td>
      <td>${escHtml(v.diagnosis||'—')}</td>
      <td>${severityBadge(v.severity_at_visit)}</td>
      <td>${v.follow_up_days > 0 ? `${v.follow_up_days}d` : '—'}</td>
      <td style="max-width:180px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:12px;">${escHtml(v.prescription||'—')}</td>
    </tr>`).join('');
}

document.getElementById('visit-patient-filter').addEventListener('change', function() {
  const pid = parseInt(this.value);
  loadVisits(pid || null);
});

/* ─── Appointments ────────────────────────────────────────── */
function loadAppointments() {
  allAppts = getAppointments();
  const tbody = document.getElementById('appts-tbody');
  const empty = document.getElementById('appts-empty');

  if (!allAppts.length) {
    tbody.innerHTML = '';
    empty.style.display = '';
    return;
  }
  empty.style.display = 'none';

  tbody.innerHTML = allAppts.map(a => `
    <tr>
      <td class="td-id">#${a.appt_id}</td>
      <td class="td-name">${escHtml(a.patient_name||a.patient_id)}</td>
      <td>${escHtml(a.doctor||'—')}</td>
      <td style="font-size:12px;">${a.date}</td>
      <td><strong>${a.time_slot}</strong></td>
      <td>${a.duration_mins}min</td>
      <td style="max-width:140px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:12px;">${escHtml(a.reason||'—')}</td>
      <td>${statusBadge(a.status||'SCHEDULED')}</td>
      <td>
        ${(a.status==='SCHEDULED') ? `<button class="btn btn-danger btn-sm" onclick="cancelAppt(${a.appt_id})">Cancel</button>` : ''}
      </td>
    </tr>`).join('');
}

/* ─── Analytics ───────────────────────────────────────────── */
function loadAnalytics() {
  dashboardData = getDashboard();
  const d = dashboardData;

  renderDiseaseChart('analytics-disease-chart', d.top_diseases);
  renderDoctorChart('analytics-doctor-chart', d.doctor_workload);

  const tbody = document.getElementById('doctor-matrix-tbody');
  const wl    = d.doctor_workload || [];
  const maxPts = Math.max(...wl.map(w => w.patients ?? w.patient_count ?? 0), 1);

  tbody.innerHTML = wl.map(w => {
    const pts = w.patients ?? w.patient_count ?? 0;
    const pct = Math.round((pts / maxPts) * 100);
    return `<tr>
      <td class="td-name">${escHtml(w.doctor)}</td>
      <td>${pts}</td>
      <td>${w.appointments ?? w.appointment_count ?? 0}</td>
      <td>${w.visits ?? w.visit_count ?? 0}</td>
      <td>
        <div style="display:flex;align-items:center;gap:8px;">
          <div class="bar-track" style="flex:1;height:8px;">
            <div class="bar-fill" style="width:${pct}%"></div>
          </div>
          <span style="font-size:12px;font-weight:600;">${pct}%</span>
        </div>
      </td>
    </tr>`;
  }).join('');
}

/* ─── Patient CRUD ────────────────────────────────────────── */
function openNewPatientModal() {
  editingPatientId = null;
  clearFormErrors();
  document.getElementById('patient-modal-title').textContent = 'Register New Patient';
  document.getElementById('patient-modal-save').textContent  = 'Register Patient';
  ['f-name','f-age','f-phone','f-disease','f-doctor','f-address'].forEach(id => {
    document.getElementById(id).value = '';
  });
  document.getElementById('f-gender').value   = '';
  document.getElementById('f-severity').value = '';
  document.getElementById('f-regdate').value  = todayStr();
  openModal('patient-modal');
}

function editPatient(id) {
  const p = (HS ? getPatients() : DemoStore.getActivePatients()).find(p => p.id === id);
  if (!p) return;
  editingPatientId = id;
  clearFormErrors();
  document.getElementById('patient-modal-title').textContent = `Edit Patient #${id}`;
  document.getElementById('patient-modal-save').textContent  = 'Save Changes';
  document.getElementById('f-name').value     = p.name;
  document.getElementById('f-age').value      = p.age;
  document.getElementById('f-gender').value   = p.gender;
  document.getElementById('f-phone').value    = p.phone;
  document.getElementById('f-address').value  = p.address || '';
  document.getElementById('f-disease').value  = p.disease || '';
  document.getElementById('f-severity').value = p.severity;
  document.getElementById('f-doctor').value   = p.doctor;
  document.getElementById('f-regdate').value  = p.reg_date;
  openModal('patient-modal');
}

function savePatient() {
  clearFormErrors();
  const name     = document.getElementById('f-name').value.trim();
  const age      = parseInt(document.getElementById('f-age').value);
  const gender   = document.getElementById('f-gender').value;
  const phone    = document.getElementById('f-phone').value.trim();
  const address  = document.getElementById('f-address').value.trim();
  const disease  = document.getElementById('f-disease').value.trim();
  const severity = parseInt(document.getElementById('f-severity').value);
  const doctor   = document.getElementById('f-doctor').value.trim();
  const regdate  = document.getElementById('f-regdate').value;

  let valid = true;
  if (!validateNonEmpty(name))   { showError('f-name', 'err-name', 'Name is required'); valid=false; }
  if (!validateAge(age))         { showError('f-age',  'err-age',  'Age must be 1–120'); valid=false; }
  if (!validatePhone(phone))     { showError('f-phone','err-phone','Phone must be 10 digits'); valid=false; }
  if (!gender)                   { toast('Please select a gender', 'error'); valid=false; }
  if (!severity || severity<1||severity>3) { toast('Please select a severity level', 'error'); valid=false; }
  if (!validateNonEmpty(doctor)) { toast('Doctor name is required', 'error'); valid=false; }
  if (!valid) return;

  let result;
  if (editingPatientId) {
    if (HS) {
      result = HS.update_patient(editingPatientId, name, age, gender.charCodeAt(0),
                                  phone, address, disease, severity, doctor);
    } else {
      DemoStore.updatePatient(editingPatientId, {name,age,gender,phone,address,disease,severity,doctor});
      result = 0;
    }
    if (result >= 0) {
      toast(`Patient #${editingPatientId} updated`, 'success');
      closeModal('patient-modal');
      loadPatients();
    } else { toast('Update failed', 'error'); }
  } else {
    if (HS) {
      result = HS.add_patient(name, age, gender.charCodeAt(0),
                               phone, address, disease, severity, doctor, regdate);
    } else {
      result = DemoStore.addPatient({name,age,gender,phone,address,disease,severity,doctor,reg_date:regdate});
    }
    if (result > 0) {
      toast(`Patient registered! ID: ${result}`, 'success');
      closeModal('patient-modal');
      loadPatients();
      loadDashboard();
    } else { toast(result===-4?'Duplicate patient ID':'Registration failed', 'error'); }
  }
}

function deletePatient(id, name) {
  if (!confirm(`Delete patient "${name}" (#${id})?\n\nThis action cannot be undone.`)) return;
  const result = HS ? HS.delete_patient(id) : DemoStore.deletePatient(id);
  if (result >= 0) {
    toast(`Patient #${id} removed`, 'success');
    loadPatients();
    loadDashboard();
  } else { toast('Delete failed', 'error'); }
}

function viewPatient(id) {
  const p = (HS ? getPatients() : DemoStore.getActivePatients()).find(p => p.id === id);
  if (!p) return;
  const sevLabel = p.severity==3?'🔴 High':p.severity==2?'🟡 Medium':'🟢 Low';
  const body = document.getElementById('detail-modal-body');
  body.innerHTML = `
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-bottom:20px;">
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Patient ID</div>
           <div style="font-family:var(--font-mono);font-size:20px;font-weight:700;">#${p.id}</div></div>
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Full Name</div>
           <div style="font-size:18px;font-weight:700;">${escHtml(p.name)}</div></div>
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Age / Gender</div>
           <div>${p.age} years · ${p.gender==='M'?'Male':p.gender==='F'?'Female':'Other'}</div></div>
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Phone</div>
           <div style="font-family:var(--font-mono);">${p.phone}</div></div>
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Primary Diagnosis</div>
           <div>${escHtml(p.disease||'—')}</div></div>
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Severity</div>
           <div>${sevLabel}</div></div>
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Assigned Doctor</div>
           <div>${escHtml(p.doctor||'—')}</div></div>
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Visit Count</div>
           <div><span class="badge badge-blue">${p.visit_count} visits</span></div></div>
      <div style="grid-column:1/-1;"><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Address</div>
           <div>${escHtml(p.address||'—')}</div></div>
      <div><div style="font-size:11px;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:3px;">Registered</div>
           <div style="font-size:13px;color:var(--text-secondary);">${p.reg_date}</div></div>
    </div>
    <div style="display:flex;gap:8px;">
      <button class="btn btn-primary btn-sm" onclick="closeModal('detail-modal');editPatient(${id})">✏ Edit</button>
      <button class="btn btn-outline btn-sm" onclick="closeModal('detail-modal');openNewVisitModal(${id})">📋 Record Visit</button>
      <button class="btn btn-outline btn-sm" onclick="closeModal('detail-modal');openNewApptModal(${id})">📅 Schedule</button>
    </div>`;
  openModal('detail-modal');
}

/* ─── Visit CRUD ──────────────────────────────────────────── */
function openNewVisitModal(patientId = null) {
  document.getElementById('v-date').value     = todayStr();
  document.getElementById('v-severity').value = '1';
  document.getElementById('v-followup').value = '0';
  ['v-patient-id','v-doctor','v-diagnosis','v-prescription','v-notes'].forEach(id => {
    const el = document.getElementById(id);
    if (el.tagName === 'INPUT') el.value = '';
    else el.value = '';
  });
  if (patientId) document.getElementById('v-patient-id').value = patientId;
  openModal('visit-modal');
}

function saveVisit() {
  const pid      = parseInt(document.getElementById('v-patient-id').value);
  const doctor   = document.getElementById('v-doctor').value.trim();
  const date     = document.getElementById('v-date').value;
  const diag     = document.getElementById('v-diagnosis').value.trim();
  const rx       = document.getElementById('v-prescription').value.trim();
  const notes    = document.getElementById('v-notes').value.trim();
  const severity = parseInt(document.getElementById('v-severity').value);
  const followup = parseInt(document.getElementById('v-followup').value) || 0;

  if (!pid)              return toast('Patient ID is required', 'error');
  if (!validateNonEmpty(doctor)) return toast('Doctor name is required', 'error');
  if (!date)             return toast('Visit date is required', 'error');

  let result;
  if (HS) {
    result = HS.add_visit(pid, doctor, date, diag, rx, notes, severity, followup);
  } else {
    const p = DemoStore.findPatientById(pid);
    if (!p) return toast('Patient not found', 'error');
    result = DemoStore.addVisit({
      patient_id: pid, patient_name: p.name, doctor, visit_date: date,
      diagnosis: diag, prescription: rx, notes, severity_at_visit: severity, follow_up_days: followup
    });
  }

  if (result > 0) {
    toast(`Visit #${result} recorded`, 'success');
    closeModal('visit-modal');
    loadVisits();
    loadDashboard();
  } else if (result === -1) {
    toast('Patient ID not found', 'error');
  } else {
    toast('Failed to record visit', 'error');
  }
}

/* ─── Appointment CRUD ────────────────────────────────────── */
function openNewApptModal(patientId = null) {
  ['a-patient-id','a-doctor','a-reason'].forEach(id => {
    document.getElementById(id).value = '';
  });
  document.getElementById('a-date').value     = todayStr();
  document.getElementById('a-time').value     = '09:00';
  document.getElementById('a-duration').value = '30';
  if (patientId) document.getElementById('a-patient-id').value = patientId;
  openModal('appt-modal');
}

function saveAppointment() {
  const pid      = parseInt(document.getElementById('a-patient-id').value);
  const doctor   = document.getElementById('a-doctor').value.trim();
  const date     = document.getElementById('a-date').value;
  const time     = document.getElementById('a-time').value;
  const reason   = document.getElementById('a-reason').value.trim();
  const duration = parseInt(document.getElementById('a-duration').value) || 30;

  if (!pid)              return toast('Patient ID is required', 'error');
  if (!validateNonEmpty(doctor)) return toast('Doctor name is required', 'error');
  if (!date)             return toast('Date is required', 'error');
  if (!time)             return toast('Time is required', 'error');

  let result;
  if (HS) {
    result = HS.add_appointment(pid, doctor, date, time, reason, duration);
  } else {
    const p = DemoStore.findPatientById(pid);
    if (!p) return toast('Patient not found', 'error');
    result = DemoStore.addAppointment({
      patient_id: pid, patient_name: p.name, doctor, date,
      time_slot: time, reason, duration_mins: duration
    });
  }

  if (result > 0) {
    toast(`Appointment #${result} scheduled`, 'success');
    closeModal('appt-modal');
    loadAppointments();
    loadDashboard();
  } else if (result === -2 || result === -3) {
    toast('⚠ Appointment conflict! Doctor already booked at that time.', 'warning');
  } else if (result === -1) {
    toast('Patient not found', 'error');
  } else {
    toast('Scheduling failed — check date/time format', 'error');
  }
}

function cancelAppt(id) {
  if (!confirm('Cancel this appointment?')) return;
  if (HS) {
    HS.cancel_appointment(id);
  } else {
    const a = DemoStore.appointments.find(a => a.appt_id === id);
    if (a) a.status = 'CANCELLED';
  }
  toast('Appointment cancelled', 'success');
  loadAppointments();
}

/* ─── Global search ───────────────────────────────────────── */
document.getElementById('global-search-input').addEventListener('input', function() {
  const q = this.value.trim();
  if (q.length < 2) return;
  switchView('patients');
  loadPatients(q);
});

/* ─── New Patient shortcut ────────────────────────────────── */
document.getElementById('btn-new-patient').addEventListener('click', openNewPatientModal);

/* ─── Save / Backup ───────────────────────────────────────── */
document.getElementById('btn-save-all').addEventListener('click', () => {
  if (HS) HS.save_all();
  toast('All data saved', 'success');
});

document.getElementById('btn-backup').addEventListener('click', () => {
  if (HS) HS.backup();
  toast('Backup created', 'success');
});

/* ─── XSS escape helper ───────────────────────────────────── */
function escHtml(s) {
  if (!s) return '';
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;')
                   .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

/* ─── Date display ────────────────────────────────────────── */
function updateDateDisplay() {
  const now = new Date();
  const opts = { weekday:'short', year:'numeric', month:'short', day:'numeric' };
  document.getElementById('sidebar-date').textContent =
    now.toLocaleDateString('en-IN', opts);
}
updateDateDisplay();

/* ─── Init / loading screen ───────────────────────────────── */
function hideLoadingScreen() {
  const el = document.getElementById('wasm-loading');
  el.style.opacity = '0';
  el.style.transition = 'opacity .4s';
  setTimeout(() => el.style.display = 'none', 400);
}

function initApp() {
  loadDashboard();
}

/* ── If WASM module doesn't load in 2s, fall back to demo mode */
setTimeout(() => {
  if (!HS) {
    console.log('[MediCore] WASM not detected — running in JS demo mode');
    seedDemoData();
    hideLoadingScreen();
    initApp();
  }
}, 2000);
