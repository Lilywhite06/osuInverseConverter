#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

/* ---------- Data structures ---------- */

typedef struct {
    int x, y, time, type, hit_sound;
    char *extra;
    int col;
} HitObject;

typedef struct {
    double time;
    double beat_length;
} TimingPoint;

/* ---------- Global dynamic arrays ---------- */

static char **lines = NULL;
static size_t line_count = 0;

static HitObject *objects = NULL;
static size_t object_count = 0;

static TimingPoint *timing_points = NULL;
static size_t timing_count = 0;

static int column_count = 4;   /* default 4K */

/* ---------- Helper: memory allocation with error check ---------- */

static void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Error: out of memory (tried to allocate %zu bytes).\n", size);
        exit(1);
    }
    return ptr;
}

static void *xrealloc(void *ptr, size_t new_size)
{
    void *new_ptr = realloc(ptr, new_size);
    if (!new_ptr) {
        fprintf(stderr, "Error: out of memory (tried to reallocate %zu bytes).\n", new_size);
        exit(1);
    }
    return new_ptr;
}

/* ---------- Helper: read one line from file (standard C, no getline) ---------- */

static char *read_line(FILE *fp)
{
    size_t capacity = 256;
    size_t length = 0;
    char *buffer = xmalloc(capacity);
    int ch;

    while ((ch = fgetc(fp)) != EOF && ch != '\n') {
        if (ch == '\r') continue;
        if (length + 1 >= capacity) {
            capacity *= 2;
            buffer = xrealloc(buffer, capacity);
        }
        buffer[length++] = (char)ch;
    }

    if (ch == EOF && length == 0) {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

/* ---------- File reading ---------- */

static void read_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s': %s\n", path, strerror(errno));
        exit(1);
    }

    char *line;
    while ((line = read_line(fp)) != NULL) {
        lines = xrealloc(lines, (line_count + 1) * sizeof(char *));
        lines[line_count] = line;
        line_count++;
    }
    fclose(fp);

    if (line_count == 0) {
        fprintf(stderr, "Error: input file is empty.\n");
        exit(1);
    }
}

/* ---------- Parsing functions ---------- */

static void parse_circle_size(void)
{
    int in_difficulty = 0;
    for (size_t i = 0; i < line_count; i++) {
        const char *line = lines[i];
        if (strcmp(line, "[Difficulty]") == 0) {
            in_difficulty = 1;
            continue;
        }
        if (in_difficulty && line[0] == '[') break;
        if (in_difficulty && strncmp(line, "CircleSize:", 11) == 0) {
            int val = atoi(line + 11);
            if (val >= 1 && val <= 18) {
                column_count = val;
            } else {
                fprintf(stderr, "Warning: suspicious CircleSize value (%d), using default 4.\n", val);
            }
            break;
        }
    }
}

static void parse_timing_points(void)
{
    int in_timing = 0;
    int red_line_count = 0;
    for (size_t i = 0; i < line_count; i++) {
        const char *line = lines[i];
        if (strcmp(line, "[TimingPoints]") == 0) {
            in_timing = 1;
            continue;
        }
        if (in_timing && line[0] == '[') break;
        if (in_timing && line[0] != '\0' && line[0] != '/') {
            double time, beat_length;
            int uninherited;
            if (sscanf(line, "%lf,%lf,%*d,%*d,%*d,%*d,%d",
                       &time, &beat_length, &uninherited) == 3) {
                if (uninherited == 1) {
                    if (beat_length <= 0) {
                        fprintf(stderr, "Warning: invalid beat_length %.2f at line %zu, ignored.\n",
                                beat_length, i + 1);
                        continue;
                    }
                    timing_points = xrealloc(timing_points,
                                             (timing_count + 1) * sizeof(TimingPoint));
                    timing_points[timing_count].time = time;
                    timing_points[timing_count].beat_length = beat_length;
                    timing_count++;
                    red_line_count++;
                }
            } else {
                fprintf(stderr, "Warning: malformed timing point line %zu: %s\n", i + 1, line);
            }
        }
    }
    if (red_line_count == 0) {
        fprintf(stderr, "Warning: no red timing points found, using default 120 BPM (500 ms).\n");
    }
}

