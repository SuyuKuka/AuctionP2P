# Compilateur
CC = gcc
# Dossiers
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
# Flags
CFLAGS = -g -Wall -Wextra -std=c17 -I$(INCLUDE_DIR) -MMD
# Noms des fichiers .c dans src
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Noms des fichiers objets (fichiers .c de SRCS remplacé en .o)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
# Flags Valgrind
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes \
			--show-reachable=no --track-fds=yes --quiet --error-exitcode=1

# Ignore les fichiers nommés trouvés
.PHONY: all clean valgrind makedir

# Règle par défaut
all: makedir auction

makedir:
	mkdir -p $(BUILD_DIR)

# Compile l'exécutable, dépend des fichiers .o
auction: $(OBJS)
	$(CC) $(OBJS) -o auction

# Compilation en .o, dépend du .c
# '%' est un wildcard.
# '$<' est la première dépendance (%.c)
# '$@' est le nom de la cible (%.o)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

valgrind: all
	valgrind $(VFLAGS) ./auction

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d auction
	rm -d $(BUILD_DIR)

-include $(BUILD_DIR)/*.d