#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stddef.h>

/* Macros de debug */
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
        do { fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#else
    #define DEBUG_PRINT(fmt, ...) do {} while(0)
#endif

/* Macros para tratamento de erros */
#define SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            ptr = NULL; \
        } \
    } while(0)

#define CHECK_NULL(ptr, msg) \
    do { \
        if (!(ptr)) { \
            fprintf(stderr, "Erro: %s\n", msg); \
            return false; \
        } \
    } while(0)

#define CHECK_NULL_PTR(ptr, msg) \
    do { \
        if (!(ptr)) { \
            fprintf(stderr, "Erro: %s\n", msg); \
            return NULL; \
        } \
    } while(0)

/* Cores para output no terminal */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_RESET   "\033[0m"

/* Estrutura para medição de tempo */
typedef struct {
    double start_time;
    double end_time;
} timer_t;

/* Estrutura para estatísticas do solver */
typedef struct {
    uint64_t decisions;          // Número de decisões feitas
    uint64_t propagations;       // Número de propagações
    uint64_t conflicts;          // Número de conflitos encontrados
    uint64_t restarts;          // Número de reinicializações
    uint64_t learned_clauses;   // Número de cláusulas aprendidas
    double solve_time;          // Tempo total de resolução
    size_t max_decision_level;  // Nível máximo de decisão alcançado
} solver_stats_t;

/* Funções de tempo */
double get_current_time(void);
void timer_start(timer_t *timer);
void timer_stop(timer_t *timer);
double timer_elapsed(const timer_t *timer);

/* Funções de logging */
void log_info(const char *format, ...);
void log_warning(const char *format, ...);
void log_error(const char *format, ...);
void log_debug(const char *format, ...);

/* Funções de estatísticas */
void stats_init(solver_stats_t *stats);
void stats_print(const solver_stats_t *stats);
void stats_reset(solver_stats_t *stats);

/* Funções de memória e arrays dinâmicos */
void* safe_malloc(size_t size);
void* safe_realloc(void *ptr, size_t size);
void* safe_calloc(size_t nmemb, size_t size);

/* Funções de manipulação de strings */
char* trim_string(char *str);
bool string_starts_with(const char *str, const char *prefix);
bool string_ends_with(const char *str, const char *suffix);
char* string_duplicate(const char *str);

/* Funções de arquivo */
bool file_exists(const char *filename);
size_t file_size(const char *filename);
char* read_entire_file(const char *filename);

/* Funções de validação */
bool is_valid_variable(int var, int max_variables);
bool is_valid_literal(int lit, int max_variables);

/* Funções matemáticas utilitárias */
int int_abs(int x);
int int_max(int a, int b);
int int_min(int a, int b);
size_t size_max(size_t a, size_t b);
size_t size_min(size_t a, size_t b);

/* Funções de hash (para futuras otimizações) */
uint32_t hash_int(int value);
uint32_t hash_string(const char *str);

/* Gerador de números aleatórios simples */
void random_seed(unsigned int seed);
int random_int(int min, int max);
double random_double(void);

/* Funções de parsing */
bool parse_int(const char *str, int *result);
bool parse_long(const char *str, long *result);
bool parse_double(const char *str, double *result);

/* Constantes úteis */
#define MAX_LINE_LENGTH 1024
#define MAX_FILENAME_LENGTH 256
#define DEFAULT_CAPACITY 16

/* Macros para comparação e troca */
#define SWAP(a, b, type) \
    do { \
        type temp = (a); \
        (a) = (b); \
        (b) = temp; \
    } while(0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif /* UTILS_H */