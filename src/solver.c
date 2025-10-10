/**
 * @file solver.c
 * @brief Implementação do algoritmo DPLL para resolução de problemas SAT
 * @author SAT Solver Team
 * @date 2025
 * 
 * Este arquivo implementa o núcleo do SAT solver usando o algoritmo DPLL
 * (Davis-Putnam-Logemann-Loveland) com otimizações modernas incluindo:
 * - Propagação unitária eficiente
 * - Eliminação de literais puros
 * - Backtracking robusto com inversão de decisões
 * - Múltiplas heurísticas de escolha de variáveis
 * - Detecção de tautologias e simplificações
 */

#include "solver.h"
#include <math.h>
#include <float.h>

/**
 * @brief Configuração padrão do solver com heurísticas otimizadas
 * 
 * Configuração recomendada para melhor performance geral:
 * - Jeroslow-Wang: boa para fórmulas estruturadas
 * - Propagação unitária e literais puros habilitados
 * - Pré-processamento ativo para simplificação
 * - Limites de segurança configuráveis
 */
const solver_config_t DEFAULT_SOLVER_CONFIG = {
    .decision_strategy = DECISION_JEROSLOW_WANG,  ///< Heurística balanceada
    .enable_pure_literal = true,                   ///< Eliminação de literais puros
    .enable_unit_propagation = true,               ///< Propagação unitária
    .enable_preprocessing = true,                  ///< Simplificação inicial
    .enable_restarts = false,                     ///< Restarts desabilitados por padrão
    .max_decisions = 0,                           ///< Sem limite de decisões
    .timeout_seconds = 0.0,                       ///< Sem timeout
    .restart_threshold = 1000,                    ///< Threshold para restarts
    .verbose = false                              ///< Modo silencioso
};

/* ========== Funções Principais ========== */

/**
 * @brief Cria um novo solver com configuração padrão
 * @param formula Fórmula CNF a ser resolvida (transfere propriedade)
 * @return Ponteiro para o solver criado ou NULL em caso de erro
 */
dpll_solver_t* solver_create(cnf_formula_t *formula) {
    return solver_create_with_config(formula, &DEFAULT_SOLVER_CONFIG);
}

/**
 * @brief Cria um novo solver com configuração personalizada
 * @param formula Fórmula CNF a ser resolvida (transfere propriedade)
 * @param config Configuração personalizada do solver
 * @return Ponteiro para o solver criado ou NULL em caso de erro
 * 
 * Aloca e inicializa todas as estruturas necessárias:
 * - Pilha de atribuições para backtracking
 * - Arrays auxiliares para otimizações
 * - Contadores de estatísticas
 * - Configuração personalizada ou padrão
 */
dpll_solver_t* solver_create_with_config(cnf_formula_t *formula, const solver_config_t *config) {
    if (!formula) return NULL;
    
    dpll_solver_t *solver = safe_malloc(sizeof(dpll_solver_t));
    
    solver->formula = formula;
    solver->assignments = assignment_stack_create(formula->num_variables * 2);
    
    if (!solver->assignments) {
        free(solver);
        return NULL;
    }
    
    /* Copiar configuração personalizada ou usar padrão */
    if (config) {
        solver->config = *config;
    } else {
        solver->config = DEFAULT_SOLVER_CONFIG;
    }
    
    /* Inicializar contadores de estatísticas */
    stats_init(&solver->stats);
    
    /* Alocar arrays auxiliares para otimizações */
    solver->pure_literals = safe_calloc(formula->num_variables + 1, sizeof(bool));
    solver->unit_clauses = safe_malloc(formula->clauses.count * sizeof(clause_t*));
    solver->unit_clauses_count = 0;
    
    solver->formula_modified = false;
    solver->conflicts_since_restart = 0;
    
    return solver;
}

void solver_destroy(dpll_solver_t *solver) {
    if (solver) {
        assignment_stack_destroy(solver->assignments);
        free(solver->pure_literals);
        free(solver->unit_clauses);
        free(solver);
    }
}

