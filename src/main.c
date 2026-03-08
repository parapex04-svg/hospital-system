/* ============================================================
 * main.c — Hospital Patient Registration & Follow-up Tracker
 *
 * Entry point for both:
 *   (a) Native CLI mode (standard C build)
 *   (b) WebAssembly mode (Emscripten — functions exported to JS)
 *
 * Architecture:
 *   PatientStore, AppointmentStore, VisitStore are kept as
 *   module-level globals so exported functions can access them
 *   without passing pointers from JavaScript.
 *
 * Compile (native):
 *   gcc -Wall -Wextra -o hospital src/main.c src/patient.c \
 *       src/appointment.c src/visit.c src/analytics.c \
 *       src/validation.c -Iinclude
 *
 * Compile (WASM):
 *   emcc src/main.c src/patient.c src/appointment.c \
 *        src/visit.c src/analytics.c src/validation.c \
 *        -Iinclude -o web/hospital.js \
 *        -s WASM=1 -s FORCE_FILESYSTEM=1 \
 *        -s EXPORTED_FUNCTIONS='["_hs_init","_hs_add_patient",
 *          "_hs_get_all_patients_json","_hs_delete_patient",
 *          "_hs_update_patient","_hs_search_patient",
 *          "_hs_add_visit","_hs_get_visits_json",
 *          "_hs_add_appointment","_hs_get_appointments_json",
 *          "_hs_get_dashboard_json","_hs_save_all","_hs_load_all",
 *          "_hs_triage_queue_json"]' \
 *        -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8"]'
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "../include/patient.h"
#include "../include/appointment.h"
#include "../include/visit.h"
#include "../include/analytics.h"
#include "../include/validation.h"

#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
  #define EXPORT EMSCRIPTEN_KEEPALIVE
#else
  #define EXPORT
#endif

/* ═══════════════════════════════════════════════════════════════
 * Module-level global state (single instance per process / WASM)
 * ═══════════════════════════════════════════════════════════════ */
static PatientStore     g_patients;
static AppointmentStore g_appts;
static VisitStore       g_visits;
static char             g_today[MAX_DATE_LEN];    /* YYYY-MM-DD */
static char             g_json_buf[65536];        /* shared JSON output buffer */
static int              g_initialized = 0;

/* ── Helper: get today's date string ────────────────────────── */
static void set_today(void) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(g_today, sizeof(g_today), "%Y-%m-%d", tm_info);
}

/* ── JSON helpers (minimal, no external library needed) ──────── */
/* Escape a string for JSON output */
static void json_escape(const char *src, char *dst, int maxlen) {
    int di = 0;
    for (int si = 0; src[si] && di < maxlen - 2; si++) {
        char c = src[si];
        if      (c == '"')  { dst[di++] = '\\'; dst[di++] = '"'; }
        else if (c == '\\') { dst[di++] = '\\'; dst[di++] = '\\'; }
        else if (c == '\n') { dst[di++] = '\\'; dst[di++] = 'n'; }
        else if (c == '\r') { dst[di++] = '\\'; dst[di++] = 'r'; }
        else                { dst[di++] = c; }
    }
    dst[di] = '\0';
}

/* Append formatted string to g_json_buf */
static int json_pos = 0;  /* current write position in g_json_buf */
static void jbuf_reset(void)             { json_pos = 0; g_json_buf[0] = '\0'; }
static void jbuf_cat(const char *s)      { 
    int rem = (int)sizeof(g_json_buf) - json_pos - 1;
    if (rem <= 0) return;
    int sl = (int)strlen(s);
    if (sl > rem) sl = rem;
    memcpy(g_json_buf + json_pos, s, sl);
    json_pos += sl;
    g_json_buf[json_pos] = '\0';
}
static void jbuf_printf(const char *fmt, ...) {
    char tmp[512];
    va_list ap; va_start(ap, fmt); vsnprintf(tmp, sizeof(tmp), fmt, ap); va_end(ap);
    jbuf_cat(tmp);
}

