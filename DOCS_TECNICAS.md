# 📋 Documentação Técnica - SAT Solver DPLL

## 🏗️ Arquitetura do Sistema

### Componentes Principais

#### 1. **Parser (`src/parser.c`)**
- **Responsabilidade**: Análise de arquivos DIMACS CNF
- **Funcionalidades**:
  - Parsing robusto com detecção de erros
  - Remoção automática de tautologias (`x ∨ ¬x`)
  - Limpeza de literais duplicados
  - Validação de formato DIMACS
- **Estruturas**: `cnf_parser_t`, `parser_config_t`

#### 2. **Solver (`src/solver.c`)**
- **Responsabilidade**: Algoritmo DPLL principal
- **Funcionalidades**:
  - Propagação unitária otimizada
  - Eliminação de literais puros
  - Backtracking robusto com inversão de decisões
  - Múltiplas heurísticas de decisão
  - Detecção de timeout e limites
- **Estruturas**: `dpll_solver_t`, `solver_config_t`

#### 3. **Estruturas (`src/structures.c`)**
- **Responsabilidade**: Estruturas de dados fundamentais
- **Funcionalidades**:
  - Cláusulas dinâmicas com crescimento automático
  - Fórmulas CNF com validação
  - Pilha de atribuições para backtracking
  - Operações de avaliação eficientes
- **Estruturas**: `clause_t`, `cnf_formula_t`, `assignment_stack_t`

#### 4. **Interface (`src/main.c`)**
- **Responsabilidade**: CLI e formatação de saída
- **Funcionalidades**:
  - Parsing de argumentos da linha de comando
  - Configuração de heurísticas e limites
  - Formatação compatível com aula
  - Códigos de saída padronizados

#### 5. **Utilitários (`src/utils.c`)**
- **Responsabilidade**: Funções auxiliares
- **Funcionalidades**:
  - Logging com cores e níveis
  - Alocação de memória segura
  - Medição de tempo e estatísticas
  - Gerador de números aleatórios

---

## 🧮 Algoritmo DPLL Detalhado

### Fluxo de Execução

```
┌─────────────────┐
│   Pré-processo  │ ─── Remove tautologias, simplifica
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Fórmula SAT?   │ ─── Verifica se já está satisfeita
└─────────┬───────┘
          │ Não
          ▼
┌─────────────────┐
│   Conflito?     │ ─── Detecta cláusulas falsas
└─────────┬───────┘
          │ Sim
          ▼
┌─────────────────┐
│   Backtrack     │ ─── Desfaz decisões e inverte
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Propagação     │ ─── Propaga literais unitários
│    Unitária     │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│ Elimina Puros   │ ─── Remove literais de polaridade única
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│ Escolhe Variável│ ─── Heurística de decisão
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│ Faz Atribuição │ ─── Decisão + empilha
└─────────┬───────┘
          │
          └─────────── Loop até SAT/UNSAT/TIMEOUT
```

### Otimizações Implementadas

#### **1. Propagação Unitária Inteligente**
```c
// Detecta cláusulas com apenas um literal livre
if (clause_is_unit(clause, assignment, &unit_literal)) {
    // Força atribuição para satisfazer a cláusula
    assign_variable(solver, var, value, false); // marcado como propagação
}
```

#### **2. Backtracking Robusto**
```c
// Encontra última decisão na pilha
// Remove propagações subsequentes
// Inverte decisão e reaplica COMO DECISÃO
assign_variable(solver, var, opposite_value, true); // crucial: is_decision=true
```

#### **3. Detecção de Tautologias**
```c
// Parser detecta e remove automaticamente cláusulas como "x ∨ ¬x"
bool clause_is_tautology(const clause_t *clause) {
    // Verifica se existem literais opostos da mesma variável
}
```

---

## 📊 Heurísticas de Decisão

### 1. **First Unassigned** (`DECISION_FIRST_UNASSIGNED`)
- **Estratégia**: Primeira variável não atribuída (x1, x2, x3...)
- **Performance**: Rápida, determinística
- **Uso**: Testes e casos simples

### 2. **Most Frequent** (`DECISION_MOST_FREQUENT`)
- **Estratégia**: Variável que aparece em mais cláusulas
- **Performance**: Boa para fórmulas densas
- **Implementação**: Conta ocorrências positivas e negativas

