/**
 * @file structures.c
 * @brief Implementação das estruturas de dados para fórmulas CNF e pilha de atribuições
 * @author SAT Solver Team
 * @date 2025
 * 
 * Este arquivo implementa as estruturas fundamentais do SAT solver:
 * - Cláusulas (conjuntos de literais)
 * - Fórmulas CNF (conjuntos de cláusulas)
 * - Pilha de atribuições para backtracking
 * - Operações de avaliação e manipulação
 */

#include "structures.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========== Funções para Cláusulas ========== */

/**
 * @brief Cria uma nova cláusula com capacidade inicial especificada
 * @param initial_capacity Capacidade inicial do array de literais
 * @return Ponteiro para a cláusula criada ou NULL em caso de erro
 * 
 * Aloca memória para uma cláusula vazia que pode crescer dinamicamente.
 * Capacidade mínima é 4 literais para evitar realocações frequentes.
 */
clause_t* clause_create(size_t initial_capacity) {
    clause_t *clause = malloc(sizeof(clause_t));
    if (!clause) return NULL;
    
    clause->capacity = initial_capacity > 0 ? initial_capacity : 4;
    clause->literals = malloc(clause->capacity * sizeof(literal_t));
    if (!clause->literals) {
        free(clause);
        return NULL;
    }
    
    clause->size = 0;                    ///< Inicialmente vazia
    clause->is_satisfied = false;        ///< Estado não satisfeita
    clause->is_unit = false;
    clause->unit_literal = 0;
    
    return clause;
}

void clause_destroy(clause_t *clause) {
    if (clause) {
        free(clause->literals);
        free(clause);
    }
}

bool clause_add_literal(clause_t *clause, literal_t literal) {
    if (!clause || literal == 0) return false;
    
    /* Evitar duplicatas exatas, mas permitir polaridades opostas para detectar tautologia depois */
    for (size_t i = 0; i < clause->size; i++) {
        if (clause->literals[i] == literal) {
            return true; /* Literal já existe, não é erro */
        }
    }
    
    /* Expandir array se necessário */
    if (clause->size >= clause->capacity) {
        size_t new_capacity = clause->capacity * 2;
        literal_t *new_literals = realloc(clause->literals, new_capacity * sizeof(literal_t));
        if (!new_literals) return false;
        
        clause->literals = new_literals;
        clause->capacity = new_capacity;
    }
    
    clause->literals[clause->size++] = literal;
    return true;
}

bool clause_is_tautology(const clause_t *clause) {
    if (!clause) return false;
    for (size_t i = 0; i < clause->size; ++i) {
        variable_t vi = literal_variable(clause->literals[i]);
        bool pi = literal_is_positive(clause->literals[i]);
        for (size_t j = i + 1; j < clause->size; ++j) {
            if (vi == literal_variable(clause->literals[j])) {
                bool pj = literal_is_positive(clause->literals[j]);
                if (pi != pj) {
                    return true; /* contém v e ¬v */
                }
            }
        }
    }
    return false;
}

bool clause_is_satisfied(const clause_t *clause, const var_assignment_t *assignment) {
    if (!clause || !assignment) return false;
    
    for (size_t i = 0; i < clause->size; i++) {
        literal_t lit = clause->literals[i];
        variable_t var = literal_variable(lit);
        bool is_positive = literal_is_positive(lit);
        
        var_assignment_t var_value = assignment[var];
        
        if ((is_positive && var_value == VAR_TRUE) || 
            (!is_positive && var_value == VAR_FALSE)) {
            return true;
        }
    }
    
    return false;
}

bool clause_is_unit(const clause_t *clause, const var_assignment_t *assignment, literal_t *unit_literal) {
    if (!clause || !assignment) return false;
    
    literal_t unassigned_literal = 0;
    int unassigned_count = 0;
    
    for (size_t i = 0; i < clause->size; i++) {
        literal_t lit = clause->literals[i];
        variable_t var = literal_variable(lit);
        bool is_positive = literal_is_positive(lit);
        
        var_assignment_t var_value = assignment[var];
        
        /* Se algum literal está satisfeito, não é unitária */
        if ((is_positive && var_value == VAR_TRUE) || 
            (!is_positive && var_value == VAR_FALSE)) {
            return false;
        }
        
        /* Se literal não está atribuído */
        if (var_value == VAR_UNASSIGNED) {
            unassigned_literal = lit;
            unassigned_count++;
            if (unassigned_count > 1) {
                return false; /* Mais de um literal não atribuído */
            }
        }
    }
    
    if (unassigned_count == 1) {
        if (unit_literal) *unit_literal = unassigned_literal;
        return true;
    }
    
    return false;
}