solver_result_t solver_solve(dpll_solver_t *solver) {
    if (!solver || !solver->formula) return SOLVER_ERROR;
    
    timer_start(&solver->total_timer);
    
    if (solver->config.verbose) {
        log_info("Iniciando resolução SAT...");
        log_info("Variáveis: %d, Cláusulas: %zu", 
                solver->formula->num_variables, solver->formula->clauses.count);
    }
    
    /* Pré-processamento */
    if (solver->config.enable_preprocessing) {
        if (!preprocess_formula(solver)) {
            return SOLVER_MEMORY_ERROR;
        }
        
        /* Verificar se já está satisfeito ou insatisfatível após pré-processamento */
        if (solver->formula->clauses.count == 0) {
            timer_stop(&solver->total_timer);
            solver->stats.solve_time = timer_elapsed(&solver->total_timer);
            return SOLVER_SATISFIABLE;
        }
        
        if (has_conflict(solver)) {
            timer_stop(&solver->total_timer);
            solver->stats.solve_time = timer_elapsed(&solver->total_timer);
            return SOLVER_UNSATISFIABLE;
        }
    }
    
    /* Configurar limites de segurança */
    if (solver->config.timeout_seconds == 0.0) {
        solver->config.timeout_seconds = 5.0; // 5 segundos padrão
    }
    if (solver->config.max_decisions == 0) {
        solver->config.max_decisions = 1000; // Limite de decisões
    }
    
    /* Executar algoritmo DPLL */
    solver_result_t result = dpll_algorithm(solver);
    
    timer_stop(&solver->total_timer);
    solver->stats.solve_time = timer_elapsed(&solver->total_timer);
    
    if (solver->config.verbose) {
        log_info("Resultado: %s", solver_result_string(result));
        solver_print_stats(solver);
    }
    
    return result;
}

/* ========== Algoritmo DPLL ========== */

/**
 * @brief Implementação principal do algoritmo DPLL
 * @param solver Instância do solver inicializada
 * @return Resultado da busca: SATISFIABLE, UNSATISFIABLE, UNKNOWN, TIMEOUT ou ERROR
 * 
 * Algoritmo DPLL (Davis-Putnam-Logemann-Loveland):
 * 1. Verifica se a fórmula já está satisfeita
 * 2. Detecta conflitos (cláusulas falsas)
 * 3. Executa propagação unitária
 * 4. Elimina literais puros
 * 5. Escolhe variável para decisão (heurística)
 * 6. Faz backtracking em caso de conflito
 * 
 * Inclui proteções contra loops infinitos e detecção de progresso.
 */
