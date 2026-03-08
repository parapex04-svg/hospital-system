#ifndef PATIENT_H
#define PATIENT_H

/* ============================================================
 * patient.h — Patient data structures and function declarations
 * Hospital Patient Registration & Follow-up Tracker
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Constants ─────────────────────────────────────────────── */
#define MAX_PATIENTS        500
#define MAX_NAME_LEN        64
#define MAX_PHONE_LEN       16
#define MAX_ADDRESS_LEN     128
#define MAX_DISEASE_LEN     64
#define MAX_DOCTOR_LEN      64
#define MAX_DATE_LEN        16
#define MAX_NOTES_LEN       256
#define PATIENT_FILE        "data/patients.txt"

/* ── Severity / Triage levels ───────────────────────────────── */
#define SEVERITY_LOW        1
#define SEVERITY_MEDIUM     2
#define SEVERITY_HIGH       3

/* ── Gender codes ───────────────────────────────────────────── */
#define GENDER_MALE         'M'
#define GENDER_FEMALE       'F'
#define GENDER_OTHER        'O'

/* ============================================================
 * Core patient record
 * ============================================================ */
typedef struct {
    int    id;
    char   name[MAX_NAME_LEN];
    int    age;
    char   gender;
    char   phone[MAX_PHONE_LEN];
    char   address[MAX_ADDRESS_LEN];
    char   disease[MAX_DISEASE_LEN];
    int    severity;
    char   doctor[MAX_DOCTOR_LEN];
    char   reg_date[MAX_DATE_LEN];
    int    is_active;
    int    visit_count;
} Patient;

typedef struct {
    Patient records[MAX_PATIENTS];
    int     count;
    int     next_id;
} PatientStore;

/* ── Function declarations ──────────────────────────────────── */
void    patient_init_store(PatientStore *store);
int     patient_add(PatientStore *store, Patient *p);
Patient *patient_find_by_id(PatientStore *store, int id);
int     patient_find_index_by_id(PatientStore *store, int id);
int     patient_search_by_name(PatientStore *store, const char *name,
                               int results[], int max_results);
int     patient_update(PatientStore *store, int id, Patient *updated);
int     patient_delete(PatientStore *store, int id);
int     patient_save_to_file(PatientStore *store, const char *path);
int     patient_load_from_file(PatientStore *store, const char *path);
void    patient_print(const Patient *p);
void    patient_print_all(const PatientStore *store);
int     patient_generate_id(PatientStore *store);
int     patient_count_active(const PatientStore *store);

#endif
