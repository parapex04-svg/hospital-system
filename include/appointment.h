#ifndef APPOINTMENT_H
#define APPOINTMENT_H

/* ============================================================
 * appointment.h — Appointment scheduling structures
 * ============================================================ */

#include "patient.h"

#define MAX_APPOINTMENTS    300
#define APPT_FILE           "data/appointments.txt"

#define APPT_SCHEDULED      0
#define APPT_COMPLETED      1
#define APPT_CANCELLED      2
#define APPT_NO_SHOW        3

typedef struct {
    int  appt_id;
    int  patient_id;
    char patient_name[MAX_NAME_LEN];
    char doctor[MAX_DOCTOR_LEN];
    char date[MAX_DATE_LEN];      /* YYYY-MM-DD */
    char time_slot[8];            /* HH:MM      */
    char reason[MAX_NOTES_LEN];
    int  status;                  /* APPT_* constants */
    int  duration_mins;           /* default 30 */
} Appointment;

typedef struct {
    Appointment records[MAX_APPOINTMENTS];
    int         count;
    int         next_id;
} AppointmentStore;

/* ── Function declarations ──────────────────────────────────── */
void  appt_init_store(AppointmentStore *store);
int   appt_add(AppointmentStore *store, Appointment *a);
int   appt_conflict_exists(AppointmentStore *store,
                           const char *doctor,
                           const char *date,
                           const char *time_slot,
                           int         duration_mins,
                           int         exclude_id);
int   appt_cancel(AppointmentStore *store, int appt_id);
int   appt_complete(AppointmentStore *store, int appt_id);
int   appt_get_by_patient(AppointmentStore *store, int patient_id,
                          int out_ids[], int max_out);
int   appt_get_by_doctor(AppointmentStore *store, const char *doctor,
                         int out_ids[], int max_out);
int   appt_save_to_file(AppointmentStore *store, const char *path);
int   appt_load_from_file(AppointmentStore *store, const char *path);
void  appt_print(const Appointment *a);
Appointment *appt_find_by_id(AppointmentStore *store, int id);

#endif