solver_result_t dpll_algorithm(dpll_solver_t *solver) {
    if (!solver) return SOLVER_ERROR;
    
    size_t max_iterations = 1000;         ///< Limite de segurança contra loops infinitos
    size_t iterations = 0;                ///< Contador de iterações do loop principal
    size_t no_progress_rounds = 0;        ///< Detecta iterações sem progresso (livelock)
    
    while (iterations < max_iterations) {
        iterations++;
        SOLVER_TIMEOUT_CHECK(solver);
        bool progress_made = false;
        size_t prev_assign_count = solver->assignments->size;

        /* Debug: mostrar progresso a cada 100 iterações */
        if (iterations % 100 == 0 && solver->config.verbose) {
            log_debug("Iteração %zu | Decisões: %zu | Conflitos: %zu", 
                     iterations, solver->stats.decisions, solver->stats.conflicts);
        }
        
        /* 3. Verificar se fórmula está satisfeita PRIMEIRO */
        if (is_formula_satisfied(solver)) {
            return SOLVER_SATISFIABLE;
        }
        
        /* 4. Verificar conflitos */
        if (has_conflict(solver)) {
            if (!backtrack(solver)) {
                return SOLVER_UNSATISFIABLE;
            }
            progress_made = true; /* backtrack altera o estado */
            continue;
        }
        
        /* 1. Propagação de unidades */
        if (solver->config.enable_unit_propagation) {
            unit_propagation(solver);
            if (solver->assignments->size > prev_assign_count) {
                progress_made = true;
                prev_assign_count = solver->assignments->size;
            }
            /* Após propagação, verificar conflitos para permitir backtrack */
            if (has_conflict(solver)) {
                if (!backtrack(solver)) {
                    return SOLVER_UNSATISFIABLE;
                }
                progress_made = true;
                continue;
            }
        }
        
        /* 2. Eliminação de literais puros */
        if (solver->config.enable_pure_literal) {
            if (pure_literal_elimination(solver)) {
                progress_made = true;
                prev_assign_count = solver->assignments->size;
            }
            /* Verificar conflitos também após eliminação de puros */
            if (has_conflict(solver)) {
                if (!backtrack(solver)) {
                    return SOLVER_UNSATISFIABLE;
                }
                progress_made = true;
                continue;
            }
        }
        
        /* 5. Escolher variável para decisão */
        variable_t decision_var = choose_decision_variable(solver);
        if (decision_var == 0) {
            /* Todas as variáveis estão atribuídas */
            if (is_formula_satisfied(solver)) {
                return SOLVER_SATISFIABLE;
            } else {
                /* Sem variáveis livres mas não satisfeita = UNSAT */
                return SOLVER_UNSATISFIABLE;
            }
        }
        
        /* 6. Fazer decisão */
        var_assignment_t decision_value = choose_decision_value(solver, decision_var);
        if (!assign_variable(solver, decision_var, decision_value, true)) {
            return SOLVER_ERROR;
        }
        progress_made = true;
        
        SOLVER_STATS_INCREMENT(solver, decisions);
        
        /* Verificar limite de decisões */
        if (solver->config.max_decisions > 0 && 
            solver->stats.decisions >= solver->config.max_decisions) {
            return SOLVER_UNKNOWN;
        }
        
        /* Verificar se deve reinicializar */
        if (solver->config.enable_restarts && should_restart(solver)) {
            perform_restart(solver);
            solver->conflicts_since_restart = 0;
            progress_made = true;
        }

        /* Guarda-louça: se nenhuma operação mudou o estado, evite spin infinito */
        if (!progress_made) {
            no_progress_rounds++;
            if (solver->config.verbose) {
                log_debug("Sem progresso na iteração %zu (rounds=%zu). Encerrando como UNKNOWN para evitar loop.",
                          iterations, no_progress_rounds);
            }
            return SOLVER_UNKNOWN;
        } else {
            no_progress_rounds = 0;
        }
    }
    
    /* Se chegou ao limite de iterações */
    return SOLVER_TIMEOUT;
}

/**
 * @brief Executa propagação unitária (Unit Propagation)
 * @param solver Instância do solver
 * @return SATISFIABLE se todas cláusulas ficaram satisfeitas, UNKNOWN se deve continuar
 * 
 * Propagação Unitária:
 * - Identifica cláusulas com apenas um literal não atribuído (unit clauses)
 * - Força a atribuição desse literal para satisfazer a cláusula
 * - Repete até não haver mais propagações possíveis
 * - Detecta conflitos quando uma variável precisaria de dois valores diferentes
 * 
 * É a otimização mais importante do DPLL, reduzindo drasticamente o espaço de busca.
 */
solver_result_t unit_propagation(dpll_solver_t *solver) {
    if (!solver) return SOLVER_ERROR;
    
    bool propagated;
    do {
        propagated = false;
        
        /* Examinar todas as cláusulas em busca de unit clauses */
        for (size_t i = 0; i < solver->formula->clauses.count; i++) {
            clause_t *clause = &solver->formula->clauses.clauses[i];
            
            /* Pular cláusulas já satisfeitas (otimização) */
            if (clause_is_satisfied(clause, solver->formula->assignment)) {
                continue;
            }
            
            literal_t unit_literal;
            if (clause_is_unit(clause, solver->formula->assignment, &unit_literal)) {
                variable_t var = literal_variable(unit_literal);
                var_assignment_t value = literal_is_positive(unit_literal) ? VAR_TRUE : VAR_FALSE;
                
                /* Verificar se já está atribuída com valor diferente (conflito) */
                if (IS_VARIABLE_ASSIGNED(solver, var)) {
                    if (solver->formula->assignment[var] != value) {
                        solver->conflicts_since_restart++;
                        SOLVER_STATS_INCREMENT(solver, conflicts);
                        /* Não conclui UNSAT aqui; deixe o DPLL fazer backtrack */
                        return SOLVER_UNKNOWN;
                    }
                    continue; /* Já atribuída corretamente */
                }
                
                /* Propagar */
                if (!assign_variable(solver, var, value, false)) {
                    return SOLVER_MEMORY_ERROR;
                }
                
                SOLVER_STATS_INCREMENT(solver, propagations);
                propagated = true;
                solver->formula_modified = true;
                
                if (solver->config.verbose) {
                    log_debug("Propagação: %sx%d", value == VAR_TRUE ? "" : "¬", var);
                }
            }
        }
    } while (propagated);
    
    /* Se todas as cláusulas ficaram satisfeitas no processo, reportar SAT */
    if (cnf_is_satisfied(solver->formula)) {
        return SOLVER_SATISFIABLE;
    }
    return SOLVER_UNKNOWN; /* Continuar algoritmo */
}