### 3. **Jeroslow-Wang** (`DECISION_JEROSLOW_WANG`) - **Padrão**
- **Estratégia**: Peso baseado no tamanho das cláusulas
- **Fórmula**: `score = Σ 2^(-|C|)` para cláusulas C contendo a variável
- **Performance**: Balanceada, boa para casos gerais
- **Justificativa**: Cláusulas menores têm maior impacto

### 4. **Random** (`DECISION_RANDOM`)
- **Estratégia**: Escolha aleatória entre variáveis livres
- **Performance**: Imprevisível, útil para testes
- **Uso**: Quebra de empates e diversificação

---

## 🔧 Configurações Avançadas

### Solver Config
```c
solver_config_t config = {
    .decision_strategy = DECISION_JEROSLOW_WANG,
    .enable_pure_literal = true,        // Recomendado: ON
    .enable_unit_propagation = true,    // Essencial: ON
    .enable_preprocessing = true,       // Recomendado: ON
    .timeout_seconds = 5.0,            // Padrão: 5s
    .max_decisions = 1000              // Limite de segurança
};
```

### Parser Config
```c
parser_config_t config = {
    .allow_tautologies = true,         // Remove automaticamente
    .allow_duplicate_literals = true,  // Limpa automaticamente
    .allow_empty_clauses = false,      // Inválidas
    .max_line_length = 1024           // Buffer de linha
};
```

---

## 📈 Métricas e Estatísticas

### Contadores Implementados
- **`decisions`**: Número de decisões tomadas
- **`propagations`**: Propagações unitárias executadas
- **`conflicts`**: Conflitos detectados
- **`restarts`**: Reinicializações (se habilitadas)
- **`solve_time`**: Tempo total de resolução

### Interpretação
- **Alta razão propagations/decisions**: Boa eficiência
- **Muitos conflitos**: Fórmula difícil ou heurística inadequada
- **Poucas decisões**: Resolvido principalmente por propagação (ideal)

---

## 🐛 Debugging e Troubleshooting

### Flags de Debug
```powershell
# Compilar com debug
gcc -g -DDEBUG -Wall -Wextra -std=c99 -Iinclude src/*.c -o solver_debug.exe

# Executar com logs detalhados
.\solver_fixed.exe -v -s arquivo.cnf
```

### Problemas Comuns

#### **1. Loop Infinito**
- **Sintoma**: Solver não termina
- **Causa**: Progresso não detectado, backtrack incorreto
- **Solução**: Verificar `no_progress_rounds`, limitar iterações

#### **2. UNSAT Incorreto**
- **Sintoma**: Tautologia retorna UNSAT
- **Causa**: Parser não remove tautologias, conflito mal detectado
- **Solução**: Verificar `clause_is_tautology()`, `has_conflict()`

#### **3. Crash na Saída**
- **Sintoma**: Erro ao finalizar programa
- **Causa**: Double-free, ownership de estruturas
- **Solução**: Verificar `cnf_destroy()`, `clause_list_dispose_contents()`

### Logs Úteis
```c
// Ativar logs no código
#define DEBUG
log_debug("Variável x%d atribuída como %s", var, value ? "TRUE" : "FALSE");
print_assignment_stack(solver); // Estado da pilha
```

---

## 📚 Referências e Padrões

### Formato DIMACS CNF
```
c Comentários começam com 'c'
p cnf <num_vars> <num_clauses>
<literal1> <literal2> ... 0
<literal1> <literal2> ... 0
```

### Códigos de Saída Padrão
- **10**: SAT (satisfazível)
- **20**: UNSAT (insatisfazível) 
- **0**: UNKNOWN/TIMEOUT
- **1**: ERRO

### Saída Esperada
```
s SATISFIABLE
1 = 1
2 = 0  
3 = 1
```

---

## 🚀 Extensões Futuras

### Possíveis Melhorias
1. **Clause Learning**: Aprender cláusulas de conflitos
2. **Restarts Adaptativos**: Reinicializações baseadas em métricas
3. **Preprocessamento Avançado**: Equivalências, subsunções
4. **Paralelização**: Busca paralela com compartilhamento
5. **Heurísticas Modernas**: VSIDS, CHB, LRB

### Interface Gráfica
- Visualização da árvore de busca
- Debug interativo passo-a-passo
- Análise de performance em tempo real

---

**Última atualização**: Outubro 2025  
**Versão**: 2.0 - DPLL Robusto