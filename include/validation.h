#ifndef VALIDATION_H
#define VALIDATION_H

/* ============================================================
 * validation.h — Input validation function declarations
 * ============================================================ */

#include "patient.h"

/* Return codes */
#define VALID           0
#define ERR_EMPTY       -1
#define ERR_FORMAT      -2
#define ERR_RANGE       -3
#define ERR_DUPLICATE   -4
#define ERR_LENGTH      -5

/* ── Function declarations ──────────────────────────────────── */
int  val_non_empty(const char *s);
int  val_phone(const char *phone);           /* exactly 10 digits    */
int  val_age(int age);                       /* 1 – 120              */
int  val_severity(int severity);             /* 1 / 2 / 3 only       */
int  val_gender(char g);                     /* M / F / O            */
int  val_date(const char *date);             /* YYYY-MM-DD           */
int  val_time_slot(const char *t);           /* HH:MM  00:00-23:59   */
int  val_duplicate_patient_id(PatientStore *store, int id);
int  val_name_length(const char *name);
int  val_patient_fields(PatientStore *store, Patient *p, int is_new);

/* Human-readable error message */
const char *val_error_msg(int code);

#endif