static double get_beat_length(int time)
{
    double beat_length = 500.0;
    for (size_t i = 0; i < timing_count; i++) {
        if (timing_points[i].time <= time) {
            beat_length = timing_points[i].beat_length;
        } else {
            break;
        }
    }
    return beat_length;
}

static void parse_hit_objects(void)
{
    int in_hit = 0;
    for (size_t i = 0; i < line_count; i++) {
        const char *line = lines[i];
        if (strcmp(line, "[HitObjects]") == 0) {
            in_hit = 1;
            continue;
        }
        if (in_hit && line[0] == '[') break;
        if (in_hit && line[0] != '\0' && line[0] != '/') {
            HitObject obj;
            char extra[512];
            int n = sscanf(line, "%d,%d,%d,%d,%d,%511[^\n]",
                           &obj.x, &obj.y, &obj.time,
                           &obj.type, &obj.hit_sound, extra);
            if (n < 5) {
                fprintf(stderr, "Warning: cannot parse hit object line: %s\n", line);
                continue;
            }
            if (n == 5) {
                strcpy(extra, "0:0:0:0:");
            }
            obj.extra = xmalloc(strlen(extra) + 1);
            strcpy(obj.extra, extra);
            obj.col = (int)((obj.x * column_count) / 512);
            if (obj.col < 0) obj.col = 0;
            if (obj.col >= column_count) obj.col = column_count - 1;
            objects = xrealloc(objects, (object_count + 1) * sizeof(HitObject));
            objects[object_count++] = obj;
        }
    }
}

/* ---------- Sorting comparators ---------- */

static int cmp_col_time(const void *a, const void *b)
{
    const HitObject *oa = (const HitObject *)a;
    const HitObject *ob = (const HitObject *)b;
    if (oa->col != ob->col) return oa->col - ob->col;
    return oa->time - ob->time;
}

static int cmp_time_x(const void *a, const void *b)
{
    const HitObject *oa = (const HitObject *)a;
    const HitObject *ob = (const HitObject *)b;
    if (oa->time != ob->time) return oa->time - ob->time;
    return oa->x - ob->x;
}

/* ---------- Helper: clone a HitObject ---------- */

static HitObject clone_object(const HitObject *src)
{
    HitObject dst;
    dst.x = src->x;
    dst.y = src->y;
    dst.time = src->time;
    dst.type = src->type;
    dst.hit_sound = src->hit_sound;
    dst.col = src->col;
    dst.extra = xmalloc(strlen(src->extra) + 1);
    strcpy(dst.extra, src->extra);
    return dst;
}

/* ---------- Mode 2: delete original LNs and reconstruct ---------- */