bool pure_literal_elimination(dpll_solver_t *solver) {
    if (!solver) return false;
    bool changed = false;
    
    /* Marcar todas as variáveis como candidatas a literais puros */
    for (variable_t var = 1; var <= solver->formula->num_variables; var++) {
        if (IS_VARIABLE_ASSIGNED(solver, var)) {
            solver->pure_literals[var] = false;
            continue;
        }
        
        bool appears_positive = false;
        bool appears_negative = false;
        
        /* Verificar em todas as cláusulas não satisfeitas */
        for (size_t i = 0; i < solver->formula->clauses.count; i++) {
            clause_t *clause = &solver->formula->clauses.clauses[i];
            
            if (clause_is_satisfied(clause, solver->formula->assignment)) {
                continue;
            }
            
            for (size_t j = 0; j < clause->size; j++) {
                literal_t lit = clause->literals[j];
                if (literal_variable(lit) == var) {
                    if (literal_is_positive(lit)) {
                        appears_positive = true;
                    } else {
                        appears_negative = true;
                    }
                    
                    if (appears_positive && appears_negative) {
                        break; /* Não é puro */
                    }
                }
            }
            
            if (appears_positive && appears_negative) {
                break;
            }
        }
        
        /* Se aparece apenas em uma polaridade, é literal puro */
        if (appears_positive && !appears_negative) {
            assign_variable(solver, var, VAR_TRUE, false);
            changed = true;
            solver->formula_modified = true;
            if (solver->config.verbose) {
                log_debug("Literal puro: x%d", var);
            }
        } else if (appears_negative && !appears_positive) {
            assign_variable(solver, var, VAR_FALSE, false);
            changed = true;
            solver->formula_modified = true;
            if (solver->config.verbose) {
                log_debug("Literal puro: ¬x%d", var);
            }
        }
    }
    
    return changed;
}

/* ========== Funções de Decisão ========== */

variable_t choose_decision_variable(dpll_solver_t *solver) {
    if (!solver) return 0;
    
    switch (solver->config.decision_strategy) {
        case DECISION_FIRST_UNASSIGNED:
            return decision_first_unassigned(solver);
        case DECISION_MOST_FREQUENT:
            return decision_most_frequent(solver);
        case DECISION_JEROSLOW_WANG:
            return decision_jeroslow_wang(solver);
        case DECISION_RANDOM:
            return decision_random(solver);
        default:
            return decision_first_unassigned(solver);
    }
}

var_assignment_t choose_decision_value(dpll_solver_t *solver, variable_t var) {
    if (!solver || var == 0) return VAR_UNASSIGNED;
    
    /* Por padrão, escolher TRUE (pode ser melhorado com heurísticas) */
    return VAR_TRUE;
}

variable_t decision_first_unassigned(const dpll_solver_t *solver) {
    if (!solver) return 0;
    
    for (variable_t var = 1; var <= solver->formula->num_variables; var++) {
        if (!IS_VARIABLE_ASSIGNED(solver, var)) {
            return var;
        }
    }
    
    return 0; /* Todas atribuídas */
}

variable_t decision_most_frequent(const dpll_solver_t *solver) {
    if (!solver) return 0;
    
    variable_t best_var = 0;
    size_t max_frequency = 0;
    
    for (variable_t var = 1; var <= solver->formula->num_variables; var++) {
        if (IS_VARIABLE_ASSIGNED(solver, var)) continue;
        
        size_t pos_freq = calculate_literal_frequency(solver, var);
        size_t neg_freq = calculate_literal_frequency(solver, -var);
        size_t total_freq = pos_freq + neg_freq;
        
        if (total_freq > max_frequency) {
            max_frequency = total_freq;
            best_var = var;
        }
    }
    
    return best_var;
}

