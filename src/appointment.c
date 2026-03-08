/* ============================================================
 * appointment.c — Appointment scheduling with conflict detection
 * Hospital Patient Registration & Follow-up Tracker
 * ============================================================ */

#include "../include/appointment.h"
#include "../include/validation.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Convert "HH:MM" to total minutes since midnight */
static int time_to_minutes(const char *t) {
    if (!t || strlen(t) < 5) return -1;
    int h = atoi(t);
    int m = atoi(t + 3);
    return h * 60 + m;
}

/* ── Initialise appointment store ───────────────────────────── */
void appt_init_store(AppointmentStore *store) {
    memset(store, 0, sizeof(AppointmentStore));
    store->count   = 0;
    store->next_id = 1;
}

/* ── Add a new appointment ───────────────────────────────────── */
int appt_add(AppointmentStore *store, Appointment *a) {
    if (store->count >= MAX_APPOINTMENTS) {
        fprintf(stderr, "[ERROR] Appointment store is full.\n");
        return -1;
    }
    if (a->appt_id <= 0) {
        a->appt_id = store->next_id++;
    } else {
        if (a->appt_id >= store->next_id) store->next_id = a->appt_id + 1;
    }
    if (a->duration_mins <= 0) a->duration_mins = 30; /* default 30 min */
    a->status = APPT_SCHEDULED;
    store->records[store->count++] = *a;
    return a->appt_id;
}

/* ── Conflict detection ──────────────────────────────────────── */
/*   Returns 1 if new slot overlaps an existing scheduled appt    */
/*   for the same doctor on the same date.                        */
int appt_conflict_exists(AppointmentStore *store,
                         const char *doctor,
                         const char *date,
                         const char *time_slot,
                         int         duration_mins,
                         int         exclude_id) {
    int new_start = time_to_minutes(time_slot);
    int new_end   = new_start + duration_mins;
    if (new_start < 0) return 0;

    for (int i = 0; i < store->count; i++) {
        Appointment *a = &store->records[i];
        if (a->appt_id  == exclude_id) continue;
        if (a->status   == APPT_CANCELLED) continue;
        if (strcmp(a->doctor, doctor) != 0) continue;
        if (strcmp(a->date, date)     != 0) continue;

        int ex_start = time_to_minutes(a->time_slot);
        int ex_end   = ex_start + a->duration_mins;
        /* Overlap: new starts before existing ends AND new ends after existing starts */
        if (new_start < ex_end && new_end > ex_start) {
            return 1; /* conflict */
        }
    }
    return 0;
}

/* ── Cancel appointment ──────────────────────────────────────── */
int appt_cancel(AppointmentStore *store, int appt_id) {
    Appointment *a = appt_find_by_id(store, appt_id);
    if (!a) return -1;
    a->status = APPT_CANCELLED;
    return 0;
}

/* ── Mark appointment as completed ──────────────────────────── */
int appt_complete(AppointmentStore *store, int appt_id) {
    Appointment *a = appt_find_by_id(store, appt_id);
    if (!a) return -1;
    a->status = APPT_COMPLETED;
    return 0;
}

/* ── Find appointment by ID ──────────────────────────────────── */
Appointment *appt_find_by_id(AppointmentStore *store, int id) {
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].appt_id == id) return &store->records[i];
    }
    return NULL;
}

/* ── Get appointments by patient ─────────────────────────────── */
int appt_get_by_patient(AppointmentStore *store, int patient_id,
                        int out_ids[], int max_out) {
    int n = 0;
    for (int i = 0; i < store->count && n < max_out; i++) {
        if (store->records[i].patient_id == patient_id)
            out_ids[n++] = store->records[i].appt_id;
    }
    return n;
}

/* ── Get appointments by doctor ──────────────────────────────── */
int appt_get_by_doctor(AppointmentStore *store, const char *doctor,
                       int out_ids[], int max_out) {
    int n = 0;
    for (int i = 0; i < store->count && n < max_out; i++) {
        if (strcmp(store->records[i].doctor, doctor) == 0)
            out_ids[n++] = store->records[i].appt_id;
    }
    return n;
}

/* ── Print a single appointment ──────────────────────────────── */
void appt_print(const Appointment *a) {
    const char *status_str[] = {"SCHEDULED","COMPLETED","CANCELLED","NO-SHOW"};
    printf("  [APPT #%d] Patient: %s (ID:%d) | Doctor: %s\n",
           a->appt_id, a->patient_name, a->patient_id, a->doctor);
    printf("            Date: %s  Time: %s  Duration: %dmin  Status: %s\n",
           a->date, a->time_slot, a->duration_mins,
           status_str[a->status < 4 ? a->status : 0]);
    printf("            Reason: %s\n", a->reason);
}

/* ── Save appointments to file ───────────────────────────────── */
int appt_save_to_file(AppointmentStore *store, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open %s for writing.\n", path);
        return -1;
    }

    fprintf(fp, "# Appointments — FORMAT: appt_id|patient_id|patient_name|"
                "doctor|date|time_slot|reason|status|duration_mins\n");
    fprintf(fp, "# NEXT_ID: %d\n", store->next_id);

    for (int i = 0; i < store->count; i++) {
        Appointment *a = &store->records[i];
        fprintf(fp, "%d|%d|%s|%s|%s|%s|%s|%d|%d\n",
                a->appt_id, a->patient_id, a->patient_name,
                a->doctor, a->date, a->time_slot,
                a->reason, a->status, a->duration_mins);
    }
    fclose(fp);
    return 0;
}

/* ── Load appointments from file ─────────────────────────────── */
int appt_load_from_file(AppointmentStore *store, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("[INFO] No appointment file at %s. Starting fresh.\n", path);
        return 0;
    }

    appt_init_store(store);
    char line[512];
    int  loaded = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        Appointment a;
        memset(&a, 0, sizeof(a));

        int rc = sscanf(line,
            "%d|%d|%63[^|]|%63[^|]|%15[^|]|%7[^|]|%255[^|]|%d|%d",
            &a.appt_id, &a.patient_id, a.patient_name,
            a.doctor, a.date, a.time_slot,
            a.reason, &a.status, &a.duration_mins);

        if (rc == 9 && store->count < MAX_APPOINTMENTS) {
            store->records[store->count++] = a;
            if (a.appt_id >= store->next_id) store->next_id = a.appt_id + 1;
            loaded++;
        }
    }
    fclose(fp);
    return loaded;
}