static HitObject *convert_delete_mode(size_t *new_count, int gap_divisor)
{
    /* Phase 1: Process original LNs */
    HitObject *phase1_objects = NULL;
    size_t phase1_count = 0;

    size_t col_start = 0;
    while (col_start < object_count) {
        size_t col_end = col_start;
        while (col_end < object_count && objects[col_end].col == objects[col_start].col) {
            col_end++;
        }

        HitObject *col_list = NULL;
        size_t col_list_count = 0;

        for (size_t i = col_start; i < col_end; i++) {
            HitObject *obj = &objects[i];
            if (obj->type & 128) {
                HitObject *prev_obj = (col_list_count > 0) ? &col_list[col_list_count - 1] : NULL;

                if (prev_obj) {
                    if (prev_obj->type & 128) {
                        char *colon = strchr(prev_obj->extra, ':');
                        char new_extra[512];
                        snprintf(new_extra, sizeof(new_extra), "%d%s",
                                 obj->time, colon ? colon : ":0:0:0:0:");
                        free(prev_obj->extra);
                        prev_obj->extra = xmalloc(strlen(new_extra) + 1);
                        strcpy(prev_obj->extra, new_extra);
                    } else if (prev_obj->type & 1) {
                        if (obj->time - prev_obj->time >= 10) {
                            prev_obj->type = 128;
                            char new_extra[512];
                            snprintf(new_extra, sizeof(new_extra), "%d:0:0:0:0:", obj->time);
                            free(prev_obj->extra);
                            prev_obj->extra = xmalloc(strlen(new_extra) + 1);
                            strcpy(prev_obj->extra, new_extra);
                        }
                    }
                }

                int ln_end = atoi(obj->extra);
                HitObject add_obj;
                add_obj.x = obj->x;
                add_obj.y = obj->y;
                add_obj.time = ln_end;
                add_obj.hit_sound = obj->hit_sound;
                add_obj.col = obj->col;

                HitObject *next_obj = NULL;
                for (size_t j = i + 1; j < col_end; j++) {
                    if (objects[j].time > obj->time) {
                        next_obj = &objects[j];
                        break;
                    }
                }

                if (next_obj) {
                    double beat_length = get_beat_length(ln_end);
                    double gap = beat_length / gap_divisor;
                    double desired_end_d = (double)next_obj->time - gap;
                    if (desired_end_d - ln_end >= 10) {
                        add_obj.type = 128;
                        char extra[128];
                        snprintf(extra, sizeof(extra), "%d:0:0:0:0:", (int)round(desired_end_d));
                        add_obj.extra = xmalloc(strlen(extra) + 1);
                        strcpy(add_obj.extra, extra);
                    } else {
                        add_obj.type = 1;
                        add_obj.extra = xmalloc(strlen("0:0:0:0:") + 1);
                        strcpy(add_obj.extra, "0:0:0:0:");
                    }
                } else {
                    add_obj.type = 1;
                    add_obj.extra = xmalloc(strlen("0:0:0:0:") + 1);
                    strcpy(add_obj.extra, "0:0:0:0:");
                }

                col_list = xrealloc(col_list, (col_list_count + 1) * sizeof(HitObject));
                col_list[col_list_count++] = add_obj;
            } else {
                col_list = xrealloc(col_list, (col_list_count + 1) * sizeof(HitObject));
                col_list[col_list_count] = clone_object(obj);
                col_list_count++;
            }
        }

        phase1_objects = xrealloc(phase1_objects,
                                  (phase1_count + col_list_count) * sizeof(HitObject));
        for (size_t i = 0; i < col_list_count; i++) {
            phase1_objects[phase1_count + i] = col_list[i];
        }
        phase1_count += col_list_count;
        free(col_list);
        col_start = col_end;
    }

    /* Sort phase1_objects by column and time to ensure correct order */
    qsort(phase1_objects, phase1_count, sizeof(HitObject), cmp_col_time);

    /* Phase 2: Rice to LN (mode 0) on phase1_objects */
    HitObject *final_objects = NULL;
    size_t final_count = 0;

    for (size_t i = 0; i < phase1_count; i++) {
        HitObject *obj = &phase1_objects[i];
        int is_ln = (obj->type & 128) != 0;
        int is_rice = (obj->type & 1) != 0 && !is_ln;

        if (is_rice) {
            HitObject *next_obj = NULL;
            for (size_t j = i + 1; j < phase1_count; j++) {
                if (phase1_objects[j].col == obj->col) {
                    next_obj = &phase1_objects[j];
                    break;
                }
            }

            int convert = 0;
            int end_time = 0;
            if (next_obj) {
                double beat_length = get_beat_length(obj->time);
                double gap = beat_length / gap_divisor;
                double next_time = (double)next_obj->time;
                double end_time_d = next_time - gap;
                if (next_time - obj->time >= gap - 0.001) {
                    end_time = (int)round(end_time_d);
                    if (end_time - obj->time >= 10) convert = 1;
                }
            }

            final_objects = xrealloc(final_objects, (final_count + 1) * sizeof(HitObject));
            HitObject *dst = &final_objects[final_count];
            dst->x = obj->x;
            dst->y = obj->y;
            dst->time = obj->time;
            dst->hit_sound = obj->hit_sound;
            dst->col = obj->col;
            if (convert) {
                dst->type = 128;
                char extra[128];
                snprintf(extra, sizeof(extra), "%d:0:0:0:0:", end_time);
                dst->extra = xmalloc(strlen(extra) + 1);
                strcpy(dst->extra, extra);
            } else {
                dst->type = 1;
                dst->extra = xmalloc(strlen("0:0:0:0:") + 1);
                strcpy(dst->extra, "0:0:0:0:");
            }
            final_count++;
        } else {
            final_objects = xrealloc(final_objects, (final_count + 1) * sizeof(HitObject));
            final_objects[final_count] = clone_object(obj);
            final_count++;
        }
    }

    for (size_t i = 0; i < phase1_count; i++) free(phase1_objects[i].extra);
    free(phase1_objects);

    qsort(final_objects, final_count, sizeof(HitObject), cmp_time_x);
    *new_count = final_count;
    return final_objects;
}

