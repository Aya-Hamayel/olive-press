#include "common.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) ++s;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = '\0';
    return s;
}

static int parse_int(const char *s, int *out) {
    char *end;
    errno = 0;
    long n = strtol(s, &end, 10);
    while (isspace((unsigned char)*end)) ++end;
    if (errno || end == s || *end || n < 0 || n > 1000000) return -1;
    *out = (int)n;
    return 0;
}

static int parse_float(const char *s, float *out) {
    char *end;
    errno = 0;
    float n = strtof(s, &end);
    while (isspace((unsigned char)*end)) ++end;
    if (errno || end == s || *end || !isfinite(n)) return -1;
    *out = n;
    return 0;
}

static int parse_range(char *s, float *low, float *high) {
    char *comma = strchr(s, ',');
    if (!comma) return -1;
    *comma = '\0';
    return parse_float(trim(s), low) || parse_float(trim(comma + 1), high) ? -1 : 0;
}

int load_config(const char *path, config_t *cfg) {
    if (!path || !cfg) return -1;
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }
    config_t tmp = {0};
    unsigned seen = 0;
    char line[512];
    int line_no = 0;
    while (fgets(line, sizeof line, f)) {
        ++line_no;
        if (!strchr(line, '\n') && !feof(f)) {
            fprintf(stderr, "Config line %d too long\n", line_no);
            fclose(f); return -1;
        }
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        char *s = trim(line);
        if (!*s) continue;
        char *eq = strchr(s, '=');
        if (!eq) goto invalid;
        *eq = '\0';
        char *key = trim(s), *val = trim(eq + 1);
        int index = -1;
        int bit = -1;
        int result = -1;
        if (!strcmp(key,"num_batches")) {bit=0; result=parse_int(val,&tmp.num_batches);}
        else if (!strcmp(key,"batch_size_kg")) {bit=1; result=parse_float(val,&tmp.batch_size_kg);}
        else if (!strcmp(key,"container_kg")) {bit=2; result=parse_float(val,&tmp.container_kg);}
        else if (!strcmp(key,"reception_time")) index=RECEPTION;
        else if (!strcmp(key,"cleaning_time")) index=CLEANING;
        else if (!strcmp(key,"washing_time")) index=WASHING;
        else if (!strcmp(key,"crushing_time")) index=CRUSHING;
        else if (!strcmp(key,"malaxation_time")) index=MALAXATION;
        else if (!strcmp(key,"pressing_time")) index=PRESSING;
        else if (!strcmp(key,"temp_range")) {bit=9; result=parse_range(val,&tmp.temp_min,&tmp.temp_max);}
        else if (!strcmp(key,"pressure_range")) {bit=10; result=parse_range(val,&tmp.press_min,&tmp.press_max);}
        else if (!strcmp(key,"fault_probability")) {bit=11; result=parse_float(val,&tmp.fault_prob);}
        else if (!strcmp(key,"log_file")) {
            bit=12;
            if (*val && strlen(val)<sizeof tmp.log_file) {
                strcpy(tmp.log_file,val); result=0;
            }
        } else goto invalid;
        if (index >= 0) {
            bit=3+index;
            result=parse_int(val,&tmp.stage_time[index]);
        }
        if (result || (seen & (1u << bit))) goto invalid;
        seen |= 1u << bit;
        continue;
invalid:
        fprintf(stderr,"Invalid or duplicate config entry at line %d\n",line_no);
        fclose(f); return -1;
    }
    if (ferror(f)) { perror("read config"); fclose(f); return -1; }
    fclose(f);
    if (seen != ((1u << 13)-1) || tmp.num_batches <= 0 ||
        tmp.batch_size_kg <= 0 || tmp.container_kg <= 0 ||
        (double)tmp.num_batches * tmp.batch_size_kg > tmp.container_kg + 0.0001 ||
        tmp.temp_min >= tmp.temp_max || tmp.press_min >= tmp.press_max ||
        tmp.fault_prob < 0 || tmp.fault_prob > 1) {
        fprintf(stderr,"Missing or inconsistent config values\n"); return -1;
    }
    *cfg=tmp;
    return 0;
}
