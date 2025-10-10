/**
 * @file main.c
 * @brief Interface principal do SAT Solver - argumentos CLI e execução
 * @author SAT Solver Team
 * @date 2025
 * 
 * Este arquivo implementa a interface de linha de comando do SAT solver,
 * incluindo parsing de argumentos, execução do solver e formatação da saída.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "parser.h"
#include "solver.h"
#include "utils.h"

/* Declaração antecipada da função parse_double */
bool parse_double(const char *str, double *result);

/**
 * @brief Imprime o modelo SAT no formato da aula (uma variável por linha)
 * @param formula Fórmula CNF com atribuições resolvidas
 * 
 * Formato de saída: "1 = 1\n2 = 0\n3 = 1\n"
 * Variáveis UNASSIGNED são tratadas como 0 (FALSE)
 */
static void print_class_model_line(const cnf_formula_t *formula) {
    if (!formula || !formula->assignment) return;
    
    for (variable_t var = 1; var <= formula->num_variables; var++) {
        var_assignment_t val = formula->assignment[var];
        int bit = (val == VAR_TRUE) ? 1 : 0; /* UNASSIGNED tratado como 0 */
        printf("%d = %d\n", var, bit);
    }
}

/**
 * @brief Estrutura para armazenar argumentos da linha de comando
 */
typedef struct {
    char *input_file;                   ///< Caminho para arquivo CNF de entrada
    bool verbose;                       ///< Flag para modo verboso (logs detalhados)
    bool show_assignment;               ///< Flag para mostrar atribuição das variáveis
    bool show_stats;                    ///< Flag para mostrar estatísticas de performance
    bool help;                          ///< Flag para mostrar ajuda
    decision_strategy_t strategy;       ///< Estratégia de escolha de variáveis
    double timeout;                     ///< Timeout em segundos (0 = sem limite)
    size_t max_decisions;              ///< Máximo de decisões (0 = sem limite)
} cmd_args_t;

/**
 * @brief Imprime a ajuda do programa com todas as opções disponíveis
 * @param program_name Nome do programa (argv[0])
 */
void print_help(const char *program_name) {
    printf("SAT Solver em C - Algoritmo DPLL\n\n");
    printf("Uso: %s [opções] <arquivo.cnf>\n\n", program_name);
    printf("Opções:\n");
    printf("  -h, --help           Mostrar esta ajuda\n");
    printf("  -v, --verbose        Modo verboso\n");
    printf("  -a, --assignment     Mostrar atribuição das variáveis\n");
    printf("  -s, --stats          Mostrar estatísticas detalhadas\n");
    printf("  -t, --timeout <seg>  Timeout em segundos (padrão: sem limite)\n");
    printf("  -d, --decisions <n>  Máximo de decisões (padrão: sem limite)\n");
    printf("  --strategy <tipo>    Estratégia de decisão:\n");
    printf("                       first    - Primeira não atribuída (padrão)\n");
    printf("                       frequent - Mais frequente\n");
    printf("                       jw       - Jeroslow-Wang\n");
    printf("                       random   - Aleatória\n");
    printf("\n");
    printf("Formato de entrada: DIMACS CNF\n");
    printf("Código de saída:\n");
    printf("  10 - SATISFIABLE\n");
    printf("  20 - UNSATISFIABLE\n");
    printf("  0  - UNKNOWN/TIMEOUT\n");
    printf("  1  - ERRO\n");
    printf("\n");
    printf("Exemplos:\n");
    printf("  %s exemplo.cnf\n", program_name);
    printf("  %s -v -s --strategy jw problema.cnf\n", program_name);
    printf("  %s --timeout 60 --decisions 10000 formula.cnf\n", program_name);
}

/* Função para parsear argumentos da linha de comando */
bool parse_arguments(int argc, char *argv[], cmd_args_t *args) {
    /* Inicializar com valores padrão */
    memset(args, 0, sizeof(cmd_args_t));
    args->strategy = DECISION_FIRST_UNASSIGNED;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            args->help = true;
            return true;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            args->verbose = true;
        }
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--assignment") == 0) {
            args->show_assignment = true;
        }
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0) {
            args->show_stats = true;
        }
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if (i + 1 >= argc) {
                log_error("Opção %s requer um valor", argv[i]);
                return false;
            }
            if (!parse_double(argv[++i], &args->timeout) || args->timeout < 0) {
                log_error("Timeout inválido: %s", argv[i]);
                return false;
            }
        }
        else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--decisions") == 0) {
            if (i + 1 >= argc) {
                log_error("Opção %s requer um valor", argv[i]);
                return false;
            }
            long decisions;
            if (!parse_long(argv[++i], &decisions) || decisions < 0) {
                log_error("Número de decisões inválido: %s", argv[i]);
                return false;
            }
            args->max_decisions = (size_t)decisions;
        }
        else if (strcmp(argv[i], "--strategy") == 0) {
            if (i + 1 >= argc) {
                log_error("Opção --strategy requer um valor");
                return false;
            }
            char *strategy = argv[++i];
            if (strcmp(strategy, "first") == 0) {
                args->strategy = DECISION_FIRST_UNASSIGNED;
            } else if (strcmp(strategy, "frequent") == 0) {
                args->strategy = DECISION_MOST_FREQUENT;
            } else if (strcmp(strategy, "jw") == 0) {
                args->strategy = DECISION_JEROSLOW_WANG;
            } else if (strcmp(strategy, "random") == 0) {
                args->strategy = DECISION_RANDOM;
            } else {
                log_error("Estratégia desconhecida: %s", strategy);
                return false;
            }
        }
        else if (argv[i][0] == '-') {
            log_error("Opção desconhecida: %s", argv[i]);
            return false;
        }
        else {
            /* Arquivo de entrada */
            if (args->input_file) {
                log_error("Múltiplos arquivos de entrada especificados");
                return false;
            }
            args->input_file = argv[i];
        }
    }
    
    return true;
}

