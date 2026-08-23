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
    char *extra;          /* dynamically allocated extra string */
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
    size_t capacity = 256;       /* initial buffer size */
    size_t length = 0;
    char *buffer = xmalloc(capacity);
    int ch;

    while ((ch = fgetc(fp)) != EOF && ch != '\n') {
        /* skip carriage return (Windows line endings) */
        if (ch == '\r') {
            continue;
        }

        /* grow buffer if needed */
        if (length + 1 >= capacity) {
            capacity *= 2;
            buffer = xrealloc(buffer, capacity);
        }

        buffer[length++] = (char)ch;
    }

    /* EOF and nothing read -> return NULL */
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
        if (in_difficulty && line[0] == '[') {
            break;
        }
        if (in_difficulty && strncmp(line, "CircleSize:", 11) == 0) {
            int val = atoi(line + 11);
            if (val >= 1 && val <= 18) {   /* sane column count for mania */
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
        if (in_timing && line[0] == '[') {
            break;
        }
        if (in_timing && line[0] != '\0' && line[0] != '/') {
            double time, beat_length;
            int uninherited;
            if (sscanf(line, "%lf,%lf,%*d,%*d,%*d,%*d,%d",
                       &time, &beat_length, &uninherited) == 3) {
                if (uninherited == 1) {  // red line
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
    double beat_length = 500.0;  /* default 120 BPM */
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
        if (in_hit && line[0] == '[') {
            break;
        }
        if (in_hit && line[0] != '\0' && line[0] != '/') {
            HitObject obj;
            char extra[512];   /* temporary buffer for extra field */

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

            /* Calculate column */
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

/* ---------- Object conversion ---------- */

static HitObject *convert_objects(size_t *new_count, int gap_divisor)
{
    /* Sort original objects by column, then time */
    qsort(objects, object_count, sizeof(HitObject), cmp_col_time);

    HitObject *new_objects = NULL;
    *new_count = 0;

    for (size_t i = 0; i < object_count; i++) {
        HitObject *obj = &objects[i];
        int is_ln   = (obj->type & 128) != 0;
        int is_rice = (obj->type & 1) != 0 && !is_ln;

        if (is_ln) {
            /* Copy LN as is */
            new_objects = xrealloc(new_objects, (*new_count + 1) * sizeof(HitObject));
            HitObject *dst = &new_objects[*new_count];
            dst->x = obj->x;
            dst->y = obj->y;
            dst->time = obj->time;
            dst->type = obj->type;
            dst->hit_sound = obj->hit_sound;
            dst->extra = xmalloc(strlen(obj->extra) + 1);
            strcpy(dst->extra, obj->extra);
            dst->col = obj->col;
            (*new_count)++;
        }
        else if (is_rice) {
            /* Find next object in same column */
            HitObject *next_obj = NULL;
            if (i + 1 < object_count && objects[i + 1].col == obj->col) {
                next_obj = &objects[i + 1];
            }

            int convert = 0;
            int end_time = 0;

            if (next_obj) {
                double beat_length = get_beat_length(obj->time);
                double gap = beat_length / gap_divisor;   /* custom gap */
                double next_time = (double)next_obj->time;
                double end_time_d = next_time - gap;

                /* Check interval >= gap (with small epsilon) */
                if (next_time - obj->time >= gap - 0.001) {
                    end_time = (int)round(end_time_d);
                    /* Ensure LN length at least 10 ms */
                    if (end_time - obj->time >= 10) {
                        convert = 1;
                    }
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
                dst->type = 128;  /* LN */
                char extra[128];
                snprintf(extra, sizeof(extra), "%d:0:0:0:0:", end_time);
                dst->extra = xmalloc(strlen(extra) + 1);
                strcpy(dst->extra, extra);
            } else {
                dst->type = 1;    /* normal note */
                dst->extra = xmalloc(strlen("0:0:0:0:") + 1);
                strcpy(dst->extra, "0:0:0:0:");
            }
            (*new_count)++;
        }
        else {
            /* Unknown type, copy as is */
            new_objects = xrealloc(new_objects, (*new_count + 1) * sizeof(HitObject));
            HitObject *dst = &new_objects[*new_count];
            dst->x = obj->x;
            dst->y = obj->y;
            dst->time = obj->time;
            dst->type = obj->type;
            dst->hit_sound = obj->hit_sound;
            dst->extra = xmalloc(strlen(obj->extra) + 1);
            strcpy(dst->extra, obj->extra);
            dst->col = obj->col;
            (*new_count)++;
        }
    }

    qsort(new_objects, *new_count, sizeof(HitObject), cmp_time_x);
    return new_objects;
}

/* ---------- Writing output ---------- */

static void write_output(const char *output_path, HitObject *new_objects, size_t new_count, int gap_divisor)
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

            /* Write new objects */
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

        if (in_hit) {
            /* skip original hit objects */
            continue;
        }

        /* Modify Version line to include gap info */
        if (strncmp(line, "Version:", 8) == 0) {
            fprintf(fp, "%s (inverse 1/%d)\n", line, gap_divisor);
        } else {
            fprintf(fp, "%s\n", line);
        }
    }

    fclose(fp);
}

/* ---------- Cleanup ---------- */

static void free_all(void)
{
    for (size_t i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(lines);

    for (size_t i = 0; i < object_count; i++) {
        free(objects[i].extra);
    }
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

static void process_file(const char *input_path, int gap_divisor)
{
    /* Ensure clean global state before processing */
    reset_globals();

    /* Read and parse input */
    read_file(input_path);
    parse_circle_size();
    parse_timing_points();
    parse_hit_objects();

    if (object_count == 0) {
        fprintf(stderr, "Warning: no hit objects found, output will be empty.\n");
    }

    /* Convert */
    size_t new_count = 0;
    HitObject *new_objects = convert_objects(&new_count, gap_divisor);

    /* Build output filename */
    char output_path[1024];
    strncpy(output_path, input_path, sizeof(output_path) - 1);
    output_path[sizeof(output_path) - 1] = '\0';

    char *dot = strrchr(output_path, '.');
    if (dot) {
        *dot = '\0';
    }
    snprintf(output_path + strlen(output_path), sizeof(output_path) - strlen(output_path),
             " (inverse 1-%d).osu", gap_divisor);

    /* Write output */
    write_output(output_path, new_objects, new_count, gap_divisor);

    /* Free new_objects */
    for (size_t i = 0; i < new_count; i++) {
        free(new_objects[i].extra);
    }
    free(new_objects);

    /* Clean up global data (lines, objects, timing_points) */
    reset_globals();
}

/* ---------- Main ---------- */

int main(int argc, char *argv[])
{
    int gap_divisor = 4;
    int use_gui_message = 0;
    int file_start_index = 1;

    if (argc > 1) {
        int maybe_gap = atoi(argv[1]);
        if (maybe_gap > 0) {
            FILE *test = fopen(argv[1], "r");
            if (test) {
                fclose(test);
                use_gui_message = 1;
            } else {
                gap_divisor = maybe_gap;
                file_start_index = 2;
            }
        } else {
            use_gui_message = 1;
        }
    }

    if (file_start_index >= argc) {
        char input_path[1024];

        if (argc < 2) {
            printf("Enter gap divisor (positive integer, e.g. 4 = 1/4 beat, 8 = 1/8 beat): ");
            fflush(stdout);
            if (scanf("%d", &gap_divisor) != 1 || gap_divisor <= 0) {
                gap_divisor = 4;
                printf("Invalid input, using default 4 (1/4 beat).\n");
            }
        }

#ifdef _WIN32
        if (!choose_input_file(input_path, sizeof(input_path))) {
            MessageBoxA(NULL, "No file selected.", "Information", MB_ICONINFORMATION);
            return 0;
        }
#else
        printf("Usage: %s [gap_divisor] <input.osu> [input2.osu ...]\n", argv[0]);
        return 1;
#endif

        process_file(input_path, gap_divisor);
        return 0;
    }

    int processed_count = 0;
    int failed_count = 0;

    if (use_gui_message) {
        printf("Enter gap divisor (positive integer, default 4, press Enter for default): ");
        fflush(stdout);
        char input_str[32];
        if (scanf("%31s", input_str) == 1) {
            int val = atoi(input_str);
            if (val > 0) {
                gap_divisor = val;
            } else {
                printf("Invalid input, using default 4.\n");
            }
        } else {
            printf("Using default 4.\n");
        }
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
        process_file(argv[i], gap_divisor);
        processed_count++;
    }

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