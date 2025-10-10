<<<<<<< HEAD
# 🧠 SAT Solver em C - Algoritmo DPLL

Um solver SAT (Boolean Satisfiability Problem) robusto implementado em C usando o algoritmo DPLL (Davis-Putnam-Logemann-Loveland) com heurísticas avançadas e backtracking otimizado.

## 📋 Características

- ✅ **Algoritmo DPLL** completo com propagação unitária e eliminação de literais puros
- ✅ **Múltiplas heurísticas** de decisão (First, Most Frequent, Jeroslow-Wang, Random)
- ✅ **Backtracking robusto** com detecção de tautologias
- ✅ **Parser DIMACS** completo com validação
- ✅ **Logs verbosos** e estatísticas detalhadas
- ✅ **Timeouts configuráveis** e limites de decisões
- ✅ **Saída compatível** com formato de aula

## 🛠️ Compilação

### Usando Makefile:
```bash
make
```

### Compilação manual:
```bash
gcc -Wall -Wextra -std=c99 -Iinclude src/*.c -o solver_fixed.exe
```

## 🚀 Uso

### Execução básica:
```powershell
.\solver_fixed.exe arquivo.cnf
```

### Opções avançadas:
```powershell
# Modo verboso + estatísticas
.\solver_fixed.exe -v -s arquivo.cnf

# Configurar timeout e estratégia
.\solver_fixed.exe --timeout 60 --strategy jw arquivo.cnf

# Mostrar modelo se SAT
.\solver_fixed.exe -a arquivo.cnf
```

### 📖 Opções completas:
| Opção | Descrição |
|-------|-----------|
| `-h, --help` | Mostrar ajuda completa |
| `-v, --verbose` | Logs detalhados do processo |
| `-s, --stats` | Estatísticas de performance |
| `-a, --assignment` | Mostrar atribuição das variáveis |
| `-t, --timeout <seg>` | Timeout em segundos (padrão: 5s) |
| `-d, --decisions <n>` | Máximo de decisões (padrão: 1000) |
| `--strategy <tipo>` | `first`\|`frequent`\|`jw`\|`random` |

## 📄 Formato de Entrada (DIMACS CNF)

```
c Este é um comentário
c Problema: (x1 ∨ ¬x3) ∧ (x2 ∨ x3 ∨ ¬x1)
p cnf 3 2
1 -3 0
2 3 -1 0
```

**Regras:**
- Linha `p cnf V C`: V variáveis, C cláusulas
- Cada cláusula termina com `0`
- Literais positivos: `1, 2, 3...`
- Literais negativos: `-1, -2, -3...`
- Comentários começam com `c`

## 📊 Códigos de Saída

| Código | Significado | Saída |
|--------|-------------|-------|
| **10** | SATISFIABLE | Fórmula tem solução |
| **20** | UNSATISFIABLE | Fórmula é contraditória |
| **0** | UNKNOWN/TIMEOUT | Não conseguiu resolver |
| **1** | ERRO | Erro de parsing ou execução |

## 🗂️ Estrutura do Projeto

```
satsolver-c/
├── 📁 src/                    # Código fonte
│   ├── main.c                 # Interface e argumentos CLI
│   ├── solver.c               # Algoritmo DPLL principal  
│   ├── parser.c               # Parser formato DIMACS
│   ├── structures.c           # Estruturas de dados CNF
│   └── utils.c                # Funções utilitárias
├── 📁 include/                # Headers (.h)
│   ├── solver.h               # Definições do solver
│   ├── parser.h               # Interface do parser
│   ├── structures.h           # Tipos de dados
│   └── utils.h                # Utilitários e macros
├── 📁 examples/               # CNFs de exemplo
│   ├── simple.cnf             # Exemplo básico
│   ├── satisfiable.cnf        # Caso SAT
│   └── unsatisfiable.cnf      # Caso UNSAT
├── entrada.cnf                # Seu arquivo de teste
├── solver_fixed.exe           # Binário compilado
├── README.md                  # Esta documentação
├── Makefile                   # Sistema de build
└── .gitignore                 # Configuração Git
```

## 🧮 Algoritmo DPLL

### Fluxo principal:
1. **📥 Pré-processamento**: Remove tautologias, simplifica
2. **🔄 Loop DPLL**:
   - **Propagação Unitária**: Atribui literais únicos
   - **Eliminação Puros**: Remove literais de polaridade única  
   - **Decisão**: Escolhe variável usando heurística
   - **Backtracking**: Desfaz em caso de conflito
3. **✅ Resultado**: SAT/UNSAT/UNKNOWN

### Heurísticas de decisão:
- **`first`** - Primeira variável não atribuída
- **`frequent`** - Variável mais frequente nas cláusulas
- **`jw`** - Jeroslow-Wang (peso por tamanho de cláusula)
- **`random`** - Escolha aleatória (para testes)

## 📈 Exemplos de Uso

### Teste rápido:
```powershell
# Criar CNF simples
echo "p cnf 2 2`n1 2 0`n-1 -2 0" | Out-File teste.cnf -Encoding ascii

# Executar
.\solver_fixed.exe -v teste.cnf
```

### CNF Satisfazível:
```
p cnf 3 3
1 2 0     # x1 ∨ x2  
-2 3 0    # ¬x2 ∨ x3
-1 -3 0   # ¬x1 ∨ ¬x3
```
**Solução**: `x1=1, x2=0, x3=1`

### CNF Insatisfazível:
```  
p cnf 1 2
1 0       # x1
-1 0      # ¬x1
```
**Resultado**: UNSAT (contradição)

## 🔧 Desenvolvimento

### Compilar com debug:
```bash
gcc -g -DDEBUG -Wall -Wextra -std=c99 -Iinclude src/*.c -o solver_debug.exe
```

### Executar com Valgrind (Linux):
```bash
valgrind --leak-check=full ./solver_debug exemplo.cnf
```

### Adicionar nova heurística:
1. Definir enum em `include/solver.h`
2. Implementar função em `src/solver.c`  
3. Adicionar case em `choose_decision_variable()`
4. Atualizar help em `src/main.c`

## 📚 Referências

- [DPLL Algorithm](https://en.wikipedia.org/wiki/DPLL_algorithm)
- [DIMACS CNF Format](http://www.satcompetition.org/2009/format-benchmarks2009.html)
- [SAT Solving Techniques](https://baldur.iti.kit.edu/sat/)

## 📄 Licença

MIT License - Livre para uso acadêmico e comercial.

---

**Desenvolvido para disciplina de Estrutura de Dados - UFAL** 🎓
=======
# SatSolver
Projeto da Disciplina de Estrutura de Dados 