/* ---------- Mode 3: LN to Rice (rc) ---------- */

static HitObject *convert_ln_to_rice(size_t *new_count)
{
    HitObject *new_objects = NULL;
    size_t count = 0;

    for (size_t i = 0; i < object_count; i++) {
        HitObject *obj = &objects[i];
        int is_ln = (obj->type & 128) != 0;
        if (is_ln) {
            new_objects = xrealloc(new_objects, (count + 1) * sizeof(HitObject));
            HitObject *dst = &new_objects[count];
            dst->x = obj->x;
            dst->y = obj->y;
            dst->time = obj->time;
            dst->type = 1;
            dst->hit_sound = obj->hit_sound;
            dst->extra = xmalloc(strlen("0:0:0:0:") + 1);
            strcpy(dst->extra, "0:0:0:0:");
            dst->col = obj->col;
            count++;
        } else {
            new_objects = xrealloc(new_objects, (count + 1) * sizeof(HitObject));
            new_objects[count] = clone_object(obj);
            count++;
        }
    }

    qsort(new_objects, count, sizeof(HitObject), cmp_time_x);
    *new_count = count;
    return new_objects;
}

/* ---------- Object conversion (dispatcher for modes 0,1,2,3) ---------- */

static HitObject *convert_objects(size_t *new_count, int gap_divisor, int process_ln)
{
    /* Defensive check: gap only meaningful for modes 0,1,2 */
    if (gap_divisor <= 0 && process_ln != 3) {
        fprintf(stderr, "Warning: invalid gap divisor, using default 4.\n");
        gap_divisor = 4;
    }
    if (process_ln < 0 || process_ln > 3) {
        fprintf(stderr, "Warning: invalid LN mode, using default 0.\n");
        process_ln = 0;
    }

    qsort(objects, object_count, sizeof(HitObject), cmp_col_time);

    /* Mode 0 and 1: keep or extend LNs, and convert rice notes */
    if (process_ln == 0 || process_ln == 1) {
        HitObject *new_objects = NULL;
        *new_count = 0;

        for (size_t i = 0; i < object_count; i++) {
            HitObject *obj = &objects[i];
            int is_ln = (obj->type & 128) != 0;
            int is_rice = (obj->type & 1) != 0 && !is_ln;

            if (is_ln) {
                HitObject *next_obj = NULL;
                if (i + 1 < object_count && objects[i + 1].col == obj->col) {
                    next_obj = &objects[i + 1];
                }
                char new_extra[512];
                strcpy(new_extra, obj->extra);

                if (process_ln == 1 && next_obj) {
                    int old_end = atoi(obj->extra);
                    double beat_length = get_beat_length(obj->time);
                    double gap = beat_length / gap_divisor;
                    int desired_end = (int)round((double)next_obj->time - gap);
                    if (desired_end > old_end && desired_end > obj->time) {
                        char *colon = strchr(obj->extra, ':');
                        if (colon) {
                            snprintf(new_extra, sizeof(new_extra), "%d%s", desired_end, colon);
                        } else {
                            snprintf(new_extra, sizeof(new_extra), "%d:0:0:0:0:", desired_end);
                        }
                    }
                }

                new_objects = xrealloc(new_objects, (*new_count + 1) * sizeof(HitObject));
                HitObject *dst = &new_objects[*new_count];
                dst->x = obj->x;
                dst->y = obj->y;
                dst->time = obj->time;
                dst->type = obj->type;
                dst->hit_sound = obj->hit_sound;
                dst->extra = xmalloc(strlen(new_extra) + 1);
                strcpy(dst->extra, new_extra);
                dst->col = obj->col;
                (*new_count)++;
            }
            else if (is_rice) {
                HitObject *next_obj = NULL;
                if (i + 1 < object_count && objects[i + 1].col == obj->col) {
                    next_obj = &objects[i + 1];
                }
                int convert = 0;
                int end_time = 0;
                if (next_obj) {
                    double beat_length = get_beat_length(obj->time);
                    double gap = beat_length / gap_divisor;
                    double next_time = (double)next_obj->time;
                    double end_time_d = next_time - gap;
                    if (next_time - obj->time >= gap - 0.001) {
                        end_time = (int)round(end_time_d);
                        if (end_time - obj->time >= 10) convert = 1;
                    }
                }
                new_objects = xrealloc(new_objects, (*new_count + 1) * sizeof(HitObject));
                HitObject *dst = &new_objects[*new_count];
                dst->x = obj->x;
                dst->y = obj->y;
                dst->time = obj->time;
                dst->hit_sound = obj->hit_sound;
                dst->col = obj->col;
                if (convert) {
                    dst->type = 128;
                    char extra[128];
                    snprintf(extra, sizeof(extra), "%d:0:0:0:0:", end_time);
                    dst->extra = xmalloc(strlen(extra) + 1);
                    strcpy(dst->extra, extra);
                } else {
                    dst->type = 1;
                    dst->extra = xmalloc(strlen("0:0:0:0:") + 1);
                    strcpy(dst->extra, "0:0:0:0:");
                }
                (*new_count)++;
            }
            else {
                new_objects = xrealloc(new_objects, (*new_count + 1) * sizeof(HitObject));
                new_objects[*new_count] = clone_object(obj);
                (*new_count)++;
            }
        }

        qsort(new_objects, *new_count, sizeof(HitObject), cmp_time_x);
        return new_objects;
    }

    /* Mode 2: delete LNs */
    if (process_ln == 2) {
        return convert_delete_mode(new_count, gap_divisor);
    }

    /* Mode 3: LN to rice */
    if (process_ln == 3) {
        return convert_ln_to_rice(new_count);
    }

    /* Should not reach here */
    *new_count = 0;
    return NULL;
}