/* ── Patient to JSON object ──────────────────────────────────── */
static void patient_to_json(const Patient *p, int trailing_comma) {
    char esc_name[MAX_NAME_LEN*2], esc_addr[MAX_ADDRESS_LEN*2];
    char esc_dis[MAX_DISEASE_LEN*2], esc_doc[MAX_DOCTOR_LEN*2];
    json_escape(p->name,    esc_name, sizeof(esc_name));
    json_escape(p->address, esc_addr, sizeof(esc_addr));
    json_escape(p->disease, esc_dis,  sizeof(esc_dis));
    json_escape(p->doctor,  esc_doc,  sizeof(esc_doc));

    const char *sev_label = (p->severity==3)?"HIGH":(p->severity==2)?"MEDIUM":"LOW";

    jbuf_printf("{\"id\":%d,\"name\":\"%s\",\"age\":%d,"
                "\"gender\":\"%c\",\"phone\":\"%s\","
                "\"address\":\"%s\",\"disease\":\"%s\","
                "\"severity\":%d,\"severity_label\":\"%s\","
                "\"doctor\":\"%s\",\"reg_date\":\"%s\","
                "\"visit_count\":%d}",
                p->id, esc_name, p->age, p->gender, p->phone,
                esc_addr, esc_dis, p->severity, sev_label,
                esc_doc, p->reg_date, p->visit_count);
    if (trailing_comma) jbuf_cat(",");
}

/* ── Appointment to JSON ─────────────────────────────────────── */
static void appt_to_json(const Appointment *a, int trailing_comma) {
    const char *status_str[] = {"SCHEDULED","COMPLETED","CANCELLED","NO-SHOW"};
    char esc_name[MAX_NAME_LEN*2], esc_doc[MAX_DOCTOR_LEN*2];
    char esc_reason[MAX_NOTES_LEN*2];
    json_escape(a->patient_name, esc_name,   sizeof(esc_name));
    json_escape(a->doctor,       esc_doc,    sizeof(esc_doc));
    json_escape(a->reason,       esc_reason, sizeof(esc_reason));

    jbuf_printf("{\"appt_id\":%d,\"patient_id\":%d,"
                "\"patient_name\":\"%s\",\"doctor\":\"%s\","
                "\"date\":\"%s\",\"time_slot\":\"%s\","
                "\"reason\":\"%s\",\"status\":\"%s\","
                "\"duration_mins\":%d}",
                a->appt_id, a->patient_id, esc_name, esc_doc,
                a->date, a->time_slot, esc_reason,
                status_str[a->status < 4 ? a->status : 0],
                a->duration_mins);
    if (trailing_comma) jbuf_cat(",");
}

/* ── Visit to JSON ───────────────────────────────────────────── */
static void visit_to_json(const Visit *v, int trailing_comma) {
    char esc_name[MAX_NAME_LEN*2], esc_doc[MAX_DOCTOR_LEN*2];
    char esc_diag[MAX_DISEASE_LEN*2], esc_rx[MAX_PRESCRIPTION_LEN*2];
    char esc_notes[MAX_NOTES_LEN*2];
    json_escape(v->patient_name,  esc_name,  sizeof(esc_name));
    json_escape(v->doctor,        esc_doc,   sizeof(esc_doc));
    json_escape(v->diagnosis,     esc_diag,  sizeof(esc_diag));
    json_escape(v->prescription,  esc_rx,    sizeof(esc_rx));
    json_escape(v->notes,         esc_notes, sizeof(esc_notes));
    const char *sev_label = (v->severity_at_visit==3)?"HIGH":
                            (v->severity_at_visit==2)?"MEDIUM":"LOW";

    jbuf_printf("{\"visit_id\":%d,\"patient_id\":%d,"
                "\"patient_name\":\"%s\",\"doctor\":\"%s\","
                "\"visit_date\":\"%s\",\"diagnosis\":\"%s\","
                "\"prescription\":\"%s\",\"notes\":\"%s\","
                "\"severity_at_visit\":%d,\"severity_label\":\"%s\","
                "\"follow_up_days\":%d}",
                v->visit_id, v->patient_id, esc_name, esc_doc,
                v->visit_date, esc_diag, esc_rx, esc_notes,
                v->severity_at_visit, sev_label, v->follow_up_days);
    if (trailing_comma) jbuf_cat(",");
}

/* ═══════════════════════════════════════════════════════════════
 * EXPORTED FUNCTIONS — called from JavaScript
 * ═══════════════════════════════════════════════════════════════ */

