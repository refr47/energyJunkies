# Name des ausführbaren Programms
TARGET = fronius

# Compiler
CC = gcc

# Compiler-Flags
CFLAGS = -Wall -Werror -I/usr/include/modbus -g

# Bibliotheks-Flags
LDFLAGS = -lmodbus -lm

# Quellcodedateien
SRC = pvclient.c

# Objektdateien
OBJ = $(SRC:.c=.o)

# Regel zum Erstellen des ausführbaren Programms
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Regel zum Übersetzen von C-Quelldateien in Objektdateien
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regel zum Löschen der Objekt- und ausführbaren Dateien
clean:
	rm -f $(OBJ) $(TARGET)