bool clause_is_conflicting(const clause_t *clause, const var_assignment_t *assignment) {
    if (!clause || !assignment) return false;
    
    for (size_t i = 0; i < clause->size; i++) {
        literal_t lit = clause->literals[i];
        variable_t var = literal_variable(lit);
        bool is_positive = literal_is_positive(lit);
        
        var_assignment_t var_value = assignment[var];
        
        /* Se algum literal não está falsificado, não há conflito */
        if (var_value == VAR_UNASSIGNED ||
            (is_positive && var_value == VAR_TRUE) || 
            (!is_positive && var_value == VAR_FALSE)) {
            return false;
        }
    }
    
    return true; /* Todos os literais estão falsificados */
}

clause_t* clause_copy(const clause_t *clause) {
    if (!clause) return NULL;
    
    clause_t *copy = clause_create(clause->capacity);
    if (!copy) return NULL;
    
    copy->size = clause->size;
    memcpy(copy->literals, clause->literals, clause->size * sizeof(literal_t));
    copy->is_satisfied = clause->is_satisfied;
    copy->is_unit = clause->is_unit;
    copy->unit_literal = clause->unit_literal;
    
    return copy;
}

/* ========== Funções para Lista de Cláusulas ========== */

clause_list_t* clause_list_create(size_t initial_capacity) {
    clause_list_t *list = malloc(sizeof(clause_list_t));
    if (!list) return NULL;
    
    list->capacity = initial_capacity > 0 ? initial_capacity : 8;
    list->clauses = malloc(list->capacity * sizeof(clause_t));
    if (!list->clauses) {
        free(list);
        return NULL;
    }
    
    list->count = 0;
    return list;
}

void clause_list_destroy(clause_list_t *list) {
    if (list) {
        for (size_t i = 0; i < list->count; i++) {
            /* As cláusulas são armazenadas por valor; apenas liberar seus arrays internos */
            free(list->clauses[i].literals);
            list->clauses[i].literals = NULL;
            list->clauses[i].size = 0;
            list->clauses[i].capacity = 0;
        }
        free(list->clauses);
        free(list);
    }
}

void clause_list_dispose_contents(clause_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->clauses[i].literals);
        list->clauses[i].literals = NULL;
        list->clauses[i].size = 0;
        list->clauses[i].capacity = 0;
    }
    free(list->clauses);
    list->clauses = NULL;
    list->count = 0;
    list->capacity = 0;
}

bool clause_list_add(clause_list_t *list, clause_t *clause) {
    if (!list || !clause) return false;
    
    /* Expandir array se necessário */
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        clause_t *new_clauses = realloc(list->clauses, new_capacity * sizeof(clause_t));
        if (!new_clauses) return false;
        
        list->clauses = new_clauses;
        list->capacity = new_capacity;
    }
    
    /* Copiar a cláusula para a lista */
    list->clauses[list->count] = *clause;
    list->count++;
    
    return true;
}

void clause_list_clear(clause_list_t *list) {
    if (list) {
        for (size_t i = 0; i < list->count; i++) {
            free(list->clauses[i].literals);
        }
        list->count = 0;
    }
}

/* ========== Funções para Fórmula CNF ========== */