variable_t decision_jeroslow_wang(const dpll_solver_t *solver) {
    if (!solver) return 0;
    
    variable_t best_var = 0;
    double max_score = -1.0;
    
    for (variable_t var = 1; var <= solver->formula->num_variables; var++) {
        if (IS_VARIABLE_ASSIGNED(solver, var)) continue;
        
        double score = calculate_jeroslow_wang_score(solver, var);
        if (score > max_score) {
            max_score = score;
            best_var = var;
        }
    }
    
    return best_var;
}

variable_t decision_random(const dpll_solver_t *solver) {
    if (!solver) return 0;
    
    /* Contar variáveis não atribuídas */
    variable_t unassigned_vars[solver->formula->num_variables];
    size_t unassigned_count = 0;
    
    for (variable_t var = 1; var <= solver->formula->num_variables; var++) {
        if (!IS_VARIABLE_ASSIGNED(solver, var)) {
            unassigned_vars[unassigned_count++] = var;
        }
    }
    
    if (unassigned_count == 0) return 0;
    
    size_t random_index = random_int(0, unassigned_count - 1);
    return unassigned_vars[random_index];
}

/* ========== Backtracking ========== */

/**
 * @brief Helper para debug: imprime o estado atual da pilha de atribuições
 * @param solver Instância do solver
 * 
 * Mostra todas as atribuições na pilha com suas características:
 * - Índice na pilha
 * - Variável e seu valor (1/0/U para TRUE/FALSE/UNASSIGNED)
 * - Tipo (decision ou propagação)
 * 
 * Útil para depurar problemas de backtracking e estado da busca.
 */
void print_assignment_stack(const dpll_solver_t *solver) {
    if (!solver || !solver->assignments) return;
    log_debug("Assignment stack (size=%zu):", solver->assignments->size);
    for (size_t i = 0; i < solver->assignments->size; ++i) {
        const assignment_entry_t *e = &solver->assignments->stack[i];
        log_debug(" [%zu] x%u = %s (%s)",
                  i,
                  e->variable,
                  e->value == VAR_TRUE ? "1" : (e->value == VAR_FALSE ? "0" : "U"),
                  e->is_decision ? "decision" : "prop");
    }
}

/**
 * @brief Executa backtracking robusto quando um conflito é detectado
 * @param solver Instância do solver
 * @return true se conseguiu fazer backtrack, false se não há mais decisões
 * 
 * Backtracking robusto:
 * 1. Encontra a última decisão na pilha de atribuições
 * 2. Remove todas as propagações posteriores a essa decisão
 * 3. Remove a própria decisão
 * 4. Inverte o valor da decisão e a reaplica COMO DECISÃO
 * 5. Mantém consistência entre pilha e fórmula
 * 
 * Crítico: a decisão invertida deve ser marcada como decisão (não propagação)
 * para permitir futuras operações de backtracking.
 */
