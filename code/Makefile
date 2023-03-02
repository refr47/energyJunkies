# Name des ausführbaren Programms
TARGET = fronius

# Compiler
CC = gcc

# Compiler-Flags
CFLAGS = -Wall -Werror -I/usr/include/modbus

# Bibliotheks-Flags
LDFLAGS = -L/path/to/lib -lmodbus-dev

# Quellcodedateien
SRC = main.c foo.c bar.c

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