cnf_formula_t* cnf_create(variable_t num_variables) {
    if (num_variables <= 0) return NULL;
    
    cnf_formula_t *cnf = malloc(sizeof(cnf_formula_t));
    if (!cnf) return NULL;
    
    /* Inicializar lista de cláusulas */
    cnf->clauses.clauses = NULL;
    cnf->clauses.count = 0;
    cnf->clauses.capacity = 0;
    
    /* Alocar array de atribuições (índice 0 não usado, variáveis começam em 1) */
    cnf->assignment = calloc(num_variables + 1, sizeof(var_assignment_t));
    if (!cnf->assignment) {
        free(cnf);
        return NULL;
    }
    
    /* Alocar array de variáveis usadas */
    cnf->variable_used = calloc(num_variables + 1, sizeof(bool));
    if (!cnf->variable_used) {
        free(cnf->assignment);
        free(cnf);
        return NULL;
    }
    
    cnf->num_variables = num_variables;
    cnf->satisfied_clauses = 0;
    
    /* Listas de ocorrências (serão inicializadas quando necessário) */
    cnf->positive_occurrences = NULL;
    cnf->negative_occurrences = NULL;
    
    return cnf;
}

void cnf_destroy(cnf_formula_t *cnf) {
    if (cnf) {
        /* cnf->clauses é embutido dentro de cnf, então não dar free na struct, apenas no conteúdo */
        clause_list_dispose_contents(&cnf->clauses);
        free(cnf->assignment);
        free(cnf->variable_used);
        
        /* Liberar listas de ocorrências se alocadas */
        if (cnf->positive_occurrences) {
            for (variable_t i = 1; i <= cnf->num_variables; i++) {
                clause_list_destroy(cnf->positive_occurrences[i]);
            }
            free(cnf->positive_occurrences);
        }
        
        if (cnf->negative_occurrences) {
            for (variable_t i = 1; i <= cnf->num_variables; i++) {
                clause_list_destroy(cnf->negative_occurrences[i]);
            }
            free(cnf->negative_occurrences);
        }
        
        free(cnf);
    }
}

bool cnf_add_clause(cnf_formula_t *cnf, clause_t *clause) {
    if (!cnf || !clause) return false;
    
    /* Inicializar lista de cláusulas se necessário */
    if (cnf->clauses.clauses == NULL) {
        cnf->clauses.capacity = 16;
        cnf->clauses.clauses = malloc(cnf->clauses.capacity * sizeof(clause_t));
        if (!cnf->clauses.clauses) return false;
        cnf->clauses.count = 0;
    }
    
    /* Marcar variáveis como usadas */
    for (size_t i = 0; i < clause->size; i++) {
        variable_t var = literal_variable(clause->literals[i]);
        if (var <= cnf->num_variables) {
            cnf->variable_used[var] = true;
        }
    }
    
    bool ok = clause_list_add(&cnf->clauses, clause);
    /* Transferência de propriedade: após copiar para a lista,
       liberar apenas o contêiner original (não os literais) */
    if (ok) {
        free(clause);
    } else {
        /* Falhou ao adicionar: destruir completamente a cláusula alocada */
        clause_destroy(clause);
    }
    return ok;
}

bool cnf_is_satisfied(const cnf_formula_t *cnf) {
    if (!cnf) return false;
    
    for (size_t i = 0; i < cnf->clauses.count; i++) {
        if (!clause_is_satisfied(&cnf->clauses.clauses[i], cnf->assignment)) {
            return false;
        }
    }
    
    return true;
}

bool cnf_has_conflict(const cnf_formula_t *cnf) {
    if (!cnf) return false;
    
    for (size_t i = 0; i < cnf->clauses.count; i++) {
        if (clause_is_conflicting(&cnf->clauses.clauses[i], cnf->assignment)) {
            return true;
        }
    }
    
    return false;
}

void cnf_update_caches(cnf_formula_t *cnf) {
    if (!cnf) return;
    
    cnf->satisfied_clauses = 0;
    
    for (size_t i = 0; i < cnf->clauses.count; i++) {
        clause_t *clause = &cnf->clauses.clauses[i];
        
        clause->is_satisfied = clause_is_satisfied(clause, cnf->assignment);
        if (clause->is_satisfied) {
            cnf->satisfied_clauses++;
        }
        
        clause->is_unit = clause_is_unit(clause, cnf->assignment, &clause->unit_literal);
    }
}

/* ========== Funções para Pilha de Atribuições ========== */

