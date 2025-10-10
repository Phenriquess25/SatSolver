/**
 * @file parser.c
 * @brief Parser para arquivos no formato DIMACS CNF
 * @author SAT Solver Team
 * @date 2025
 * 
 * Este arquivo implementa um parser robusto para o formato padrão DIMACS CNF:
 * - Suporte completo à especificação DIMACS
 * - Detecção e remoção automática de tautologias
 * - Validação rigorosa de formato
 * - Tratamento de erros com mensagens detalhadas
 * - Configurações flexíveis (modo estrito/permissivo)
 */

#include "parser.h"
#include "utils.h"
#include <string.h>
#include <ctype.h>

/**
 * @brief Configuração padrão do parser (modo permissivo)
 * 
 * Configuração balanceada que aceita:
 * - Tautologias (removidas automaticamente)
 * - Literais duplicados (limpos automaticamente)
 * - Formato DIMACS padrão com tolerância a variações menores
 */
const parser_config_t DEFAULT_PARSER_CONFIG = {
    .allow_empty_clauses = false,        ///< Cláusulas vazias são inválidas
    .allow_tautologies = true,           ///< Aceita mas remove tautologias
    .allow_duplicate_literals = true,    ///< Aceita mas remove duplicatas
    .ignore_extra_clauses = false,       ///< Respeita contagem declarada
    .require_sorted_literals = false,    ///< Não exige literais ordenados
    .max_clause_size = 0,               ///< Sem limite de tamanho
    .max_line_length = MAX_LINE_LENGTH   ///< Limite de linha configurável
};

/* ========== Funções Principais do Parser ========== */

/**
 * @brief Cria uma nova instância do parser DIMACS
 * @param strict_mode Se true, usa validação rigorosa
 * @param verbose Se true, mostra logs detalhados do parsing
 * @return Ponteiro para o parser criado ou NULL em caso de erro
 */
cnf_parser_t* parser_create(bool strict_mode, bool verbose) {
    cnf_parser_t *parser = safe_malloc(sizeof(cnf_parser_t));
    
    parser->info.line_number = 0;
    parser->info.expected_clauses = 0;
    parser->info.parsed_clauses = 0;
    parser->info.max_variables = 0;
    parser->info.error_message[0] = '\0';
    
    parser->formula = NULL;
    parser->strict_mode = strict_mode;
    parser->verbose = verbose;
    
    return parser;
}

cnf_parser_t* parser_create_with_config(const parser_config_t *config) {
    cnf_parser_t *parser = parser_create(false, false);
    if (parser && config) {
        /* Configuração seria armazenada aqui se necessário */
        (void)config; /* Por enquanto não usado */
    }
    return parser;
}

void parser_destroy(cnf_parser_t *parser) {
    if (parser) {
        if (parser->formula) {
            cnf_destroy(parser->formula);
        }
        free(parser);
    }
}

void parser_reset(cnf_parser_t *parser) {
    if (parser) {
        if (parser->formula) {
            cnf_destroy(parser->formula);
            parser->formula = NULL;
        }
        
        parser->info.line_number = 0;
        parser->info.expected_clauses = 0;
        parser->info.parsed_clauses = 0;
        parser->info.max_variables = 0;
        parser->info.error_message[0] = '\0';
    }
}

/* ========== Funções de Parsing ========== */

parse_result_t parser_parse_file(cnf_parser_t *parser, const char *filename) {
    if (!parser || !filename) return PARSE_ERROR_INVALID_FORMAT;
    
    if (!file_exists(filename)) {
        snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                "Arquivo não encontrado: %s", filename);
        return PARSE_ERROR_FILE_NOT_FOUND;
    }
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                "Não foi possível abrir o arquivo: %s", filename);
        return PARSE_ERROR_FILE_NOT_FOUND;
    }
    
    if (parser->verbose) {
        log_info("Parsing arquivo: %s", filename);
    }
    
    parse_result_t result = parser_parse_stream(parser, file);
    fclose(file);
    
    return result;
}

parse_result_t parser_parse_string(cnf_parser_t *parser, const char *content) {
    if (!parser || !content) return PARSE_ERROR_INVALID_FORMAT;
    
    /* Criar um stream temporário da string */
    FILE *stream = tmpfile();
    if (!stream) return PARSE_ERROR_MEMORY;
    
    fprintf(stream, "%s", content);
    rewind(stream);
    
    parse_result_t result = parser_parse_stream(parser, stream);
    fclose(stream);
    
    return result;
}