/* ── Initialise all stores and load persisted data ───────────── */
EXPORT void hs_init(void) {
    if (g_initialized) return;
    patient_init_store(&g_patients);
    appt_init_store(&g_appts);
    visit_init_store(&g_visits);
    set_today();

    /* Attempt to load from persisted files */
    patient_load_from_file(&g_patients, PATIENT_FILE);
    appt_load_from_file(&g_appts, APPT_FILE);
    visit_load_from_file(&g_visits, VISIT_FILE);

    g_initialized = 1;
    printf("[HS] Initialised. Date: %s  Patients: %d\n",
           g_today, patient_count_active(&g_patients));
}

/* ── Save all stores to disk ─────────────────────────────────── */
EXPORT int hs_save_all(void) {
    int rc = 0;
    rc += patient_save_to_file(&g_patients, PATIENT_FILE);
    rc += appt_save_to_file(&g_appts, APPT_FILE);
    rc += visit_save_to_file(&g_visits, VISIT_FILE);
    return rc;
}

/* ── Load all stores from disk ───────────────────────────────── */
EXPORT int hs_load_all(void) {
    patient_load_from_file(&g_patients, PATIENT_FILE);
    appt_load_from_file(&g_appts, APPT_FILE);
    visit_load_from_file(&g_visits, VISIT_FILE);
    return 0;
}

/* ── Add patient — parameters passed as strings from JS ──────── */
/*   Returns new patient ID, or negative error code              */
EXPORT int hs_add_patient(const char *name, int age, char gender,
                          const char *phone, const char *address,
                          const char *disease, int severity,
                          const char *doctor, const char *reg_date) {
    Patient p;
    memset(&p, 0, sizeof(p));

    strncpy(p.name,    name,    MAX_NAME_LEN    - 1);
    strncpy(p.phone,   phone,   MAX_PHONE_LEN   - 1);
    strncpy(p.address, address, MAX_ADDRESS_LEN - 1);
    strncpy(p.disease, disease, MAX_DISEASE_LEN - 1);
    strncpy(p.doctor,  doctor,  MAX_DOCTOR_LEN  - 1);
    strncpy(p.reg_date, reg_date, MAX_DATE_LEN  - 1);
    p.age      = age;
    p.gender   = (char)gender;
    p.severity = severity;
    p.id       = 0;  /* auto-assign */

    /* Validate */
    int rc = val_patient_fields(&g_patients, &p, 1);
    if (rc != VALID) return rc;

    return patient_add(&g_patients, &p);
}

/* ── Update patient ──────────────────────────────────────────── */
EXPORT int hs_update_patient(int id, const char *name, int age, char gender,
                             const char *phone, const char *address,
                             const char *disease, int severity,
                             const char *doctor) {
    Patient p;
    memset(&p, 0, sizeof(p));

    strncpy(p.name,    name,    MAX_NAME_LEN    - 1);
    strncpy(p.phone,   phone,   MAX_PHONE_LEN   - 1);
    strncpy(p.address, address, MAX_ADDRESS_LEN - 1);
    strncpy(p.disease, disease, MAX_DISEASE_LEN - 1);
    strncpy(p.doctor,  doctor,  MAX_DOCTOR_LEN  - 1);
    strncpy(p.reg_date, g_today, MAX_DATE_LEN   - 1);
    p.age      = age;
    p.gender   = (char)gender;
    p.severity = severity;
    p.id       = id;

    int rc = val_patient_fields(&g_patients, &p, 0);
    if (rc != VALID) return rc;

    return patient_update(&g_patients, id, &p);
}

/* ── Delete patient (soft delete) ────────────────────────────── */
EXPORT int hs_delete_patient(int id) {
    return patient_delete(&g_patients, id);
}

/* ── Search patients by name — returns JSON array ────────────── */
EXPORT const char *hs_search_patient(const char *name) {
    int ids[MAX_PATIENTS];
    int n = patient_search_by_name(&g_patients, name, ids, MAX_PATIENTS);
    jbuf_reset();
    jbuf_cat("[");
    for (int i = 0; i < n; i++) {
        Patient *p = patient_find_by_id(&g_patients, ids[i]);
        if (p) patient_to_json(p, i < n - 1);
    }
    jbuf_cat("]");
    return g_json_buf;
}

