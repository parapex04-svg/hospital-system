#ifndef ANALYTICS_H
#define ANALYTICS_H

/* ============================================================
 * analytics.h — Dashboard statistics and analytics
 * ============================================================ */

#include "patient.h"
#include "visit.h"
#include "appointment.h"

#define MAX_DISEASES    50
#define MAX_DOCTORS     20

/* Disease frequency entry */
typedef struct {
    char disease[MAX_DISEASE_LEN];
    int  count;
} DiseaseFreq;

/* Doctor workload entry */
typedef struct {
    char doctor[MAX_DOCTOR_LEN];
    int  patient_count;
    int  appointment_count;
    int  visit_count;
} DoctorWorkload;

/* Dashboard summary snapshot */
typedef struct {
    int total_patients;
    int active_patients;
    int total_visits;
    int total_appointments;
    int appointments_today;
    int high_severity_count;
    int medium_severity_count;
    int low_severity_count;
    DiseaseFreq   top_diseases[MAX_DISEASES];
    int           disease_count;
    DoctorWorkload workloads[MAX_DOCTORS];
    int            doctor_count;
} DashboardStats;

/* ── Function declarations ──────────────────────────────────── */
void analytics_compute(DashboardStats    *stats,
                       PatientStore      *patients,
                       VisitStore        *visits,
                       AppointmentStore  *appts,
                       const char        *today);

void analytics_print_dashboard(const DashboardStats *stats);
void analytics_disease_frequency(PatientStore *store,
                                 DiseaseFreq out[], int *out_count);
void analytics_doctor_workload(PatientStore *patients,
                               AppointmentStore *appts,
                               VisitStore *visits,
                               DoctorWorkload out[], int *out_count);

/* Triage queue — returns active patient IDs sorted by severity desc */
int  analytics_triage_queue(PatientStore *store, int out_ids[], int max_out);

/* Returns a suggested doctor (least loaded) */
const char *analytics_suggest_doctor(DoctorWorkload *wl, int count);

#endif
