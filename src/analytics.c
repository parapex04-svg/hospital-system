/* ============================================================
 * analytics.c — Dashboard statistics, triage queue, workload
 * Hospital Patient Registration & Follow-up Tracker
 * ============================================================ */

#include "../include/analytics.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Compute full dashboard stats ────────────────────────────── */
void analytics_compute(DashboardStats    *stats,
                       PatientStore      *patients,
                       VisitStore        *visits,
                       AppointmentStore  *appts,
                       const char        *today) {
    memset(stats, 0, sizeof(DashboardStats));

    stats->total_patients = patients->count;
    stats->total_visits   = visits->count;
    stats->total_appointments = appts->count;

    /* Count active patients and severity breakdown */
    for (int i = 0; i < patients->count; i++) {
        Patient *p = &patients->records[i];
        if (!p->is_active) continue;
        stats->active_patients++;
        if      (p->severity == SEVERITY_HIGH)   stats->high_severity_count++;
        else if (p->severity == SEVERITY_MEDIUM) stats->medium_severity_count++;
        else                                     stats->low_severity_count++;
    }

    /* Count today's appointments */
    if (today) {
        for (int i = 0; i < appts->count; i++) {
            if (strcmp(appts->records[i].date, today) == 0 &&
                appts->records[i].status == APPT_SCHEDULED) {
                stats->appointments_today++;
            }
        }
    }

    /* Disease frequency */
    analytics_disease_frequency(patients,
                                stats->top_diseases,
                                &stats->disease_count);

    /* Doctor workload */
    analytics_doctor_workload(patients, appts, visits,
                              stats->workloads, &stats->doctor_count);
}

/* ── Disease frequency analyser (counting sort style) ────────── */
void analytics_disease_frequency(PatientStore *store,
                                 DiseaseFreq out[], int *out_count) {
    int n = 0;
    for (int i = 0; i < store->count; i++) {
        if (!store->records[i].is_active) continue;
        const char *d = store->records[i].disease;
        if (!d[0]) continue;

        /* Find existing entry */
        int found = 0;
        for (int j = 0; j < n; j++) {
            if (strcmp(out[j].disease, d) == 0) {
                out[j].count++;
                found = 1;
                break;
            }
        }
        if (!found && n < MAX_DISEASES) {
            strncpy(out[n].disease, d, MAX_DISEASE_LEN - 1);
            out[n].count = 1;
            n++;
        }
    }

    /* Bubble sort descending by count */
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (out[j].count < out[j+1].count) {
                DiseaseFreq tmp = out[j];
                out[j]   = out[j+1];
                out[j+1] = tmp;
            }
        }
    }
    *out_count = n;
}

/* ── Doctor workload balancer ────────────────────────────────── */
void analytics_doctor_workload(PatientStore *patients,
                               AppointmentStore *appts,
                               VisitStore *visits,
                               DoctorWorkload out[], int *out_count) {
    int n = 0;

    /* Count patients per doctor */
    for (int i = 0; i < patients->count; i++) {
        if (!patients->records[i].is_active) continue;
        const char *doc = patients->records[i].doctor;
        if (!doc[0]) continue;

        int found = 0;
        for (int j = 0; j < n; j++) {
            if (strcmp(out[j].doctor, doc) == 0) {
                out[j].patient_count++;
                found = 1;
                break;
            }
        }
        if (!found && n < MAX_DOCTORS) {
            strncpy(out[n].doctor, doc, MAX_DOCTOR_LEN - 1);
            out[n].patient_count     = 1;
            out[n].appointment_count = 0;
            out[n].visit_count       = 0;
            n++;
        }
    }

    /* Count appointments per doctor */
    for (int i = 0; i < appts->count; i++) {
        const char *doc = appts->records[i].doctor;
        for (int j = 0; j < n; j++) {
            if (strcmp(out[j].doctor, doc) == 0) {
                out[j].appointment_count++;
                break;
            }
        }
    }

    /* Count visits per doctor */
    for (int i = 0; i < visits->count; i++) {
        const char *doc = visits->records[i].doctor;
        for (int j = 0; j < n; j++) {
            if (strcmp(out[j].doctor, doc) == 0) {
                out[j].visit_count++;
                break;
            }
        }
    }

    *out_count = n;
}

/* ── Triage queue: sort active patients by severity desc ─────── */
int analytics_triage_queue(PatientStore *store, int out_ids[], int max_out) {
    /* Collect active patient indices */
    int tmp[MAX_PATIENTS];
    int n = 0;
    for (int i = 0; i < store->count && n < max_out; i++) {
        if (store->records[i].is_active) tmp[n++] = i;
    }

    /* Insertion sort by severity descending */
    for (int i = 1; i < n; i++) {
        int key = tmp[i];
        int j   = i - 1;
        while (j >= 0 && store->records[tmp[j]].severity
                       < store->records[key].severity) {
            tmp[j+1] = tmp[j];
            j--;
        }
        tmp[j+1] = key;
    }

    for (int i = 0; i < n; i++) out_ids[i] = store->records[tmp[i]].id;
    return n;
}

/* ── Suggest least-loaded doctor ─────────────────────────────── */
const char *analytics_suggest_doctor(DoctorWorkload *wl, int count) {
    if (count == 0) return "N/A";
    int min_load = wl[0].patient_count;
    int min_idx  = 0;
    for (int i = 1; i < count; i++) {
        if (wl[i].patient_count < min_load) {
            min_load = wl[i].patient_count;
            min_idx  = i;
        }
    }
    return wl[min_idx].doctor;
}

/* ── Print formatted dashboard ───────────────────────────────── */
void analytics_print_dashboard(const DashboardStats *s) {
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║         HOSPITAL DASHBOARD — STATISTICS          ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Total Patients:  %-6d  Active: %-6d          ║\n",
           s->total_patients, s->active_patients);
    printf("║  Total Visits:    %-6d  Appointments: %-6d  ║\n",
           s->total_visits, s->total_appointments);
    printf("║  Today's Appts:   %-6d                          ║\n",
           s->appointments_today);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Severity Breakdown:                             ║\n");
    printf("║    HIGH (Critical): %-4d  MEDIUM: %-4d  LOW: %-4d║\n",
           s->high_severity_count, s->medium_severity_count, s->low_severity_count);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Top Diseases:                                   ║\n");
    int top = s->disease_count < 5 ? s->disease_count : 5;
    for (int i = 0; i < top; i++) {
        printf("║    %-3d. %-30s  %3d cases  ║\n",
               i+1, s->top_diseases[i].disease, s->top_diseases[i].count);
    }
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Doctor Workload:                                ║\n");
    for (int i = 0; i < s->doctor_count; i++) {
        printf("║    %-20s  Pts:%-3d  Appts:%-3d  Vis:%-3d ║\n",
               s->workloads[i].doctor,
               s->workloads[i].patient_count,
               s->workloads[i].appointment_count,
               s->workloads[i].visit_count);
    }
    printf("╚══════════════════════════════════════════════════╝\n");
}