/* ── Get all active patients as JSON ─────────────────────────── */
EXPORT const char *hs_get_all_patients_json(void) {
    jbuf_reset();
    jbuf_cat("[");
    int first = 1;
    for (int i = 0; i < g_patients.count; i++) {
        if (!g_patients.records[i].is_active) continue;
        if (!first) jbuf_cat(",");
        patient_to_json(&g_patients.records[i], 0);
        first = 0;
    }
    jbuf_cat("]");
    return g_json_buf;
}

/* ── Add a visit record ──────────────────────────────────────── */
EXPORT int hs_add_visit(int patient_id, const char *doctor,
                        const char *visit_date, const char *diagnosis,
                        const char *prescription, const char *notes,
                        int severity_at_visit, int follow_up_days) {
    /* Validate patient exists */
    Patient *p = patient_find_by_id(&g_patients, patient_id);
    if (!p) return -1;

    Visit v;
    memset(&v, 0, sizeof(v));
    v.patient_id = patient_id;
    strncpy(v.patient_name,  p->name,       MAX_NAME_LEN         - 1);
    strncpy(v.doctor,        doctor,         MAX_DOCTOR_LEN       - 1);
    strncpy(v.visit_date,    visit_date,     MAX_DATE_LEN         - 1);
    strncpy(v.diagnosis,     diagnosis,      MAX_DISEASE_LEN      - 1);
    strncpy(v.prescription,  prescription,   MAX_PRESCRIPTION_LEN - 1);
    strncpy(v.notes,         notes,          MAX_NOTES_LEN        - 1);
    v.severity_at_visit = severity_at_visit;
    v.follow_up_days    = follow_up_days;

    int vid = visit_add(&g_visits, &v);
    if (vid > 0) {
        p->visit_count++;   /* update patient's visit counter */
        p->severity = severity_at_visit; /* update current severity */
    }
    return vid;
}

/* ── Get visits for a patient as JSON ────────────────────────── */
EXPORT const char *hs_get_visits_json(int patient_id) {
    jbuf_reset();
    jbuf_cat("[");
    int first = 1;
    for (int i = 0; i < g_visits.count; i++) {
        if (g_visits.records[i].patient_id != patient_id) continue;
        if (!first) jbuf_cat(",");
        visit_to_json(&g_visits.records[i], 0);
        first = 0;
    }
    jbuf_cat("]");
    return g_json_buf;
}

/* ── Add appointment (with conflict detection) ───────────────── */
EXPORT int hs_add_appointment(int patient_id, const char *doctor,
                              const char *date, const char *time_slot,
                              const char *reason, int duration_mins) {
    /* Validate date and time */
    if (val_date(date)       != VALID) return ERR_FORMAT;
    if (val_time_slot(time_slot) != VALID) return ERR_FORMAT;

    Patient *p = patient_find_by_id(&g_patients, patient_id);
    if (!p) return -1;

    /* Conflict check */
    if (appt_conflict_exists(&g_appts, doctor, date, time_slot,
                             duration_mins > 0 ? duration_mins : 30, -1)) {
        fprintf(stderr, "[ERROR] Appointment conflict detected.\n");
        return -2;
    }

    Appointment a;
    memset(&a, 0, sizeof(a));
    a.patient_id    = patient_id;
    a.duration_mins = duration_mins > 0 ? duration_mins : 30;
    strncpy(a.patient_name, p->name,   MAX_NAME_LEN  - 1);
    strncpy(a.doctor,       doctor,    MAX_DOCTOR_LEN - 1);
    strncpy(a.date,         date,      MAX_DATE_LEN  - 1);
    strncpy(a.time_slot,    time_slot, 7);
    strncpy(a.reason,       reason,    MAX_NOTES_LEN - 1);

    return appt_add(&g_appts, &a);
}

/* ── Get all appointments as JSON ────────────────────────────── */
EXPORT const char *hs_get_appointments_json(void) {
    jbuf_reset();
    jbuf_cat("[");
    for (int i = 0; i < g_appts.count; i++) {
        appt_to_json(&g_appts.records[i], i < g_appts.count - 1);
    }
    jbuf_cat("]");
    return g_json_buf;
}

