#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Definições de tipos básicos */
typedef int32_t variable_t;     // Identificador de variável (1, 2, 3, ...)
typedef int32_t literal_t;      // Literal (positivo ou negativo: 1, -1, 2, -2, ...)

/* Status de satisfatibilidade */
typedef enum {
    SAT_UNKNOWN = 0,
    SAT_SATISFIABLE = 1,
    SAT_UNSATISFIABLE = -1
} sat_result_t;

/* Status de uma variável */
typedef enum {
    VAR_UNASSIGNED = 0,
    VAR_TRUE = 1,
    VAR_FALSE = -1
} var_assignment_t;

/* Estrutura para representar um literal */
typedef struct {
    variable_t variable;    // Número da variável (sempre positivo)
    bool is_positive;       // true se literal positivo, false se negativo
} literal_struct_t;

/* Estrutura para uma cláusula */
typedef struct {
    literal_t *literals;    // Array de literais na cláusula
    size_t size;           // Número de literais
    size_t capacity;       // Capacidade alocada
    bool is_satisfied;     // Cache: se a cláusula está satisfeita
    bool is_unit;          // Cache: se é uma cláusula unitária
    literal_t unit_literal; // Se unitária, qual é o literal unitário
} clause_t;

/* Lista de cláusulas */
typedef struct {
    clause_t *clauses;     // Array de cláusulas
    size_t count;          // Número de cláusulas
    size_t capacity;       // Capacidade alocada
} clause_list_t;

/* Estrutura principal da fórmula CNF */
typedef struct {
    clause_list_t clauses;      // Lista de todas as cláusulas
    variable_t num_variables;   // Número total de variáveis
    var_assignment_t *assignment; // Array de atribuições de variáveis [1..num_variables]
    
    /* Estatísticas e cache */
    size_t satisfied_clauses;   // Número de cláusulas satisfeitas
    bool *variable_used;        // Quais variáveis são usadas na fórmula
    
    /* Para otimizações */
    clause_list_t **positive_occurrences; // Cláusulas onde cada variável aparece positiva
    clause_list_t **negative_occurrences; // Cláusulas onde cada variável aparece negativa
} cnf_formula_t;

/* Estrutura para o estado do solver (pilha de decisões) */
typedef struct {
    variable_t variable;        // Variável decidida
    var_assignment_t value;     // Valor atribuído
    size_t decision_level;      // Nível de decisão
    bool is_decision;          // true se foi uma decisão, false se propagação
} assignment_entry_t;

typedef struct {
    assignment_entry_t *stack;  // Pilha de atribuições
    size_t size;               // Tamanho atual da pilha
    size_t capacity;           // Capacidade da pilha
    size_t decision_level;     // Nível atual de decisão
} assignment_stack_t;

/* Funções para manipulação de literais */
static inline literal_t make_literal(variable_t var, bool positive) {
    return positive ? var : -var;
}

static inline variable_t literal_variable(literal_t lit) {
    return lit > 0 ? lit : -lit;
}

static inline bool literal_is_positive(literal_t lit) {
    return lit > 0;
}

static inline literal_t literal_negate(literal_t lit) {
    return -lit;
}

/* Funções para manipulação de cláusulas */
clause_t* clause_create(size_t initial_capacity);
void clause_destroy(clause_t *clause);
bool clause_add_literal(clause_t *clause, literal_t literal);
bool clause_is_satisfied(const clause_t *clause, const var_assignment_t *assignment);
bool clause_is_unit(const clause_t *clause, const var_assignment_t *assignment, literal_t *unit_literal);
bool clause_is_conflicting(const clause_t *clause, const var_assignment_t *assignment);
bool clause_is_tautology(const clause_t *clause);
clause_t* clause_copy(const clause_t *clause);

/* Funções para lista de cláusulas */
clause_list_t* clause_list_create(size_t initial_capacity);
void clause_list_destroy(clause_list_t *list);
/* Libera apenas o conteúdo interno (literals/array), sem dar free no próprio list */
void clause_list_dispose_contents(clause_list_t *list);
bool clause_list_add(clause_list_t *list, clause_t *clause);
void clause_list_clear(clause_list_t *list);

/* Funções para fórmula CNF */
cnf_formula_t* cnf_create(variable_t num_variables);
void cnf_destroy(cnf_formula_t *cnf);
bool cnf_add_clause(cnf_formula_t *cnf, clause_t *clause);
bool cnf_is_satisfied(const cnf_formula_t *cnf);
bool cnf_has_conflict(const cnf_formula_t *cnf);
void cnf_update_caches(cnf_formula_t *cnf);
void cnf_build_occurrence_lists(cnf_formula_t *cnf);

/* Funções para atribuições */
assignment_stack_t* assignment_stack_create(size_t initial_capacity);
void assignment_stack_destroy(assignment_stack_t *stack);
bool assignment_stack_push(assignment_stack_t *stack, variable_t var, 
                          var_assignment_t value, bool is_decision);
bool assignment_stack_pop(assignment_stack_t *stack, assignment_entry_t *entry);
void assignment_stack_backtrack_to_level(assignment_stack_t *stack, size_t level);
void assignment_stack_clear(assignment_stack_t *stack);

/* Funções utilitárias */
void cnf_print_stats(const cnf_formula_t *cnf);
void cnf_print_formula(const cnf_formula_t *cnf);
bool cnf_validate_assignment(const cnf_formula_t *cnf);

#endif /* STRUCTURES_H */