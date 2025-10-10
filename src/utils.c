#include "utils.h"
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

/* ========== Funções de Tempo ========== */

double get_current_time(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

void timer_start(timer_t *timer) {
    if (timer) {
        timer->start_time = get_current_time();
    }
}

void timer_stop(timer_t *timer) {
    if (timer) {
        timer->end_time = get_current_time();
    }
}

double timer_elapsed(const timer_t *timer) {
    if (!timer) return 0.0;
    return timer->end_time - timer->start_time;
}

/* ========== Funções de Logging ========== */

void log_info(const char *format, ...) {
    printf(COLOR_GREEN "[INFO] " COLOR_RESET);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void log_warning(const char *format, ...) {
    printf(COLOR_YELLOW "[AVISO] " COLOR_RESET);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void log_error(const char *format, ...) {
    fprintf(stderr, COLOR_RED "[ERRO] " COLOR_RESET);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void log_debug(const char *format, ...) {
#ifdef DEBUG
    printf(COLOR_CYAN "[DEBUG] " COLOR_RESET);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
#else
    (void)format; /* Evitar warning de parâmetro não usado */
#endif
}

/* ========== Funções de Estatísticas ========== */

void stats_init(solver_stats_t *stats) {
    if (stats) {
        memset(stats, 0, sizeof(solver_stats_t));
    }
}

void stats_print(const solver_stats_t *stats) {
    if (!stats) return;
    
    printf("\n" COLOR_BLUE "=== Estatísticas do Solver ===" COLOR_RESET "\n");
    printf("Decisões:              %llu\n", (unsigned long long)stats->decisions);
    printf("Propagações:           %llu\n", (unsigned long long)stats->propagations);
    printf("Conflitos:             %llu\n", (unsigned long long)stats->conflicts);
    printf("Reinicializações:      %llu\n", (unsigned long long)stats->restarts);
    printf("Cláusulas aprendidas:  %llu\n", (unsigned long long)stats->learned_clauses);
    printf("Nível máximo:          %zu\n", stats->max_decision_level);
    printf("Tempo total:           %.6f segundos\n", stats->solve_time);
    
    if (stats->solve_time > 0.0) {
        printf("Decisões por segundo:  %.2f\n", stats->decisions / stats->solve_time);
        printf("Propagações por seg:   %.2f\n", stats->propagations / stats->solve_time);
    }
    printf("\n");
}

void stats_reset(solver_stats_t *stats) {
    stats_init(stats);
}

/* ========== Funções de Memória ========== */

void* safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        log_error("Falha na alocação de memória: %zu bytes", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void* safe_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        log_error("Falha na realocação de memória: %zu bytes", size);
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

void* safe_calloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (!ptr && nmemb > 0 && size > 0) {
        log_error("Falha na alocação de memória zerada: %zu elementos de %zu bytes", nmemb, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

/* ========== Funções de String ========== */

char* trim_string(char *str) {
    if (!str) return NULL;
    
    /* Remove espaços do início */
    while (isspace(*str)) str++;
    
    /* String vazia */
    if (*str == 0) return str;
    
    /* Remove espaços do final */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    end[1] = '\0';
    
    return str;
}

bool string_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool string_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return false;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

char* string_duplicate(const char *str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

/* ========== Funções de Arquivo ========== */

bool file_exists(const char *filename) {
    if (!filename) return false;
    
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

size_t file_size(const char *filename) {
    if (!filename) return 0;
    
    FILE *file = fopen(filename, "r");
    if (!file) return 0;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    
    return size >= 0 ? (size_t)size : 0;
}

char* read_entire_file(const char *filename) {
    if (!filename) return NULL;
    
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(content, 1, size, file);
    content[read_size] = '\0';
    
    fclose(file);
    return content;
}

/* ========== Funções de Validação ========== */

bool is_valid_variable(int var, int max_variables) {
    return var >= 1 && var <= max_variables;
}

bool is_valid_literal(int lit, int max_variables) {
    if (lit == 0) return false;
    int var = lit > 0 ? lit : -lit;
    return is_valid_variable(var, max_variables);
}

/* ========== Funções Matemáticas ========== */

int int_abs(int x) {
    return x >= 0 ? x : -x;
}

int int_max(int a, int b) {
    return a > b ? a : b;
}

int int_min(int a, int b) {
    return a < b ? a : b;
}

size_t size_max(size_t a, size_t b) {
    return a > b ? a : b;
}

size_t size_min(size_t a, size_t b) {
    return a < b ? a : b;
}

/* ========== Funções de Hash ========== */

uint32_t hash_int(int value) {
    uint32_t hash = (uint32_t)value;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = (hash >> 16) ^ hash;
    return hash;
}

uint32_t hash_string(const char *str) {
    if (!str) return 0;
    
    uint32_t hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    
    return hash;
}

/* ========== Gerador de Números Aleatórios ========== */

static unsigned int random_state = 1;

void random_seed(unsigned int seed) {
    random_state = seed;
}

int random_int(int min, int max) {
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    
    random_state = random_state * 1103515245 + 12345;
    return min + (int)((random_state / 65536) % (max - min + 1));
}

double random_double(void) {
    random_state = random_state * 1103515245 + 12345;
    return (double)(random_state / 65536) / 32768.0;
}

/* ========== Funções de Parsing ========== */

bool parse_int(const char *str, int *result) {
    if (!str || !result) return false;
    
    char *endptr;
    long value = strtol(str, &endptr, 10);
    
    if (endptr == str || *endptr != '\0') return false;
    if (value < INT32_MIN || value > INT32_MAX) return false;
    
    *result = (int)value;
    return true;
}

bool parse_long(const char *str, long *result) {
    if (!str || !result) return false;
    
    char *endptr;
    long value = strtol(str, &endptr, 10);
    
    if (endptr == str || *endptr != '\0') return false;
    
    *result = value;
    return true;
}

bool parse_double(const char *str, double *result) {
    if (!str || !result) return false;
    
    char *endptr;
    double value = strtod(str, &endptr);
    
    if (endptr == str || *endptr != '\0') return false;
    
    *result = value;
    return true;
}