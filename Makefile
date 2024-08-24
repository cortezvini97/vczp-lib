# Defina o compilador e os flags de compilação
CC = gcc
CFLAGS = -fPIC -Iinclude -I/usr/include/cjson -I/usr/include/b64
AR = ar
ARFLAGS = rcs

# Diretórios de destino
BUILD_DIR = build
DIST_DIR = dist
TEST_DIR = tests
INCLUDE_DIR = /usr/include/vczp
LIB_DIR = /usr/lib/vczp

# Nome do arquivo de saída
TARGET = $(DIST_DIR)/libvczp.a
TEST_EXEC = $(TEST_DIR)/main

# Lista de arquivos fonte para a biblioteca
LIB_SRCS = src/vczp.c
LIB_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(LIB_SRCS))

# Lista de arquivos fonte para o executável de teste
TEST_SRCS = src/main.c src/vczp.c
TEST_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(TEST_SRCS))

# Regra padrão
all: $(TARGET)

# Criação dos diretórios de destino, se necessário
$(BUILD_DIR) $(DIST_DIR) $(TEST_DIR):
	mkdir -p $@

# Regra para criar a biblioteca estática
$(TARGET): $(LIB_OBJS) | $(DIST_DIR)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

# Regra para compilar arquivos .c em .o e colocar em build/
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para compilar e gerar o executável de teste
$(TEST_EXEC): $(BUILD_DIR)/main.o $(BUILD_DIR)/vczp.o | $(TEST_DIR)
	$(CC) -o $@ $(BUILD_DIR)/main.o $(BUILD_DIR)/vczp.o

# Regra para compilar main.c em main.o
$(BUILD_DIR)/main.o: src/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/main.c -o $@

# Regra para compilar vczp.c em vczp.o
$(BUILD_DIR)/vczp.o: src/vczp.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/vczp.c -o $@

# Regra para limpar os arquivos gerados
clean:
	rm -f $(LIB_OBJS) $(TARGET)
	rm -f $(TEST_OBJS) $(TEST_EXEC)
	rmdir $(BUILD_DIR) 2>/dev/null || true
	rmdir $(TEST_DIR) 2>/dev/null || true
	rm -rf $(DIST_DIR)

# Regra para compilar e executar o executável de teste
test: $(TEST_EXEC)
	$(TEST_EXEC)

install: $(TARGET)
	make uninstall
	mkdir -p $(INCLUDE_DIR)
	mkdir -p $(LIB_DIR)
	cp include/vczp.h $(INCLUDE_DIR)/
	cp $(TARGET) $(LIB_DIR)/

uninstall:
	rm -f $(INCLUDE_DIR)/vczp.h
	rm -f $(LIB_DIR)/libvczp.a

.PHONY: all clean test install uninstall