/* ── Cancel appointment ──────────────────────────────────────── */
EXPORT int hs_cancel_appointment(int appt_id) {
    return appt_cancel(&g_appts, appt_id);
}

/* ── Get dashboard stats as JSON ─────────────────────────────── */
EXPORT const char *hs_get_dashboard_json(void) {
    DashboardStats stats;
    analytics_compute(&stats, &g_patients, &g_visits, &g_appts, g_today);

    jbuf_reset();
    jbuf_printf("{\"total_patients\":%d,\"active_patients\":%d,"
                "\"total_visits\":%d,\"total_appointments\":%d,"
                "\"appointments_today\":%d,"
                "\"high_severity\":%d,\"medium_severity\":%d,\"low_severity\":%d,",
                stats.total_patients, stats.active_patients,
                stats.total_visits, stats.total_appointments,
                stats.appointments_today,
                stats.high_severity_count, stats.medium_severity_count,
                stats.low_severity_count);

    /* Disease frequency array */
    jbuf_cat("\"top_diseases\":[");
    int top = stats.disease_count < 10 ? stats.disease_count : 10;
    for (int i = 0; i < top; i++) {
        char esc[MAX_DISEASE_LEN*2];
        json_escape(stats.top_diseases[i].disease, esc, sizeof(esc));
        jbuf_printf("{\"disease\":\"%s\",\"count\":%d}%s",
                    esc, stats.top_diseases[i].count,
                    i < top-1 ? "," : "");
    }
    jbuf_cat("],");

    /* Doctor workload array */
    jbuf_cat("\"doctor_workload\":[");
    for (int i = 0; i < stats.doctor_count; i++) {
        char esc[MAX_DOCTOR_LEN*2];
        json_escape(stats.workloads[i].doctor, esc, sizeof(esc));
        jbuf_printf("{\"doctor\":\"%s\",\"patients\":%d,"
                    "\"appointments\":%d,\"visits\":%d}%s",
                    esc,
                    stats.workloads[i].patient_count,
                    stats.workloads[i].appointment_count,
                    stats.workloads[i].visit_count,
                    i < stats.doctor_count-1 ? "," : "");
    }
    jbuf_cat("]}");
    return g_json_buf;
}

/* ── Triage queue as JSON ────────────────────────────────────── */
EXPORT const char *hs_triage_queue_json(void) {
    int ids[MAX_PATIENTS];
    int n = analytics_triage_queue(&g_patients, ids, MAX_PATIENTS);

    jbuf_reset();
    jbuf_cat("[");
    for (int i = 0; i < n; i++) {
        Patient *p = patient_find_by_id(&g_patients, ids[i]);
        if (p) patient_to_json(p, i < n - 1);
    }
    jbuf_cat("]");
    return g_json_buf;
}

/* ── Create a data backup ────────────────────────────────────── */
EXPORT int hs_backup(void) {
    char backup_path[128];
    snprintf(backup_path, sizeof(backup_path),
             "data/backup_patients_%s.txt", g_today);
    return patient_save_to_file(&g_patients, backup_path);
}

/* ═══════════════════════════════════════════════════════════════
 * CLI / interactive mode (native binary only)
 * ═══════════════════════════════════════════════════════════════ */
#ifndef __EMSCRIPTEN__

/* Safely read a line, stripping newline */
static void read_line(const char *prompt, char *buf, int maxlen) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, maxlen, stdin)) buf[0] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';
}

static int read_int(const char *prompt) {
    char buf[32];
    read_line(prompt, buf, sizeof(buf));
    return atoi(buf);
}