/* ---------- Writing output ---------- */

static void write_output(const char *output_path, HitObject *new_objects, size_t new_count, int gap_divisor, int process_ln)
{
    FILE *fp = fopen(output_path, "w");
    if (!fp) {
        fprintf(stderr, "Error: cannot create output file '%s': %s\n", output_path, strerror(errno));
        exit(1);
    }

    int in_hit = 0;
    for (size_t i = 0; i < line_count; i++) {
        const char *line = lines[i];

        if (strcmp(line, "[HitObjects]") == 0) {
            in_hit = 1;
            fprintf(fp, "%s\n", line);
            for (size_t j = 0; j < new_count; j++) {
                fprintf(fp, "%d,%d,%d,%d,%d,%s\n",
                        new_objects[j].x, new_objects[j].y,
                        new_objects[j].time, new_objects[j].type,
                        new_objects[j].hit_sound, new_objects[j].extra);
            }
            continue;
        }

        if (in_hit && line[0] == '[') {
            in_hit = 0;
            fprintf(fp, "%s\n", line);
            continue;
        }
        if (in_hit) continue;

        if (strncmp(line, "Version:", 8) == 0) {
            if (process_ln == 3) {
                fprintf(fp, "%s (rc)\n", line);
            } else {
                fprintf(fp, "%s (inverse 1/%d)\n", line, gap_divisor);
            }
        } else {
            fprintf(fp, "%s\n", line);
        }
    }
    fclose(fp);
}

/* ---------- Cleanup ---------- */

static void free_all(void)
{
    for (size_t i = 0; i < line_count; i++) free(lines[i]);
    free(lines);
    for (size_t i = 0; i < object_count; i++) free(objects[i].extra);
    free(objects);
    free(timing_points);
}

static void reset_globals(void)
{
    free_all();
    lines = NULL;
    line_count = 0;
    objects = NULL;
    object_count = 0;
    timing_points = NULL;
    timing_count = 0;
    column_count = 4;
}

/* ---------- Windows file dialog ---------- */
#ifdef _WIN32
static int choose_input_file(char *buffer, size_t buffer_size)
{
    if (buffer_size > 0) buffer[0] = '\0';
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "osu! files (*.osu)\0*.osu\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = (DWORD)buffer_size;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select an osu!mania map (.osu)";
    return GetOpenFileNameA(&ofn);
}
#endif