parse_result_t parser_parse_stream(cnf_parser_t *parser, FILE *stream) {
    if (!parser || !stream) return PARSE_ERROR_INVALID_FORMAT;
    
    parser_reset(parser);
    
    char line[MAX_LINE_LENGTH];
    bool problem_line_found = false;
    timer_t timer;
    timer_start(&timer);
    
    while (fgets(line, sizeof(line), stream)) {
        parser->info.line_number++;
        
        /* Remover quebra de linha */
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        char *trimmed_line = trim_string(line);
        
        /* Pular linhas vazias */
        if (is_empty_line(trimmed_line)) {
            continue;
        }
        
        /* Processar comentários */
        if (is_comment_line(trimmed_line)) {
            if (parser->verbose) {
                log_debug("Comentário na linha %d: %s", parser->info.line_number, trimmed_line);
            }
            continue;
        }
        
        /* Processar linha de definição do problema */
        if (is_problem_line(trimmed_line)) {
            if (problem_line_found) {
                snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                        "Múltiplas linhas de definição encontradas");
                return PARSE_ERROR_INVALID_PROBLEM_LINE;
            }
            
            int num_vars, num_clauses;
            parse_result_t result = parse_problem_line(trimmed_line, &num_vars, &num_clauses);
            if (result != PARSE_OK) {
                snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                        "Linha de definição inválida: %s", trimmed_line);
                return result;
            }
            
            parser->info.max_variables = num_vars;
            parser->info.expected_clauses = num_clauses;
            problem_line_found = true;
            
            /* Criar a fórmula CNF */
            parser->formula = cnf_create(num_vars);
            if (!parser->formula) {
                return PARSE_ERROR_MEMORY;
            }
            
            if (parser->verbose) {
                log_info("Problema: %d variáveis, %d cláusulas", num_vars, num_clauses);
            }
            
            continue;
        }
        
        /* Processar cláusulas */
        if (!problem_line_found) {
            snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                    "Linha de definição 'p cnf' não encontrada antes das cláusulas");
            return PARSE_ERROR_NO_PROBLEM_LINE;
        }
        
        clause_t *clause = clause_create(8);
        if (!clause) return PARSE_ERROR_MEMORY;
        
        parse_result_t result = parse_clause_line(trimmed_line, clause, parser->info.max_variables);
        if (result != PARSE_OK) {
            clause_destroy(clause);
            snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                    "Cláusula inválida: %s", trimmed_line);
            return result;
        }
        
        /* Verificar se cláusula não está vazia (a não ser que permitido) */
        if (clause->size == 0) {
            clause_destroy(clause);
            if (parser->strict_mode) {
                snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                        "Cláusula vazia não permitida no modo rigoroso");
                return PARSE_ERROR_INVALID_CLAUSE;
            }
            continue; /* Ignorar cláusula vazia */
        }

        /* Ignorar tautologias explicitamente (não alteram a satisfatibilidade) */
        if (clause_is_tautology(clause)) {
            if (parser->verbose) {
                log_debug("Ignorando cláusula tautológica na linha %d", parser->info.line_number);
            }
            clause_destroy(clause);
            continue;
        }
        
        /* Adicionar cláusula à fórmula */
        if (!cnf_add_clause(parser->formula, clause)) {
            clause_destroy(clause);
            return PARSE_ERROR_MEMORY;
        }
        
        parser->info.parsed_clauses++;
        
        if (parser->verbose && parser->info.parsed_clauses % 1000 == 0) {
            log_debug("Processadas %d cláusulas...", parser->info.parsed_clauses);
        }
    }
    
    timer_stop(&timer);
    
    if (!problem_line_found) {
        snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                "Linha de definição 'p cnf' não encontrada");
        return PARSE_ERROR_NO_PROBLEM_LINE;
    }
    
    /* Verificar número de cláusulas */
    if (parser->strict_mode && parser->info.parsed_clauses != parser->info.expected_clauses) {
        snprintf(parser->info.error_message, sizeof(parser->info.error_message),
                "Esperadas %d cláusulas, encontradas %d", 
                parser->info.expected_clauses, parser->info.parsed_clauses);
        return PARSE_ERROR_INVALID_FORMAT;
    }
    
    if (parser->verbose) {
        log_info("Parsing concluído: %d cláusulas em %.6f segundos", 
                parser->info.parsed_clauses, timer_elapsed(&timer));
    }
    
    return PARSE_OK;
}

/* ========== Funções de Parsing de Baixo Nível ========== */

parse_result_t parse_problem_line(const char *line, int *num_vars, int *num_clauses) {
    if (!line || !num_vars || !num_clauses) return PARSE_ERROR_INVALID_FORMAT;
    
    char format[16], dummy;
    int parsed = sscanf(line, "p %15s %d %d %c", format, num_vars, num_clauses, &dummy);
    
    if (parsed != 3) return PARSE_ERROR_INVALID_PROBLEM_LINE;
    
    if (strcmp(format, "cnf") != 0) return PARSE_ERROR_INVALID_PROBLEM_LINE;
    
    if (*num_vars <= 0 || *num_clauses < 0) return PARSE_ERROR_INVALID_PROBLEM_LINE;
    
    return PARSE_OK;
}