/* Função para validar argumentos */
bool validate_arguments(const cmd_args_t *args) {
    if (args->help) return true;
    
    if (!args->input_file) {
        log_error("Arquivo de entrada não especificado");
        return false;
    }
    
    if (!file_exists(args->input_file)) {
        log_error("Arquivo não encontrado: %s", args->input_file);
        return false;
    }
    
    return true;
}

/* Função para converter estratégia para string */
const char* strategy_to_string(decision_strategy_t strategy) {
    switch (strategy) {
        case DECISION_FIRST_UNASSIGNED: return "first-unassigned";
        case DECISION_MOST_FREQUENT: return "most-frequent";
        case DECISION_JEROSLOW_WANG: return "jeroslow-wang";
        case DECISION_RANDOM: return "random";
        default: return "unknown";
    }
}

/* Função principal */
int main(int argc, char *argv[]) {
    cmd_args_t args;
    
    /* Inicializar gerador aleatório */
    random_seed((unsigned int)time(NULL));
    
    /* Parsear argumentos */
    if (!parse_arguments(argc, argv, &args)) {
        return 1;
    }
    
    /* Mostrar ajuda se solicitado */
    if (args.help) {
        print_help(argv[0]);
        return 0;
    }
    
    /* Validar argumentos */
    if (!validate_arguments(&args)) {
        fprintf(stderr, "Use %s --help para ver as opções disponíveis\n", argv[0]);
        return 1;
    }
    
    if (args.verbose) {
        log_info("SAT Solver iniciado");
        log_info("Arquivo: %s", args.input_file);
        log_info("Estratégia: %s", strategy_to_string(args.strategy));
        if (args.timeout > 0) {
            log_info("Timeout: %.2f segundos", args.timeout);
        }
        if (args.max_decisions > 0) {
            log_info("Máximo de decisões: %zu", args.max_decisions);
        }
    }
    
    /* Parsear arquivo CNF */
    cnf_parser_t *parser = parser_create(false, args.verbose);
    if (!parser) {
        log_error("Erro ao criar parser");
        return 1;
    }
    
    parse_result_t parse_result = parser_parse_file(parser, args.input_file);
    if (parse_result != PARSE_OK) {
        log_error("Erro no parsing: %s", parser_error_string(parse_result));
        if (parser->info.error_message[0] != '\0') {
            log_error("Detalhes: %s", parser->info.error_message);
        }
        parser_destroy(parser);
        return 1;
    }
    
    cnf_formula_t *formula = parser->formula;
    parser->formula = NULL; /* Transferir propriedade */
    parser_destroy(parser);
    
    if (args.verbose) {
        log_info("Parsing concluído com sucesso");
        cnf_print_stats(formula);
    }
    
    /* Configurar solver */
    solver_config_t config = DEFAULT_SOLVER_CONFIG;
    config.decision_strategy = args.strategy;
    config.verbose = args.verbose;
    config.timeout_seconds = args.timeout;
    config.max_decisions = args.max_decisions;
    
    /* Criar e executar solver */
    dpll_solver_t *solver = solver_create_with_config(formula, &config);
    if (!solver) {
        log_error("Erro ao criar solver");
        cnf_destroy(formula);
        return 1;
    }
    
    if (args.verbose) {
        log_info("Iniciando resolução...");
    }
    
    solver_result_t result = solver_solve(solver);
    
    /* Imprimir resultado no formato esperado pela aula/PDF */
    const char *status_str = NULL;
    switch (result) {
        case SOLVER_SATISFIABLE: status_str = "SATISFIABLE"; break;
        case SOLVER_UNSATISFIABLE: status_str = "UNSATISFIABLE"; break;
        case SOLVER_UNKNOWN: status_str = "UNKNOWN"; break;
        case SOLVER_TIMEOUT: status_str = "UNKNOWN"; break; /* mapear TIMEOUT como UNKNOWN */
        default: status_str = "UNKNOWN"; break;
    }
    printf("s %s\n", status_str);
    if (result == SOLVER_SATISFIABLE) {
        print_class_model_line(formula);
    }
    
    if (args.show_stats || args.verbose) {
        solver_print_stats(solver);
    }
    
    if (args.show_assignment && result == SOLVER_SATISFIABLE) {
        /* Saída alternativa (humana) se solicitado: imprime tabela colorida */
        solver_print_assignment(solver);
        /* Validar solução */
        if (validate_solution(solver)) {
            if (args.verbose) {
                log_info("Solução validada com sucesso");
            }
        } else {
            log_error("ERRO: Solução inválida!");
        }
    }
    
    /* Limpar recursos */
    solver_destroy(solver);
    cnf_destroy(formula);
    
    /* Retornar código de saída apropriado */
    switch (result) {
        case SOLVER_SATISFIABLE:
            return 10;
        case SOLVER_UNSATISFIABLE:
            return 20;
        case SOLVER_UNKNOWN:
        case SOLVER_TIMEOUT:
            return 0;
        default:
            return 1;
    }
}