/* ---------- Process one file ---------- */

static void process_file(const char *input_path, int gap_divisor, int process_ln)
{
    reset_globals();
    read_file(input_path);
    parse_circle_size();
    parse_timing_points();
    parse_hit_objects();

    if (object_count == 0) {
        fprintf(stderr, "Warning: no hit objects found, output will be empty.\n");
    }

    size_t new_count = 0;
    HitObject *new_objects = convert_objects(&new_count, gap_divisor, process_ln);

    char output_path[1024];
    strncpy(output_path, input_path, sizeof(output_path) - 1);
    output_path[sizeof(output_path) - 1] = '\0';
    char *dot = strrchr(output_path, '.');
    if (dot) *dot = '\0';

    if (process_ln == 3) {
        snprintf(output_path + strlen(output_path), sizeof(output_path) - strlen(output_path),
                 " (rc).osu");
    } else {
        snprintf(output_path + strlen(output_path), sizeof(output_path) - strlen(output_path),
                 " (inverse 1-%d).osu", gap_divisor);
    }

    write_output(output_path, new_objects, new_count, gap_divisor, process_ln);

    for (size_t i = 0; i < new_count; i++) free(new_objects[i].extra);
    free(new_objects);

    reset_globals();
}

/* ---------- Main ---------- */

