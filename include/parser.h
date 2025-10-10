#ifndef PARSER_H
#define PARSER_H

#include "structures.h"
#include <stdio.h>

/* Códigos de erro do parser */
typedef enum {
    PARSE_OK = 0,
    PARSE_ERROR_FILE_NOT_FOUND = -1,
    PARSE_ERROR_INVALID_FORMAT = -2,
    PARSE_ERROR_MEMORY = -3,
    PARSE_ERROR_NO_PROBLEM_LINE = -4,
    PARSE_ERROR_INVALID_PROBLEM_LINE = -5,
    PARSE_ERROR_INVALID_CLAUSE = -6,
    PARSE_ERROR_VARIABLE_OUT_OF_RANGE = -7,
    PARSE_ERROR_CLAUSE_NOT_TERMINATED = -8,
    PARSE_ERROR_EMPTY_FILE = -9
} parse_result_t;

/* Estrutura para informações do parser */
typedef struct {
    int line_number;           // Linha atual sendo processada
    int expected_clauses;      // Número esperado de cláusulas
    int parsed_clauses;        // Número de cláusulas parseadas
    int max_variables;         // Número máximo de variáveis
    char error_message[256];   // Mensagem de erro detalhada
} parser_info_t;

/* Estrutura principal do parser */
typedef struct {
    parser_info_t info;
    cnf_formula_t *formula;
    bool strict_mode;          // Se deve ser rigoroso com o formato
    bool verbose;              // Se deve imprimir informações detalhadas
} cnf_parser_t;

/* Funções principais do parser */
cnf_parser_t* parser_create(bool strict_mode, bool verbose);
void parser_destroy(cnf_parser_t *parser);

/* Funções de parsing */
parse_result_t parser_parse_file(cnf_parser_t *parser, const char *filename);
parse_result_t parser_parse_string(cnf_parser_t *parser, const char *content);
parse_result_t parser_parse_stream(cnf_parser_t *parser, FILE *stream);

/* Funções de validação */
bool parser_validate_file(const char *filename, parser_info_t *info);
bool parser_validate_string(const char *content, parser_info_t *info);

/* Funções utilitárias */
const char* parser_error_string(parse_result_t result);
void parser_print_info(const parser_info_t *info);
void parser_reset(cnf_parser_t *parser);

/* Funções de parsing de baixo nível */
parse_result_t parse_problem_line(const char *line, int *num_vars, int *num_clauses);
parse_result_t parse_clause_line(const char *line, clause_t *clause, int max_variables);
parse_result_t parse_comment_line(const char *line);

/* Funções auxiliares */
bool is_comment_line(const char *line);
bool is_problem_line(const char *line);
bool is_clause_line(const char *line);
bool is_empty_line(const char *line);

/* Funções para estatísticas de parsing */
typedef struct {
    size_t total_lines;        // Total de linhas processadas
    size_t comment_lines;      // Linhas de comentário
    size_t empty_lines;        // Linhas vazias
    size_t clause_lines;       // Linhas de cláusulas
    size_t problem_lines;      // Linhas de definição do problema
    double parse_time;         // Tempo de parsing
} parse_stats_t;

void parse_stats_init(parse_stats_t *stats);
void parse_stats_print(const parse_stats_t *stats);

/* Configurações do parser */
typedef struct {
    bool allow_empty_clauses;     // Permitir cláusulas vazias
    bool allow_tautologies;       // Permitir cláusulas tautológicas
    bool allow_duplicate_literals; // Permitir literais duplicados
    bool ignore_extra_clauses;    // Ignorar cláusulas extras além do esperado
    bool require_sorted_literals; // Exigir literais ordenados
    size_t max_clause_size;       // Tamanho máximo de cláusula (0 = sem limite)
    size_t max_line_length;       // Tamanho máximo de linha
} parser_config_t;

/* Configuração padrão */
extern const parser_config_t DEFAULT_PARSER_CONFIG;

/* Parser com configuração customizada */
cnf_parser_t* parser_create_with_config(const parser_config_t *config);
void parser_set_config(cnf_parser_t *parser, const parser_config_t *config);

/* Funções para escrita (opcional - para salvar CNF processado) */
bool parser_write_cnf_file(const cnf_formula_t *cnf, const char *filename);
bool parser_write_cnf_stream(const cnf_formula_t *cnf, FILE *stream);

/* Macro para verificar resultado do parsing */
#define CHECK_PARSE_RESULT(result, parser) \
    do { \
        if ((result) != PARSE_OK) { \
            log_error("Erro de parsing: %s (linha %d)", \
                     parser_error_string(result), \
                     (parser)->info.line_number); \
            return (result); \
        } \
    } while(0)

#endif /* PARSER_H */