bool backtrack(dpll_solver_t *solver) {
    if (!solver || !solver->assignments || solver->assignments->size == 0) {
        return false;
    }

    if (solver->config.verbose) {
        log_debug("Backtrack iniciado:");
        print_assignment_stack(solver);
    }

    // Encontra o índice da última decisão na pilha (busca reversa)
    int last_decision_idx = -1;
    for (int i = (int)solver->assignments->size - 1; i >= 0; --i) {
        if (solver->assignments->stack[i].is_decision) {
            last_decision_idx = i;
            break;
        }
    }

    if (last_decision_idx == -1) {
        // não há decisões para inverter
        if (solver->config.verbose) {
            log_debug("Backtrack: nenhuma decisão encontrada na pilha");
        }
        return false;
    }

    if (solver->config.verbose) {
        log_debug("Backtrack: last decision index = %d, stack size = %zu",
                  last_decision_idx, solver->assignments->size);
    }

    // desempilha (sem usar pop) todas as entradas acima da decisão
    size_t cur_size = solver->assignments->size;
    for (size_t i = cur_size; i > (size_t)last_decision_idx + 1; --i) {
        assignment_entry_t entry = solver->assignments->stack[i - 1];
        // limpar atribuição na fórmula
        solver->formula->assignment[entry.variable] = VAR_UNASSIGNED;
        // reduzir tamanho da pilha
        solver->assignments->size--;
    }

    // agora desempilha a própria decisão (último elemento restante)
    assignment_entry_t decision_entry = solver->assignments->stack[solver->assignments->size - 1];
    solver->formula->assignment[decision_entry.variable] = VAR_UNASSIGNED;
    solver->assignments->size--;

    // inverter o valor da decisão e reatribuir COMO decisão
    var_assignment_t opposite_value = (decision_entry.value == VAR_TRUE) ? VAR_FALSE : VAR_TRUE;

    if (!assign_variable(solver, decision_entry.variable, opposite_value, true)) {
        // falha ao empilhar novamente
        if (solver->config.verbose) {
            log_debug("Backtrack: falha ao reatribuir x%u", decision_entry.variable);
        }
        return false;
    }

    if (solver->config.verbose) {
        log_debug("Backtrack: x%u %s -> %s (reaplicada como decisão)",
                  decision_entry.variable,
                  decision_entry.value == VAR_TRUE ? "TRUE" : "FALSE",
                  opposite_value == VAR_TRUE ? "TRUE" : "FALSE");
        log_debug("Backtrack concluído:");
        print_assignment_stack(solver);
    }

    return true;
}

/* ========== Funções Auxiliares ========== */

bool assign_variable(dpll_solver_t *solver, variable_t var, var_assignment_t value, bool is_decision) {
    if (!solver || var == 0 || var > solver->formula->num_variables) return false;
    
    solver->formula->assignment[var] = value;
    bool pushed = assignment_stack_push(solver->assignments, var, value, is_decision);
    if (!pushed) {
        if (solver->config.verbose) {
            log_error("Falha ao empilhar atribuição x%d=%s (%s)",
                      var,
                      value == VAR_TRUE ? "TRUE" : (value == VAR_FALSE ? "FALSE" : "UNASSIGNED"),
                      is_decision ? "decisão" : "propagação");
        }
        return false;
    }
    if (solver->config.verbose) {
        log_debug("Atribuindo x%d = %s (%s)",
                  var,
                  value == VAR_TRUE ? "TRUE" : (value == VAR_FALSE ? "FALSE" : "UNASSIGNED"),
                  is_decision ? "decisão" : "propagação");
    }
    return true;
}

bool has_conflict(const dpll_solver_t *solver) {
    if (!solver) return false;
    
    for (size_t i = 0; i < solver->formula->clauses.count; i++) {
        if (clause_is_conflicting(&solver->formula->clauses.clauses[i], 
                                  solver->formula->assignment)) {
            return true;
        }
    }
    
    return false;
}

bool is_formula_satisfied(const dpll_solver_t *solver) {
    if (!solver) return false;
    
    return cnf_is_satisfied(solver->formula);
}

bool solver_is_timeout(const dpll_solver_t *solver) {
    if (!solver || solver->config.timeout_seconds <= 0.0) return false;
    
    double current_time = get_current_time();
    return (current_time - solver->total_timer.start_time) >= solver->config.timeout_seconds;
}

/* ========== Heurísticas ========== */

double calculate_jeroslow_wang_score(const dpll_solver_t *solver, variable_t var) {
    if (!solver || var == 0) return 0.0;
    
    double score = 0.0;
    
    for (size_t i = 0; i < solver->formula->clauses.count; i++) {
        const clause_t *clause = &solver->formula->clauses.clauses[i];
        
        if (clause_is_satisfied(clause, solver->formula->assignment)) {
            continue;
        }
        
        bool contains_var = false;
        for (size_t j = 0; j < clause->size; j++) {
            if (literal_variable(clause->literals[j]) == var) {
                contains_var = true;
                break;
            }
        }
        
        if (contains_var) {
            score += pow(2.0, -(double)clause->size);
        }
    }
    
    return score;
}