int main(int argc, char *argv[])
{
    int gap_divisor = 4;
    int process_ln = 0;
    int file_start_index = 1;
    int use_gui_message = 0;   /* 1 = launched via drag-and-drop */

    /* Check if any explicit flags are present */
    int has_explicit_flags = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-gap") == 0 || strcmp(argv[i], "-mode") == 0 || strcmp(argv[i], "rc") == 0) {
            has_explicit_flags = 1;
            break;
        }
    }

    /* If no explicit flags and there are file arguments, treat as drag-and-drop */
    if (!has_explicit_flags && argc > 1) {
        use_gui_message = 1;
    }

    /* Parse arguments: support -gap <n>, -mode <m>, rc shortcut, and file list */
    if (argc > 1) {
        int i = 1;
        while (i < argc) {
            if (strcmp(argv[i], "-gap") == 0) {
                if (i + 1 < argc) {
                    gap_divisor = atoi(argv[i + 1]);
                    i += 2;
                } else {
                    i++;
                }
            } else if (strcmp(argv[i], "-mode") == 0) {
                if (i + 1 < argc) {
                    process_ln = atoi(argv[i + 1]);
                    i += 2;
                } else {
                    i++;
                }
            } else if (strcmp(argv[i], "rc") == 0) {
                /* rc shortcut: mode 3 */
                process_ln = 3;
                gap_divisor = 0;   /* gap not used in mode 3 */
                i++;
            } else {
                /* First non-flag argument: start of file list */
                file_start_index = i;
                break;
            }
        }
        /* If all arguments were flags, set file_start_index to argc */
        if (i >= argc) {
            file_start_index = argc;
        }
    }

    /* Validate parameters */
    if (gap_divisor <= 0 && process_ln != 3) {
        fprintf(stderr, "Warning: invalid gap divisor, using default 4.\n");
        gap_divisor = 4;
    }
    if (process_ln < 0 || process_ln > 3) {
        fprintf(stderr, "Warning: invalid LN mode, using default 0.\n");
        process_ln = 0;
    }

    /* If no files provided, enter interactive mode */
    if (file_start_index >= argc) {
        char input_path[1024] = {0};

        /* Ask for gap divisor or 'rc' */
        printf("Enter gap divisor (positive integer, e.g. 4 = 1/4 beat, 8 = 1/8 beat) or 'rc' to convert all LNs to rice: ");
        fflush(stdout);
        char input_str[32];
        if (scanf("%31s", input_str) == 1) {
            if (strcmp(input_str, "rc") == 0) {
                process_ln = 3;
                gap_divisor = 0;
            } else {
                int val = atoi(input_str);
                if (val > 0) {
                    gap_divisor = val;
                } else {
                    gap_divisor = 4;
                    printf("Invalid input, using default 4.\n");
                }
            }
        } else {
            gap_divisor = 4;
            printf("Using default 4.\n");
        }

        /* If not rc mode, ask for LN mode */
        if (process_ln != 3) {
            printf("Process existing LNs? (0 = keep, 1 = extend, 2 = delete): ");
            fflush(stdout);
            int mode_input;
            if (scanf("%d", &mode_input) == 1 && (mode_input == 0 || mode_input == 1 || mode_input == 2)) {
                process_ln = mode_input;
            } else {
                process_ln = 0;
                printf("Invalid input, using default 0.\n");
            }
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

#ifdef _WIN32
        if (!choose_input_file(input_path, sizeof(input_path))) {
            MessageBoxA(NULL, "No file selected.", "Information", MB_ICONINFORMATION);
            return 0;
        }
#else
        printf("Usage: %s [-gap <n>] [-mode <m>] <input.osu> [input2.osu ...]\n", argv[0]);
        return 1;
#endif

        process_file(input_path, gap_divisor, process_ln);

        char output_path[1024];
        strncpy(output_path, input_path, sizeof(output_path) - 1);
        output_path[sizeof(output_path) - 1] = '\0';
        char* dot = strrchr(output_path, '.');
        if (dot) *dot = '\0';
        if (process_ln == 3) {
            snprintf(output_path + strlen(output_path), sizeof(output_path) - strlen(output_path),
                     " (rc).osu");
        } else {
            snprintf(output_path + strlen(output_path), sizeof(output_path) - strlen(output_path),
                     " (inverse 1-%d).osu", gap_divisor);
        }

#ifdef _WIN32
        char msg[2048];
        snprintf(msg, sizeof(msg), "Conversion complete.\n\nOutput file:\n%s", output_path);
        MessageBoxA(NULL, msg, "Done", MB_OK | MB_ICONINFORMATION);
#else
        printf("Conversion complete. Output: %s\n", output_path);
#endif
        return 0;
    }

    /* Batch processing mode */
    int processed_count = 0;
    int failed_count = 0;

    /* If drag-and-drop, ask for gap divisor and LN mode before processing */
    if (use_gui_message) {
#ifdef _WIN32
        /* Bring console window to foreground and set focus */
        HWND hwnd = GetConsoleWindow();
        if (hwnd) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        }
#endif
        
        printf("Enter gap divisor (positive integer, default 4, or 'rc' to convert all LNs to rice): ");
        fflush(stdout);
        char input_str[32];
        if (scanf("%31s", input_str) == 1) {
            if (strcmp(input_str, "rc") == 0) {
                process_ln = 3;
                gap_divisor = 0;
            } else {
                int val = atoi(input_str);
                if (val > 0) {
                    gap_divisor = val;
                } else {
                    gap_divisor = 4;
                    printf("Invalid input, using default 4.\n");
                }
            }
        } else {
            gap_divisor = 4;
            printf("Using default 4.\n");
        }

        if (process_ln != 3) {
            printf("Process existing LNs? (0 = keep, 1 = extend, 2 = delete): ");
            fflush(stdout);
            int mode_input;
            if (scanf("%d", &mode_input) == 1 && (mode_input == 0 || mode_input == 1 || mode_input == 2)) {
                process_ln = mode_input;
            } else {
                process_ln = 0;
                printf("Invalid input, using default 0.\n");
            }
        }

        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }

    for (int i = file_start_index; i < argc; i++) {
        FILE *test = fopen(argv[i], "r");
        if (!test) {
            fprintf(stderr, "Warning: cannot open file %s, skipping.\n", argv[i]);
            failed_count++;
            continue;
        }
        fclose(test);

        printf("Processing: %s\n", argv[i]);
        process_file(argv[i], gap_divisor, process_ln);
        processed_count++;
    }

    /* Show summary */
    if (use_gui_message) {
#ifdef _WIN32
        char msg[256];
        snprintf(msg, sizeof(msg), "Processed %d file(s), skipped %d.", processed_count, failed_count);
        MessageBoxA(NULL, msg, "Batch conversion complete", MB_OK | MB_ICONINFORMATION);
#else
        printf("Processed %d file(s), skipped %d.\n", processed_count, failed_count);
#endif
    } else {
        printf("Processed %d file(s), skipped %d.\n", processed_count, failed_count);
    }

    return 0;
}
