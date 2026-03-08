# 🏥 MediCore — Hospital Patient Registration & Follow-up Tracker

> A complete hospital management system written in **C** with a modern web dashboard powered by **WebAssembly (Emscripten)**.

---

## 📋 Project Overview

MediCore is a full-featured hospital information system designed for patient registration, clinical record management, appointment scheduling, and operational analytics. It demonstrates:

- **Systems programming in C** with modular architecture
- **File persistence** using `fopen`/`fprintf`/`fscanf` with a human-readable pipe-delimited format
- **WebAssembly** deployment via Emscripten — C functions called directly from JavaScript
- **Real-time dashboard** with disease analytics, doctor workload balancing, and triage queues
- **Progressive enhancement** — the web UI runs in full demo mode without WASM if needed

---

## ✨ Features

### Core Patient Management
- ✅ Register new patients with auto-generated IDs (starting at 1000)
- ✅ Search patients by name (case-insensitive substring search)
- ✅ Update patient records
- ✅ Soft-delete patients (preserves data integrity)
- ✅ Display all active patients in a sortable table

### Clinical Records
- ✅ Record patient visits with diagnosis, prescription, and clinical notes
- ✅ View full visit history per patient
- ✅ Automatic patient visit counter update
- ✅ Follow-up scheduling within visit records

### Appointment Scheduling
- ✅ Schedule appointments with date/time selection
- ✅ **Conflict detection** — prevents double-booking a doctor at the same time slot
- ✅ Cancel and complete appointments
- ✅ Duration-aware overlap checking (e.g., 30-min vs 60-min slots)

### Analytics & Operations
- ✅ **Priority Triage Queue** — patients sorted by clinical severity (High → Low)
- ✅ **Disease Frequency Analyser** — most common diagnoses across all patients
- ✅ **Doctor Workload Balancer** — shows patient/appointment/visit load per physician
- ✅ **Dashboard Statistics** — total patients, today's appointments, severity breakdown
- ✅ Interactive donut chart (severity distribution) and bar charts

### Data & Persistence
- ✅ File persistence — all data stored in `data/*.txt` (pipe-delimited format)
- ✅ Automatic data backup to timestamped files
- ✅ Data loads on startup, saves on exit or manual trigger

### Validation
- ✅ Phone: exactly 10 numeric digits
- ✅ Age: 1–120 range
- ✅ Severity: 1/2/3 only
- ✅ Gender: M/F/O only
- ✅ Date: YYYY-MM-DD format
- ✅ Time slot: HH:MM format
- ✅ Empty field prevention
- ✅ Duplicate ID prevention

---

## 🏗️ Architecture

```
hospital-system/
├── src/
│   ├── main.c          ← Entry point + exported WASM functions + CLI loop
│   ├── patient.c       ← Patient CRUD + file I/O
│   ├── appointment.c   ← Scheduling + conflict detection
│   ├── visit.c         ← Visit records + file I/O
│   ├── analytics.c     ← Dashboard stats, triage queue, workload
│   └── validation.c    ← Reusable input validation functions
│
├── include/
│   ├── patient.h       ← Patient struct + PatientStore + declarations
│   ├── appointment.h   ← Appointment struct + declarations
│   ├── visit.h         ← Visit struct + declarations
│   ├── analytics.h     ← DashboardStats + analytics declarations
│   └── validation.h    ← Validation return codes + declarations
│
├── data/
│   ├── patients.txt    ← Persisted patient records (pipe-delimited)
│   ├── visits.txt      ← Persisted visit records
│   ├── appointments.txt← Persisted appointments
│   └── doctors.txt     ← Doctor registry
│
├── web/
│   ├── index.html      ← Single-page hospital dashboard
│   ├── style.css       ← Healthcare-themed CSS (DM Sans typography)
│   └── script.js       ← WASM bridge + JS demo mode + UI logic
│
├── Makefile            ← Native + WASM build targets
├── .gitignore
└── README.md
```

### Module Responsibilities

| Module | Responsibility |
|--------|---------------|
| `patient.c` | CRUD, file persistence, ID generation, name search |
| `appointment.c` | Scheduling, time-overlap conflict detection, status management |
| `visit.c` | Encounter recording, history lookup, file persistence |
| `analytics.c` | Dashboard aggregation, triage sorting, disease frequency, workload |
| `validation.c` | All input validation; returns typed error codes |
| `main.c` | Global state, WASM-exported functions, CLI loop |

### Data Flow (WASM mode)

```
Browser UI (script.js)
      │  calls via cwrap()
      ▼
WASM-exported functions (main.c)
      │  operates on
      ▼
Global PatientStore / AppointmentStore / VisitStore
      │  reads/writes
      ▼
Emscripten Virtual FS  →  data/*.txt files
```

### File Format Example (`patients.txt`)

```
# NEXT_ID: 1008
1000|Priya Sharma|45|F|9876543210|12 MG Road Chennai|Hypertension|2|Dr. Arjun Mehta|2025-01-15|1|2
id  |name        |age|gender|phone     |address        |disease     |sev|doctor        |reg_date  |active|visits
```