assignment_stack_t* assignment_stack_create(size_t initial_capacity) {
    assignment_stack_t *stack = malloc(sizeof(assignment_stack_t));
    if (!stack) return NULL;
    
    stack->capacity = initial_capacity > 0 ? initial_capacity : 32;
    stack->stack = malloc(stack->capacity * sizeof(assignment_entry_t));
    if (!stack->stack) {
        free(stack);
        return NULL;
    }
    
    stack->size = 0;
    stack->decision_level = 0;
    
    return stack;
}

void assignment_stack_destroy(assignment_stack_t *stack) {
    if (stack) {
        free(stack->stack);
        free(stack);
    }
}

bool assignment_stack_push(assignment_stack_t *stack, variable_t var, 
                          var_assignment_t value, bool is_decision) {
    if (!stack) return false;
    
    /* Expandir stack se necessário */
    if (stack->size >= stack->capacity) {
        size_t new_capacity = stack->capacity * 2;
        assignment_entry_t *new_stack = realloc(stack->stack, 
                                               new_capacity * sizeof(assignment_entry_t));
        if (!new_stack) return false;
        
        stack->stack = new_stack;
        stack->capacity = new_capacity;
    }
    
    if (is_decision) {
        stack->decision_level++;
    }
    
    assignment_entry_t *entry = &stack->stack[stack->size++];
    entry->variable = var;
    entry->value = value;
    entry->decision_level = stack->decision_level;
    entry->is_decision = is_decision;
    
    return true;
}

bool assignment_stack_pop(assignment_stack_t *stack, assignment_entry_t *entry) {
    if (!stack || stack->size == 0) return false;
    
    assignment_entry_t *top = &stack->stack[--stack->size];
    
    if (entry) {
        *entry = *top;
    }
    
    if (top->is_decision && stack->decision_level > 0) {
        stack->decision_level--;
    }
    
    return true;
}

void assignment_stack_backtrack_to_level(assignment_stack_t *stack, size_t level) {
    if (!stack) return;
    
    while (stack->size > 0 && stack->stack[stack->size - 1].decision_level > level) {
        assignment_entry_t entry;
        assignment_stack_pop(stack, &entry);
    }
    
    stack->decision_level = level;
}

void assignment_stack_clear(assignment_stack_t *stack) {
    if (stack) {
        stack->size = 0;
        stack->decision_level = 0;
    }
}

/* ========== Funções Utilitárias ========== */

void cnf_print_stats(const cnf_formula_t *cnf) {
    if (!cnf) return;
    
    printf("=== Estatísticas da Fórmula CNF ===\n");
    printf("Variáveis: %d\n", cnf->num_variables);
    printf("Cláusulas: %zu\n", cnf->clauses.count);
    printf("Cláusulas satisfeitas: %zu\n", cnf->satisfied_clauses);
    
    /* Contar variáveis usadas */
    int used_vars = 0;
    for (variable_t i = 1; i <= cnf->num_variables; i++) {
        if (cnf->variable_used[i]) used_vars++;
    }
    printf("Variáveis utilizadas: %d\n", used_vars);
}

void cnf_print_formula(const cnf_formula_t *cnf) {
    if (!cnf) return;
    
    printf("=== Fórmula CNF ===\n");
    for (size_t i = 0; i < cnf->clauses.count; i++) {
        const clause_t *clause = &cnf->clauses.clauses[i];
        printf("Cláusula %zu: (", i + 1);
        
        for (size_t j = 0; j < clause->size; j++) {
            if (j > 0) printf(" ∨ ");
            
            literal_t lit = clause->literals[j];
            if (lit < 0) printf("¬");
            printf("x%d", abs(lit));
        }
        
        printf(")");
        if (clause->is_satisfied) printf(" [SAT]");
        if (clause->is_unit) printf(" [UNIT: %d]", clause->unit_literal);
        printf("\n");
    }
}

bool cnf_validate_assignment(const cnf_formula_t *cnf) {
    if (!cnf) return false;
    
    for (size_t i = 0; i < cnf->clauses.count; i++) {
        if (!clause_is_satisfied(&cnf->clauses.clauses[i], cnf->assignment)) {
            printf("Cláusula %zu não satisfeita!\n", i + 1);
            return false;
        }
    }
    
    return true;
}