parse_result_t parse_clause_line(const char *line, clause_t *clause, int max_variables) {
    if (!line || !clause) return PARSE_ERROR_INVALID_FORMAT;
    
    char *line_copy = string_duplicate(line);
    if (!line_copy) return PARSE_ERROR_MEMORY;
    
    char *token = strtok(line_copy, " \t");
    bool terminated = false;
    
    while (token) {
        int literal;
        if (!parse_int(token, &literal)) {
            free(line_copy);
            return PARSE_ERROR_INVALID_CLAUSE;
        }
        
        if (literal == 0) {
            terminated = true;
            break;
        }
        
        if (!is_valid_literal(literal, max_variables)) {
            free(line_copy);
            return PARSE_ERROR_VARIABLE_OUT_OF_RANGE;
        }
        
        if (!clause_add_literal(clause, literal)) {
            free(line_copy);
            return PARSE_ERROR_MEMORY;
        }
        
        token = strtok(NULL, " \t");
    }
    
    free(line_copy);
    
    if (!terminated) {
        return PARSE_ERROR_CLAUSE_NOT_TERMINATED;
    }
    
    return PARSE_OK;
}

/* ========== Funções Auxiliares ========== */

bool is_comment_line(const char *line) {
    return line && line[0] == 'c';
}

bool is_problem_line(const char *line) {
    return line && string_starts_with(line, "p ");
}

bool is_clause_line(const char *line) {
    return line && !is_comment_line(line) && !is_problem_line(line) && !is_empty_line(line);
}

bool is_empty_line(const char *line) {
    if (!line) return true;
    
    while (*line) {
        if (!isspace(*line)) return false;
        line++;
    }
    
    return true;
}

/* ========== Funções de Validação ========== */

bool parser_validate_file(const char *filename, parser_info_t *info) {
    cnf_parser_t *parser = parser_create(true, false);
    if (!parser) return false;
    
    parse_result_t result = parser_parse_file(parser, filename);
    
    if (info) {
        *info = parser->info;
    }
    
    parser_destroy(parser);
    return result == PARSE_OK;
}

bool parser_validate_string(const char *content, parser_info_t *info) {
    cnf_parser_t *parser = parser_create(true, false);
    if (!parser) return false;
    
    parse_result_t result = parser_parse_string(parser, content);
    
    if (info) {
        *info = parser->info;
    }
    
    parser_destroy(parser);
    return result == PARSE_OK;
}

/* ========== Funções Utilitárias ========== */

const char* parser_error_string(parse_result_t result) {
    switch (result) {
        case PARSE_OK: return "Sucesso";
        case PARSE_ERROR_FILE_NOT_FOUND: return "Arquivo não encontrado";
        case PARSE_ERROR_INVALID_FORMAT: return "Formato inválido";
        case PARSE_ERROR_MEMORY: return "Erro de memória";
        case PARSE_ERROR_NO_PROBLEM_LINE: return "Linha de definição não encontrada";
        case PARSE_ERROR_INVALID_PROBLEM_LINE: return "Linha de definição inválida";
        case PARSE_ERROR_INVALID_CLAUSE: return "Cláusula inválida";
        case PARSE_ERROR_VARIABLE_OUT_OF_RANGE: return "Variável fora do intervalo";
        case PARSE_ERROR_CLAUSE_NOT_TERMINATED: return "Cláusula não terminada com 0";
        case PARSE_ERROR_EMPTY_FILE: return "Arquivo vazio";
        default: return "Erro desconhecido";
    }
}

void parser_print_info(const parser_info_t *info) {
    if (!info) return;
    
    printf(COLOR_BLUE "=== Informações do Parser ===" COLOR_RESET "\n");
    printf("Linhas processadas: %d\n", info->line_number);
    printf("Variáveis máximas: %d\n", info->max_variables);
    printf("Cláusulas esperadas: %d\n", info->expected_clauses);
    printf("Cláusulas parseadas: %d\n", info->parsed_clauses);
    
    if (info->error_message[0] != '\0') {
        printf("Erro: %s\n", info->error_message);
    }
}

/* ========== Funções de Escrita (Opcional) ========== */

bool parser_write_cnf_file(const cnf_formula_t *cnf, const char *filename) {
    if (!cnf || !filename) return false;
    
    FILE *file = fopen(filename, "w");
    if (!file) return false;
    
    bool result = parser_write_cnf_stream(cnf, file);
    fclose(file);
    
    return result;
}

bool parser_write_cnf_stream(const cnf_formula_t *cnf, FILE *stream) {
    if (!cnf || !stream) return false;
    
    /* Escrever header */
    fprintf(stream, "c Arquivo CNF gerado pelo SAT Solver\n");
    fprintf(stream, "p cnf %d %zu\n", cnf->num_variables, cnf->clauses.count);
    
    /* Escrever cláusulas */
    for (size_t i = 0; i < cnf->clauses.count; i++) {
        const clause_t *clause = &cnf->clauses.clauses[i];
        
        for (size_t j = 0; j < clause->size; j++) {
            fprintf(stream, "%d ", clause->literals[j]);
        }
        fprintf(stream, "0\n");
    }
    
    return true;
}