#ifndef SOLVER_H
#define SOLVER_H

#include "structures.h"
#include "utils.h"
#include <stddef.h>

/* Status de retorno do solver */
typedef enum {
    SOLVER_SATISFIABLE = 1,
    SOLVER_UNSATISFIABLE = -1,
    SOLVER_UNKNOWN = 0,
    SOLVER_TIMEOUT = -2,
    SOLVER_MEMORY_ERROR = -3,
    SOLVER_ERROR = -4
} solver_result_t;

/* Estratégias de decisão de variáveis */
typedef enum {
    DECISION_FIRST_UNASSIGNED = 0,  /* Primeira variável não atribuída */
    DECISION_MOST_FREQUENT = 1,     /* Variável mais frequente */
    DECISION_JEROSLOW_WANG = 2,     /* Heurística Jeroslow-Wang */
    DECISION_RANDOM = 3             /* Aleatória */
} decision_strategy_t;

/* Configuração do solver */
typedef struct {
    decision_strategy_t decision_strategy;  /* Estratégia de decisão */
    bool enable_pure_literal;              /* Ativar eliminação de literais puros */
    bool enable_unit_propagation;          /* Ativar propagação unitária */
    bool enable_preprocessing;             /* Ativar pré-processamento */
    bool enable_restarts;                 /* Ativar reinicializações */
    size_t max_decisions;                 /* Máximo de decisões (0 = sem limite) */
    double timeout_seconds;               /* Timeout em segundos (0 = sem timeout) */
    size_t restart_threshold;             /* Threshold para reinicialização */
    bool verbose;                         /* Modo verboso */
} solver_config_t;

/* Estado do solver DPLL */
typedef struct {
    cnf_formula_t *formula;           /* Fórmula a ser resolvida */
    assignment_stack_t *assignments;   /* Pilha de atribuições */
    solver_stats_t stats;             /* Estatísticas */
    solver_config_t config;           /* Configuração */
    
    /* Cache e estruturas auxiliares */
    bool *pure_literals;              /* Cache de literais puros */
    clause_t **unit_clauses;          /* Lista de cláusulas unitárias */
    size_t unit_clauses_count;        /* Número de cláusulas unitárias */
    
    /* Estado interno */
    bool formula_modified;            /* Se a fórmula foi modificada */
    size_t conflicts_since_restart;   /* Conflitos desde último restart */
    timer_t total_timer;              /* Timer total */
} dpll_solver_t;

/* Configuração padrão */
extern const solver_config_t DEFAULT_SOLVER_CONFIG;

/* ========== Funções Principais ========== */

/* Criar e destruir solver */
dpll_solver_t* solver_create(cnf_formula_t *formula);
dpll_solver_t* solver_create_with_config(cnf_formula_t *formula, const solver_config_t *config);
void solver_destroy(dpll_solver_t *solver);

/* Função principal de resolução */
solver_result_t solver_solve(dpll_solver_t *solver);

/* ========== Algoritmo DPLL ========== */

/* Função principal do DPLL */
solver_result_t dpll_algorithm(dpll_solver_t *solver);

/* Propagação de unidades */
solver_result_t unit_propagation(dpll_solver_t *solver);

/* Eliminação de literais puros */
bool pure_literal_elimination(dpll_solver_t *solver);

/* Funções de decisão */
variable_t choose_decision_variable(dpll_solver_t *solver);
var_assignment_t choose_decision_value(dpll_solver_t *solver, variable_t var);

/* Backtracking */
bool backtrack(dpll_solver_t *solver);
void print_assignment_stack(const dpll_solver_t *solver);

/* ========== Estratégias de Decisão ========== */

variable_t decision_first_unassigned(const dpll_solver_t *solver);
variable_t decision_most_frequent(const dpll_solver_t *solver);
variable_t decision_jeroslow_wang(const dpll_solver_t *solver);
variable_t decision_random(const dpll_solver_t *solver);

/* ========== Pré-processamento ========== */

bool preprocess_formula(dpll_solver_t *solver);
bool remove_satisfied_clauses(dpll_solver_t *solver);
bool simplify_clauses(dpll_solver_t *solver);
bool eliminate_pure_literals_preprocessing(dpll_solver_t *solver);

/* ========== Análise e Detecção ========== */

/* Detectar cláusulas unitárias */
bool find_unit_clauses(dpll_solver_t *solver);

/* Detectar literais puros */
bool find_pure_literals(dpll_solver_t *solver);

/* Detectar conflitos */
bool has_conflict(const dpll_solver_t *solver);

/* Verificar se fórmula está satisfeita */
bool is_formula_satisfied(const dpll_solver_t *solver);

/* ========== Configuração e Estado ========== */

void solver_set_config(dpll_solver_t *solver, const solver_config_t *config);
void solver_reset(dpll_solver_t *solver);
bool solver_is_timeout(const dpll_solver_t *solver);

/* ========== Utilidades ========== */

/* Atribuir variável */
bool assign_variable(dpll_solver_t *solver, variable_t var, var_assignment_t value, bool is_decision);

/* Desatribuir até nível */
void unassign_until_level(dpll_solver_t *solver, size_t level);

/* Verificar consistência */
bool solver_check_consistency(const dpll_solver_t *solver);

/* ========== Informações e Debug ========== */

void solver_print_stats(const dpll_solver_t *solver);
void solver_print_assignment(const dpll_solver_t *solver);
void solver_print_state(const dpll_solver_t *solver);

/* Converter resultado para string */
const char* solver_result_string(solver_result_t result);

/* ========== Heurísticas Avançadas ========== */

/* Calcular score Jeroslow-Wang para variável */
double calculate_jeroslow_wang_score(const dpll_solver_t *solver, variable_t var);

/* Calcular frequência de literal */
size_t calculate_literal_frequency(const dpll_solver_t *solver, literal_t literal);

/* ========== Reinicializações ========== */

bool should_restart(const dpll_solver_t *solver);
void perform_restart(dpll_solver_t *solver);

/* ========== Validação ========== */

bool validate_solution(const dpll_solver_t *solver);
bool validate_partial_assignment(const dpll_solver_t *solver);

/* ========== Macros Úteis ========== */

#define SOLVER_TIMEOUT_CHECK(solver) \
    do { \
        if (solver_is_timeout(solver)) { \
            return SOLVER_TIMEOUT; \
        } \
    } while(0)

#define SOLVER_STATS_INCREMENT(solver, field) \
    do { \
        (solver)->stats.field++; \
    } while(0)

#define IS_VARIABLE_ASSIGNED(solver, var) \
    ((solver)->formula->assignment[var] != VAR_UNASSIGNED)

#define IS_LITERAL_SATISFIED(solver, lit) \
    ((lit > 0 && (solver)->formula->assignment[lit] == VAR_TRUE) || \
     (lit < 0 && (solver)->formula->assignment[-lit] == VAR_FALSE))

#define IS_LITERAL_FALSIFIED(solver, lit) \
    ((lit > 0 && (solver)->formula->assignment[lit] == VAR_FALSE) || \
     (lit < 0 && (solver)->formula->assignment[-lit] == VAR_TRUE))

#endif /* SOLVER_H */