# Makefile para SAT Solver em C

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude
DEBUGFLAGS = -g -DDEBUG
RELEASEFLAGS = -O2 -DNDEBUG
TARGET = satsolver
SRCDIR = src
OBJDIR = obj
INCDIR = include

# Arquivos fonte
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Regra padrão - compilação release
all: CFLAGS += $(RELEASEFLAGS)
all: $(TARGET)

# Compilação debug
debug: CFLAGS += $(DEBUGFLAGS)
debug: $(TARGET)

# Criação do executável
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@
	@echo "Build concluído: $(TARGET)"

# Compilação de arquivos objeto
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Criar diretório obj se não existir
$(OBJDIR):
	mkdir $(OBJDIR)

# Limpeza
clean:
	@if exist $(OBJDIR) rmdir /s /q $(OBJDIR)
	@if exist $(TARGET).exe del $(TARGET).exe
	@if exist $(TARGET) del $(TARGET)
	@echo "Arquivos limpos"

# Executar testes
test: $(TARGET)
	@echo "Executando testes..."
	@if exist examples\simple.cnf $(TARGET) examples\simple.cnf
	@if exist examples\satisfiable.cnf $(TARGET) examples\satisfiable.cnf
	@if exist examples\unsatisfiable.cnf $(TARGET) examples\unsatisfiable.cnf

# Instalar (copiar para diretório do sistema - opcional)
install: $(TARGET)
	@echo "Para instalar, copie o executável para um diretório no PATH"

# Mostrar informações
info:
	@echo "SAT Solver em C"
	@echo "Compilador: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Arquivos fonte: $(SOURCES)"

# Regras que não são arquivos
.PHONY: all debug clean test install info

# Dependências dos headers (adicionar conforme necessário)
$(OBJDIR)/main.o: $(INCDIR)/parser.h $(INCDIR)/solver.h $(INCDIR)/utils.h
$(OBJDIR)/parser.o: $(INCDIR)/parser.h $(INCDIR)/structures.h $(INCDIR)/utils.h
$(OBJDIR)/solver.o: $(INCDIR)/solver.h $(INCDIR)/structures.h $(INCDIR)/utils.h
$(OBJDIR)/structures.o: $(INCDIR)/structures.h $(INCDIR)/utils.h
$(OBJDIR)/utils.o: $(INCDIR)/utils.h