/* ── CLI: Register new patient ───────────────────────────────── */
static void cli_register_patient(void) {
    Patient p;
    memset(&p, 0, sizeof(p));
    char gender_str[4];

    printf("\n── NEW PATIENT REGISTRATION ──\n");
    read_line("  Full Name:   ", p.name,    MAX_NAME_LEN);
    p.age = read_int("  Age:        ");
    read_line("  Gender (M/F/O): ", gender_str, sizeof(gender_str));
    p.gender = gender_str[0];
    read_line("  Phone (10 digits): ", p.phone, MAX_PHONE_LEN);
    read_line("  Address:     ", p.address, MAX_ADDRESS_LEN);
    read_line("  Disease/Diagnosis: ", p.disease, MAX_DISEASE_LEN);
    p.severity = read_int("  Severity (1=Low, 2=Medium, 3=High): ");
    read_line("  Doctor name: ", p.doctor,  MAX_DOCTOR_LEN);

    strncpy(p.reg_date, g_today, MAX_DATE_LEN - 1);
    p.id = 0;

    int rc = val_patient_fields(&g_patients, &p, 1);
    if (rc != VALID) {
        printf("  [VALIDATION FAILED] %s\n", val_error_msg(rc));
        return;
    }

    int new_id = patient_add(&g_patients, &p);
    if (new_id > 0)
        printf("  [SUCCESS] Patient registered with ID: %d\n", new_id);
    else
        printf("  [ERROR] Could not register patient.\n");
}

/* ── CLI: Search patients ────────────────────────────────────── */
static void cli_search_patient(void) {
    char query[MAX_NAME_LEN];
    printf("\n── SEARCH PATIENT ──\n");
    read_line("  Enter name (partial): ", query, sizeof(query));

    int ids[50];
    int n = patient_search_by_name(&g_patients, query, ids, 50);
    if (!n) { printf("  No patients found.\n"); return; }

    printf("  Found %d result(s):\n", n);
    for (int i = 0; i < n; i++) {
        Patient *p = patient_find_by_id(&g_patients, ids[i]);
        if (p) patient_print(p);
    }
}

/* ── CLI: Record a visit ─────────────────────────────────────── */
static void cli_record_visit(void) {
    printf("\n── RECORD VISIT ──\n");
    int pid = read_int("  Patient ID: ");
    Patient *p = patient_find_by_id(&g_patients, pid);
    if (!p) { printf("  [ERROR] Patient not found.\n"); return; }
    printf("  Patient: %s\n", p->name);

    Visit v;
    memset(&v, 0, sizeof(v));
    v.patient_id = pid;
    strncpy(v.patient_name, p->name, MAX_NAME_LEN - 1);
    read_line("  Doctor:       ", v.doctor,       MAX_DOCTOR_LEN);
    read_line("  Visit Date (YYYY-MM-DD): ", v.visit_date, MAX_DATE_LEN);
    if (v.visit_date[0] == '\0') strncpy(v.visit_date, g_today, MAX_DATE_LEN-1);
    read_line("  Diagnosis:    ", v.diagnosis,    MAX_DISEASE_LEN);
    read_line("  Prescription: ", v.prescription, MAX_PRESCRIPTION_LEN);
    read_line("  Notes:        ", v.notes,        MAX_NOTES_LEN);
    v.severity_at_visit = read_int("  Severity (1/2/3): ");
    v.follow_up_days    = read_int("  Follow-up in days (0=none): ");

    int vid = visit_add(&g_visits, &v);
    if (vid > 0) {
        p->visit_count++;
        p->severity = v.severity_at_visit;
        printf("  [SUCCESS] Visit #%d recorded.\n", vid);
    }
}

/* ── CLI: Schedule appointment ───────────────────────────────── */
static void cli_schedule_appointment(void) {
    printf("\n── SCHEDULE APPOINTMENT ──\n");
    int pid = read_int("  Patient ID: ");
    Patient *p = patient_find_by_id(&g_patients, pid);
    if (!p) { printf("  [ERROR] Patient not found.\n"); return; }

    Appointment a;
    memset(&a, 0, sizeof(a));
    a.patient_id = pid;
    strncpy(a.patient_name, p->name, MAX_NAME_LEN - 1);

    read_line("  Doctor:         ", a.doctor,    MAX_DOCTOR_LEN);
    read_line("  Date (YYYY-MM-DD): ", a.date,   MAX_DATE_LEN);
    read_line("  Time (HH:MM):   ", a.time_slot, 8);
    read_line("  Reason:         ", a.reason,    MAX_NOTES_LEN);
    a.duration_mins = read_int("  Duration (minutes, default 30): ");
    if (a.duration_mins <= 0) a.duration_mins = 30;

    /* Validate */
    if (val_date(a.date) != VALID) {
        printf("  [ERROR] Invalid date format.\n"); return;
    }
    if (val_time_slot(a.time_slot) != VALID) {
        printf("  [ERROR] Invalid time format.\n"); return;
    }

    if (appt_conflict_exists(&g_appts, a.doctor, a.date,
                              a.time_slot, a.duration_mins, -1)) {
        printf("  [CONFLICT] Dr. %s already has an appointment at %s on %s.\n",
               a.doctor, a.time_slot, a.date);
        return;
    }

    int aid = appt_add(&g_appts, &a);
    if (aid > 0)
        printf("  [SUCCESS] Appointment #%d scheduled.\n", aid);
}