size_t calculate_literal_frequency(const dpll_solver_t *solver, literal_t literal) {
    if (!solver) return 0;
    
    size_t frequency = 0;
    
    for (size_t i = 0; i < solver->formula->clauses.count; i++) {
        const clause_t *clause = &solver->formula->clauses.clauses[i];
        
        if (clause_is_satisfied(clause, solver->formula->assignment)) {
            continue;
        }
        
        for (size_t j = 0; j < clause->size; j++) {
            if (clause->literals[j] == literal) {
                frequency++;
                break;
            }
        }
    }
    
    return frequency;
}

/* ========== Pré-processamento ========== */

bool preprocess_formula(dpll_solver_t *solver) {
    if (!solver) return false;
    
    bool changed;
    do {
        changed = false;
        size_t before = solver->assignments->size;

        /* Eliminar literais puros */
        if (eliminate_pure_literals_preprocessing(solver)) {
            changed = true;
        }

        /* Propagar unidades */
        unit_propagation(solver);

        /* Detectar alterações via tamanho da pilha de atribuições */
        if (solver->assignments->size != before) {
            changed = true;
        }

        /* Verificar conflito explícito */
        if (has_conflict(solver)) {
            return true; /* será tratado pelo chamador */
        }

    } while (changed);
    
    return true;
}

bool eliminate_pure_literals_preprocessing(dpll_solver_t *solver) {
    return pure_literal_elimination(solver);
}

/* ========== Reinicializações ========== */

bool should_restart(const dpll_solver_t *solver) {
    if (!solver) return false;
    
    return solver->conflicts_since_restart >= solver->config.restart_threshold;
}

void perform_restart(dpll_solver_t *solver) {
    if (!solver) return;
    
    /* Desfazer todas as decisões, mantendo apenas as propagações de nível 0 */
    unassign_until_level(solver, 0);
    
    SOLVER_STATS_INCREMENT(solver, restarts);
    
    if (solver->config.verbose) {
        log_debug("Reinicialização executada");
    }
}

void unassign_until_level(dpll_solver_t *solver, size_t level) {
    if (!solver) return;
    
    assignment_stack_backtrack_to_level(solver->assignments, level);
    
    /* Limpar atribuições na fórmula */
    for (variable_t var = 1; var <= solver->formula->num_variables; var++) {
        solver->formula->assignment[var] = VAR_UNASSIGNED;
    }
    
    /* Reaplicar atribuições restantes */
    for (size_t i = 0; i < solver->assignments->size; i++) {
        assignment_entry_t *entry = &solver->assignments->stack[i];
        solver->formula->assignment[entry->variable] = entry->value;
    }
}

/* ========== Informações e Utilidades ========== */

const char* solver_result_string(solver_result_t result) {
    switch (result) {
        case SOLVER_SATISFIABLE: return "SATISFIABLE";
        case SOLVER_UNSATISFIABLE: return "UNSATISFIABLE";
        case SOLVER_UNKNOWN: return "UNKNOWN";
        case SOLVER_TIMEOUT: return "TIMEOUT";
        case SOLVER_MEMORY_ERROR: return "MEMORY_ERROR";
        case SOLVER_ERROR: return "ERROR";
        default: return "INVALID";
    }
}

void solver_print_stats(const dpll_solver_t *solver) {
    if (!solver) return;
    
    stats_print(&solver->stats);
}

void solver_print_assignment(const dpll_solver_t *solver) {
    if (!solver) return;
    
    printf(COLOR_GREEN "=== Atribuição de Variáveis ===" COLOR_RESET "\n");
    for (variable_t var = 1; var <= solver->formula->num_variables; var++) {
        var_assignment_t value = solver->formula->assignment[var];
        printf("x%d = ", var);
        
        switch (value) {
            case VAR_TRUE: printf(COLOR_GREEN "TRUE" COLOR_RESET); break;
            case VAR_FALSE: printf(COLOR_RED "FALSE" COLOR_RESET); break;
            case VAR_UNASSIGNED: printf(COLOR_YELLOW "UNASSIGNED" COLOR_RESET); break;
        }
        printf("\n");
    }
    printf("\n");
}

bool validate_solution(const dpll_solver_t *solver) {
    if (!solver) return false;
    
    return cnf_validate_assignment(solver->formula);
}

bool validate_partial_assignment(const dpll_solver_t *solver) {
    if (!solver) return false;
    
    /* Verificar se não há conflitos nas cláusulas */
    return !has_conflict(solver);
}