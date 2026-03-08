/* ============================================================
 * visit.c — Patient visit / encounter recording
 * Hospital Patient Registration & Follow-up Tracker
 * ============================================================ */

#include "../include/visit.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Initialise visit store ──────────────────────────────────── */
void visit_init_store(VisitStore *store) {
    memset(store, 0, sizeof(VisitStore));
    store->count   = 0;
    store->next_id = 1;
}

/* ── Add a visit record ──────────────────────────────────────── */
int visit_add(VisitStore *store, Visit *v) {
    if (store->count >= MAX_VISITS) {
        fprintf(stderr, "[ERROR] Visit store is full.\n");
        return -1;
    }
    if (v->visit_id <= 0) {
        v->visit_id = store->next_id++;
    } else {
        if (v->visit_id >= store->next_id) store->next_id = v->visit_id + 1;
    }
    store->records[store->count++] = *v;
    return v->visit_id;
}

/* ── Find visit by ID ────────────────────────────────────────── */
Visit *visit_find_by_id(VisitStore *store, int id) {
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].visit_id == id) return &store->records[i];
    }
    return NULL;
}

/* ── Get all visit IDs for a patient ─────────────────────────── */
int visit_get_by_patient(VisitStore *store, int patient_id,
                         int out_ids[], int max_out) {
    int n = 0;
    for (int i = 0; i < store->count && n < max_out; i++) {
        if (store->records[i].patient_id == patient_id)
            out_ids[n++] = store->records[i].visit_id;
    }
    return n;
}

/* ── Print a single visit record ─────────────────────────────── */
void visit_print(const Visit *v) {
    const char *sev[] = {"", "LOW", "MEDIUM", "HIGH"};
    printf("  ├─ Visit #%-4d  Date: %s  Doctor: %s\n",
           v->visit_id, v->visit_date, v->doctor);
    printf("  │   Diagnosis: %-40s Severity: %s\n",
           v->diagnosis, sev[v->severity_at_visit < 4 ? v->severity_at_visit : 0]);
    printf("  │   Prescription: %s\n", v->prescription);
    if (v->notes[0]) printf("  │   Notes: %s\n", v->notes);
    if (v->follow_up_days > 0)
        printf("  │   Follow-up in: %d days\n", v->follow_up_days);
}

/* ── Print full visit history for a patient ─────────────────── */
void visit_print_history(VisitStore *store, int patient_id) {
    printf("\n── Visit History for Patient #%d ──\n", patient_id);
    int found = 0;
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].patient_id == patient_id) {
            visit_print(&store->records[i]);
            found++;
        }
    }
    if (!found) printf("  (no visits recorded)\n");
    printf("  Total visits: %d\n", found);
}

/* ── Save visits to file ─────────────────────────────────────── */
int visit_save_to_file(VisitStore *store, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open %s for writing.\n", path);
        return -1;
    }

    fprintf(fp, "# Visits — FORMAT: visit_id|patient_id|patient_name|"
                "doctor|visit_date|diagnosis|prescription|notes|"
                "severity_at_visit|follow_up_days\n");
    fprintf(fp, "# NEXT_ID: %d\n", store->next_id);

    for (int i = 0; i < store->count; i++) {
        Visit *v = &store->records[i];
        fprintf(fp, "%d|%d|%s|%s|%s|%s|%s|%s|%d|%d\n",
                v->visit_id, v->patient_id, v->patient_name,
                v->doctor, v->visit_date, v->diagnosis,
                v->prescription, v->notes,
                v->severity_at_visit, v->follow_up_days);
    }
    fclose(fp);
    return 0;
}

/* ── Load visits from file ───────────────────────────────────── */
int visit_load_from_file(VisitStore *store, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("[INFO] No visit file at %s. Starting fresh.\n", path);
        return 0;
    }

    visit_init_store(store);
    char line[1024];
    int  loaded = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        Visit v;
        memset(&v, 0, sizeof(v));

        int rc = sscanf(line,
            "%d|%d|%63[^|]|%63[^|]|%15[^|]|%63[^|]|%255[^|]|%255[^|]|%d|%d",
            &v.visit_id, &v.patient_id, v.patient_name,
            v.doctor, v.visit_date, v.diagnosis,
            v.prescription, v.notes,
            &v.severity_at_visit, &v.follow_up_days);

        if (rc >= 9 && store->count < MAX_VISITS) {
            store->records[store->count++] = v;
            if (v.visit_id >= store->next_id) store->next_id = v.visit_id + 1;
            loaded++;
        }
    }
    fclose(fp);
    return loaded;
}