/* ── CLI: Delete patient ─────────────────────────────────────── */
static void cli_delete_patient(void) {
    printf("\n── DELETE PATIENT ──\n");
    int pid = read_int("  Patient ID: ");
    Patient *p = patient_find_by_id(&g_patients, pid);
    if (!p) { printf("  [ERROR] Patient not found.\n"); return; }
    printf("  Deleting: %s (ID: %d)\n", p->name, pid);
    read_line("  Confirm? (y/n): ", (char[]){0}, 4);
    /* In CLI, just proceed */
    if (patient_delete(&g_patients, pid) == 0)
        printf("  [SUCCESS] Patient #%d removed.\n", pid);
}

/* ── CLI: Show dashboard ─────────────────────────────────────── */
static void cli_dashboard(void) {
    DashboardStats stats;
    analytics_compute(&stats, &g_patients, &g_visits, &g_appts, g_today);
    analytics_print_dashboard(&stats);
}

/* ── CLI: Triage queue ───────────────────────────────────────── */
static void cli_triage_queue(void) {
    printf("\n── TRIAGE PRIORITY QUEUE ──\n");
    int ids[MAX_PATIENTS];
    int n = analytics_triage_queue(&g_patients, ids, MAX_PATIENTS);
    if (!n) { printf("  (no active patients)\n"); return; }
    int rank = 1;
    for (int i = 0; i < n; i++) {
        Patient *p = patient_find_by_id(&g_patients, ids[i]);
        if (!p) continue;
        const char *sev = (p->severity==3)?"[!!!CRITICAL]":
                          (p->severity==2)?"[!URGENT]":"[routine]";
        printf("  %2d. %s  %-30s  ID:%d  Age:%d  Disease:%s\n",
               rank++, sev, p->name, p->id, p->age, p->disease);
    }
}

/* ── Main CLI loop ───────────────────────────────────────────── */
int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║   Hospital Patient Registration & Follow-up      ║\n");
    printf("║              Tracker v1.0                        ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");

    hs_init();

    int running = 1;
    while (running) {
        printf("\n══ MAIN MENU ══\n");
        printf("  1. Register new patient\n");
        printf("  2. Search patient by name\n");
        printf("  3. Display all patients\n");
        printf("  4. Record patient visit\n");
        printf("  5. View patient visit history\n");
        printf("  6. Schedule appointment\n");
        printf("  7. Triage priority queue\n");
        printf("  8. Dashboard statistics\n");
        printf("  9. Delete patient\n");
        printf(" 10. Save all data\n");
        printf(" 11. Backup data\n");
        printf("  0. Exit\n");
        printf("  Choice: ");

        int choice = read_int("");
        switch (choice) {
            case 1:  cli_register_patient();                             break;
            case 2:  cli_search_patient();                               break;
            case 3:  patient_print_all(&g_patients);                     break;
            case 4:  cli_record_visit();                                  break;
            case 5: {
                int pid = read_int("  Patient ID: ");
                visit_print_history(&g_visits, pid);                     break;
            }
            case 6:  cli_schedule_appointment();                          break;
            case 7:  cli_triage_queue();                                  break;
            case 8:  cli_dashboard();                                     break;
            case 9:  cli_delete_patient();                                break;
            case 10: hs_save_all();
                     printf("  [SUCCESS] All data saved.\n");             break;
            case 11: hs_backup();
                     printf("  [SUCCESS] Backup created.\n");             break;
            case 0:  running = 0;                                         break;
            default: printf("  Invalid choice.\n");                       break;
        }
    }

    hs_save_all();
    printf("\nGoodbye. All data saved.\n");
    return 0;
}

#endif /* !__EMSCRIPTEN__ */
