/* ============================================================
 * validation.c — Reusable input validation functions
 * Hospital Patient Registration & Follow-up Tracker
 * ============================================================ */

#include "../include/validation.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

/* ── Error messages ─────────────────────────────────────────── */
const char *val_error_msg(int code) {
    switch (code) {
        case VALID:           return "OK";
        case ERR_EMPTY:       return "Field cannot be empty";
        case ERR_FORMAT:      return "Invalid format";
        case ERR_RANGE:       return "Value out of allowed range";
        case ERR_DUPLICATE:   return "Duplicate entry already exists";
        case ERR_LENGTH:      return "Input exceeds maximum length";
        default:              return "Unknown validation error";
    }
}

/* ── Check that a string is not empty or whitespace-only ─────── */
int val_non_empty(const char *s) {
    if (!s || s[0] == '\0') return ERR_EMPTY;
    for (int i = 0; s[i]; i++) {
        if (!isspace((unsigned char)s[i])) return VALID;
    }
    return ERR_EMPTY;
}

/* ── Phone: exactly 10 numeric digits ───────────────────────── */
int val_phone(const char *phone) {
    if (!phone) return ERR_EMPTY;
    int len = 0;
    for (int i = 0; phone[i]; i++) {
        if (!isdigit((unsigned char)phone[i])) return ERR_FORMAT;
        len++;
    }
    if (len != 10) return ERR_FORMAT;
    return VALID;
}

/* ── Age: 1 – 120 ────────────────────────────────────────────── */
int val_age(int age) {
    if (age < 1 || age > 120) return ERR_RANGE;
    return VALID;
}

/* ── Severity: 1 / 2 / 3 ─────────────────────────────────────── */
int val_severity(int severity) {
    if (severity < 1 || severity > 3) return ERR_RANGE;
    return VALID;
}

/* ── Gender: M / F / O (case insensitive) ────────────────────── */
int val_gender(char g) {
    char ug = (char)toupper((unsigned char)g);
    if (ug == 'M' || ug == 'F' || ug == 'O') return VALID;
    return ERR_FORMAT;
}

/* ── Date: YYYY-MM-DD ─────────────────────────────────────────── */
int val_date(const char *date) {
    if (!date || strlen(date) != 10) return ERR_FORMAT;
    /* Pattern: NNNN-NN-NN */
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (date[i] != '-') return ERR_FORMAT;
        } else {
            if (!isdigit((unsigned char)date[i])) return ERR_FORMAT;
        }
    }
    int year  = atoi(date);
    int month = atoi(date + 5);
    int day   = atoi(date + 8);
    if (year < 1900 || year > 2100) return ERR_RANGE;
    if (month < 1   || month > 12)  return ERR_RANGE;
    if (day < 1     || day > 31)    return ERR_RANGE;
    return VALID;
}

/* ── Time slot: HH:MM ────────────────────────────────────────── */
int val_time_slot(const char *t) {
    if (!t || strlen(t) != 5) return ERR_FORMAT;
    if (t[2] != ':') return ERR_FORMAT;
    for (int i = 0; i < 5; i++) {
        if (i == 2) continue;
        if (!isdigit((unsigned char)t[i])) return ERR_FORMAT;
    }
    int hour = atoi(t);
    int min  = atoi(t + 3);
    if (hour < 0 || hour > 23) return ERR_RANGE;
    if (min  < 0 || min  > 59) return ERR_RANGE;
    return VALID;
}

/* ── Duplicate patient ID check ─────────────────────────────── */
int val_duplicate_patient_id(PatientStore *store, int id) {
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].id == id && store->records[i].is_active) {
            return ERR_DUPLICATE;
        }
    }
    return VALID;
}

/* ── Name length check ───────────────────────────────────────── */
int val_name_length(const char *name) {
    if (!name) return ERR_EMPTY;
    if (strlen(name) >= MAX_NAME_LEN) return ERR_LENGTH;
    return VALID;
}

/* ── Composite patient field validation ─────────────────────── */
int val_patient_fields(PatientStore *store, Patient *p, int is_new) {
    int rc;

    rc = val_non_empty(p->name);
    if (rc != VALID) { fprintf(stderr, "Name: %s\n", val_error_msg(rc)); return rc; }

    rc = val_name_length(p->name);
    if (rc != VALID) { fprintf(stderr, "Name: %s\n", val_error_msg(rc)); return rc; }

    rc = val_age(p->age);
    if (rc != VALID) { fprintf(stderr, "Age: %s (must be 1-120)\n", val_error_msg(rc)); return rc; }

    rc = val_gender(p->gender);
    if (rc != VALID) { fprintf(stderr, "Gender: %s (M/F/O)\n", val_error_msg(rc)); return rc; }

    rc = val_phone(p->phone);
    if (rc != VALID) { fprintf(stderr, "Phone: %s (10 digits)\n", val_error_msg(rc)); return rc; }

    rc = val_severity(p->severity);
    if (rc != VALID) { fprintf(stderr, "Severity: %s (1/2/3)\n", val_error_msg(rc)); return rc; }

    rc = val_non_empty(p->doctor);
    if (rc != VALID) { fprintf(stderr, "Doctor: %s\n", val_error_msg(rc)); return rc; }

    rc = val_date(p->reg_date);
    if (rc != VALID) { fprintf(stderr, "Date: %s (YYYY-MM-DD)\n", val_error_msg(rc)); return rc; }

    if (is_new) {
        rc = val_duplicate_patient_id(store, p->id);
        if (rc != VALID) { fprintf(stderr, "ID: %s\n", val_error_msg(rc)); return rc; }
    }

    return VALID;
}
