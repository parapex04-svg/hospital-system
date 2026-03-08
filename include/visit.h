#ifndef VISIT_H
#define VISIT_H

/* ============================================================
 * visit.h — Patient visit / encounter structures
 * ============================================================ */

#include "patient.h"

#define MAX_VISITS      1000
#define VISIT_FILE      "data/visits.txt"
#define MAX_PRESCRIPTION_LEN 256

typedef struct {
    int  visit_id;
    int  patient_id;
    char patient_name[MAX_NAME_LEN];
    char doctor[MAX_DOCTOR_LEN];
    char visit_date[MAX_DATE_LEN];   /* YYYY-MM-DD   */
    char diagnosis[MAX_DISEASE_LEN];
    char prescription[MAX_PRESCRIPTION_LEN];
    char notes[MAX_NOTES_LEN];
    int  severity_at_visit;          /* 1/2/3        */
    int  follow_up_days;             /* 0 = no f/up  */
} Visit;

typedef struct {
    Visit records[MAX_VISITS];
    int   count;
    int   next_id;
} VisitStore;

/* ── Function declarations ──────────────────────────────────── */
void   visit_init_store(VisitStore *store);
int    visit_add(VisitStore *store, Visit *v);
int    visit_get_by_patient(VisitStore *store, int patient_id,
                            int out_ids[], int max_out);
Visit *visit_find_by_id(VisitStore *store, int id);
int    visit_save_to_file(VisitStore *store, const char *path);
int    visit_load_from_file(VisitStore *store, const char *path);
void   visit_print(const Visit *v);
void   visit_print_history(VisitStore *store, int patient_id);

#endif