---

## 🔧 How to Compile

### Prerequisites
- GCC 9+ (or any C99-compatible compiler)
- Make
- For WASM: [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)

### Native CLI Build

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/hospital-system.git
cd hospital-system

# Build release binary
make

# Run interactive CLI
make run
# OR
./hospital
```

### Debug Build

```bash
make debug
./hospital_debug
```

### WebAssembly Build

```bash
# Activate Emscripten (after installing emsdk)
source /path/to/emsdk/emsdk_env.sh

# Compile to WASM + JS glue
make wasm

# This generates:
#   web/hospital.js   (JS glue code)
#   web/hospital.wasm (compiled binary)
```

Then add to `web/index.html` before `</body>`:
```html
<script src="hospital.js"></script>
```

---

## 🚀 How to Run

### Option A — Open directly in browser (Demo mode)
```bash
# Just open web/index.html in any modern browser
open web/index.html
# The JS fallback demo mode activates automatically with sample data
```

### Option B — Run with a local server (recommended for WASM)
```bash
# Python
python3 -m http.server 8080 --directory web/
# Visit http://localhost:8080

# Node.js
npx serve web/
```

### Option C — Native CLI
```bash
make run
```

---

## 🌐 Deploy to GitHub Pages

```bash
# 1. Create a GitHub repository named: hospital-system

# 2. Push all files
git init
git add .
git commit -m "Initial commit — MediCore Hospital System"
git remote add origin https://github.com/YOUR_USERNAME/hospital-system.git
git push -u origin main

# 3. Build WASM (optional — the UI works without it via JS demo mode)
make wasm
git add web/hospital.js web/hospital.wasm
git commit -m "Add WASM build"
git push

# 4. Enable GitHub Pages:
#    → Repository Settings → Pages
#    → Source: Deploy from a branch
#    → Branch: main, Folder: /web
#    → Save

# 5. Your app will be live at:
#    https://YOUR_USERNAME.github.io/hospital-system/
```

> **Note:** GitHub Pages serves static files. The web UI runs in full JS demo mode without WASM, or with WASM if `hospital.js` and `hospital.wasm` are committed to the repo. File I/O via WASM uses Emscripten's virtual filesystem (in-memory).

---

## 💻 Example CLI Output

```
╔══════════════════════════════════════════════════╗
║   Hospital Patient Registration & Follow-up      ║
║              Tracker v1.0                        ║
╚══════════════════════════════════════════════════╝

[INFO] Loaded 8 patient records from data/patients.txt

══ MAIN MENU ══
  1. Register new patient
  2. Search patient by name
  3. Display all patients
  4. Record patient visit
  5. View patient visit history
  6. Schedule appointment
  7. Triage priority queue
  8. Dashboard statistics
  ...

── TRIAGE PRIORITY QUEUE ──
   1. [!!!CRITICAL]  Rajan Krishnan                 ID:1001 Age:67  Disease:Diabetes Type 2
   2. [!!!CRITICAL]  Mohammed Farouk                ID:1003 Age:55  Disease:Coronary Artery Disease
   3. [!URGENT]      Priya Sharma                   ID:1000 Age:45  Disease:Hypertension
   4. [!URGENT]      Venkat Subramanian             ID:1005 Age:72  Disease:Arthritis
   5. [routine]      Sunita Devi                    ID:1002 Age:32  Disease:Asthma
```

---

## 🧪 Validation Examples

| Input | Validation Rule | Result |
|-------|----------------|--------|
| Phone: `98765` | Must be exactly 10 digits | ❌ ERR_FORMAT |
| Phone: `9876543210` | Must be exactly 10 digits | ✅ VALID |
| Age: `150` | Must be 1–120 | ❌ ERR_RANGE |
| Age: `45` | Must be 1–120 | ✅ VALID |
| Date: `2025/01/15` | Must be YYYY-MM-DD | ❌ ERR_FORMAT |
| Date: `2025-01-15` | Must be YYYY-MM-DD | ✅ VALID |
| Severity: `5` | Must be 1/2/3 | ❌ ERR_RANGE |
| Name: ` ` (spaces) | Must not be whitespace-only | ❌ ERR_EMPTY |

---

## 📊 Line Count Summary

| File | Lines |
|------|-------|
| `src/main.c` | ~684 |
| `src/patient.c` | ~190 |
| `src/appointment.c` | ~170 |
| `src/visit.c` | ~140 |
| `src/analytics.c` | ~180 |
| `src/validation.c` | ~115 |
| `web/script.js` | ~931 |
| `web/style.css` | ~430 |
| **Total** | **~2,840** |

---

## 📸 Interface Screenshots

| Dashboard | Patient Registry |
|-----------|-----------------|
| *(Statistics, donut chart, bar charts)* | *(Sortable table with CRUD actions)* |

| Triage Queue | Appointments |
|-------------|-------------|
| *(Colour-coded severity ranking)* | *(Schedule with conflict detection)* |

---

## 📄 License

This project is submitted as a university assignment. All code is original.

---

*Built with C99 + Emscripten + Vanilla JS. No external C libraries required.*
