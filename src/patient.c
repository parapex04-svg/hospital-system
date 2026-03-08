/* ============================================================
 * patient.c — Patient CRUD operations and file persistence
 * Hospital Patient Registration & Follow-up Tracker
 * ============================================================ */

#include "../include/patient.h"
#include "../include/validation.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ── Initialise an empty patient store ──────────────────────── */
void patient_init_store(PatientStore *store) {
    memset(store, 0, sizeof(PatientStore));
    store->count   = 0;
    store->next_id = 1000;   /* IDs start at 1000 */
}

/* ── Generate the next unique patient ID ────────────────────── */
int patient_generate_id(PatientStore *store) {
    return store->next_id++;
}

/* ── Add a patient to the store ─────────────────────────────── */
int patient_add(PatientStore *store, Patient *p) {
    if (store->count >= MAX_PATIENTS) {
        fprintf(stderr, "[ERROR] Patient store is full.\n");
        return -1;
    }
    /* Assign auto ID if not set */
    if (p->id <= 0) {
        p->id = patient_generate_id(store);
    } else {
        /* Keep next_id consistent */
        if (p->id >= store->next_id) store->next_id = p->id + 1;
    }
    p->is_active = 1;
    store->records[store->count++] = *p;
    return p->id;
}

/* ── Find patient by ID (returns pointer or NULL) ────────────── */
Patient *patient_find_by_id(PatientStore *store, int id) {
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].id == id && store->records[i].is_active) {
            return &store->records[i];
        }
    }
    return NULL;
}

/* ── Find index by ID (returns -1 if not found) ─────────────── */
int patient_find_index_by_id(PatientStore *store, int id) {
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].id == id) return i;
    }
    return -1;
}

/* ── Case-insensitive substring name search ─────────────────── */
static int str_icontains(const char *haystack, const char *needle) {
    char h[MAX_NAME_LEN], n[MAX_NAME_LEN];
    int i;
    for (i = 0; haystack[i] && i < MAX_NAME_LEN-1; i++)
        h[i] = (char)tolower((unsigned char)haystack[i]);
    h[i] = '\0';
    for (i = 0; needle[i] && i < MAX_NAME_LEN-1; i++)
        n[i] = (char)tolower((unsigned char)needle[i]);
    n[i] = '\0';
    return strstr(h, n) != NULL;
}

int patient_search_by_name(PatientStore *store, const char *name,
                           int results[], int max_results) {
    int found = 0;
    for (int i = 0; i < store->count && found < max_results; i++) {
        if (!store->records[i].is_active) continue;
        if (str_icontains(store->records[i].name, name)) {
            results[found++] = store->records[i].id;
        }
    }
    return found;
}

/* ── Update patient by ID ────────────────────────────────────── */
int patient_update(PatientStore *store, int id, Patient *updated) {
    int idx = patient_find_index_by_id(store, id);
    if (idx < 0) return -1;
    updated->id        = id;              /* preserve ID      */
    updated->is_active = 1;              /* always active    */
    updated->visit_count = store->records[idx].visit_count; /* preserve visits */
    store->records[idx] = *updated;
    return 0;
}

/* ── Soft-delete patient ─────────────────────────────────────── */
int patient_delete(PatientStore *store, int id) {
    int idx = patient_find_index_by_id(store, id);
    if (idx < 0) return -1;
    store->records[idx].is_active = 0;
    return 0;
}

/* ── Count active patients ───────────────────────────────────── */
int patient_count_active(const PatientStore *store) {
    int n = 0;
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].is_active) n++;
    }
    return n;
}

/* ── Print a single patient record ──────────────────────────── */
void patient_print(const Patient *p) {
    const char *sev_str[] = {"", "LOW", "MEDIUM", "HIGH"};
    printf("┌──────────────────────────────────────────────────┐\n");
    printf("│  ID: %-6d  Name: %-30s │\n", p->id, p->name);
    printf("│  Age: %-3d  Gender: %c  Phone: %-12s         │\n",
           p->age, p->gender, p->phone);
    printf("│  Disease: %-38s  │\n", p->disease);
    printf("│  Doctor: %-39s  │\n", p->doctor);
    printf("│  Severity: %-7s  Visits: %-3d  Reg: %-10s │\n",
           sev_str[p->severity], p->visit_count, p->reg_date);
    printf("│  Address: %-38s  │\n", p->address);
    printf("└──────────────────────────────────────────────────┘\n");
}

/* ── Print all active patients ───────────────────────────────── */
void patient_print_all(const PatientStore *store) {
    printf("\n═══ PATIENT REGISTRY (%d active) ═══\n",
           patient_count_active(store));
    int shown = 0;
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].is_active) {
            patient_print(&store->records[i]);
            shown++;
        }
    }
    if (!shown) printf("  (no patients registered)\n");
}

/* ── Save patient store to text file ─────────────────────────── */
/* Format: one patient per line, pipe-delimited                   */
int patient_save_to_file(PatientStore *store, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open %s for writing.\n", path);
        return -1;
    }

    /* Header comment */
    fprintf(fp, "# Hospital Patient Registry — DO NOT EDIT MANUALLY\n");
    fprintf(fp, "# FORMAT: id|name|age|gender|phone|address|disease|"
                "severity|doctor|reg_date|is_active|visit_count\n");
    fprintf(fp, "# NEXT_ID: %d\n", store->next_id);

    for (int i = 0; i < store->count; i++) {
        Patient *p = &store->records[i];
        fprintf(fp, "%d|%s|%d|%c|%s|%s|%s|%d|%s|%s|%d|%d\n",
                p->id, p->name, p->age, p->gender, p->phone,
                p->address, p->disease, p->severity, p->doctor,
                p->reg_date, p->is_active, p->visit_count);
    }

    fclose(fp);
    printf("[INFO] %d patient records saved to %s\n", store->count, path);
    return 0;
}

/* ── Load patient store from text file ───────────────────────── */
int patient_load_from_file(PatientStore *store, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        /* File not found is acceptable on first run */
        printf("[INFO] No existing patient file found at %s. Starting fresh.\n", path);
        return 0;
    }

    patient_init_store(store);
    char line[512];
    int  loaded = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        /* Parse NEXT_ID directive */
        if (strncmp(line, "# NEXT_ID:", 10) == 0) {
            store->next_id = atoi(line + 10);
            continue;
        }

        Patient p;
        memset(&p, 0, sizeof(p));

        /* sscanf pipe-delimited */
        int rc = sscanf(line,
            "%d|%63[^|]|%d|%c|%15[^|]|%127[^|]|%63[^|]|%d|%63[^|]|%15[^|]|%d|%d",
            &p.id, p.name, &p.age, &p.gender, p.phone,
            p.address, p.disease, &p.severity, p.doctor,
            p.reg_date, &p.is_active, &p.visit_count);

        if (rc == 12 && store->count < MAX_PATIENTS) {
            store->records[store->count++] = p;
            if (p.id >= store->next_id) store->next_id = p.id + 1;
            loaded++;
        }
    }

    fclose(fp);
    printf("[INFO] Loaded %d patient records from %s\n", loaded, path);
    return loaded;
}
