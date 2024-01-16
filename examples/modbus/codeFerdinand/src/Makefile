# Name des ausführbaren Programms
TARGET = fronius

# Compiler
CC = g++

# Compiler-Flags
CFLAGS = -DMODBUS_VERBOSE -Wall -Werror -Wno-unused-variable -I/usr/include/modbus -g -I.

# Bibliotheks-Flags
LDFLAGS = -lmodbus -lm

# Quellcodedateien
OBJ = pvclient.o modbusReader.o modbusRegister.o



# Regel zum Erstellen des ausführbaren Programms
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Regel zum Übersetzen von C-Quelldateien in Objektdateien
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@


# Regel zum Löschen der Objekt- und ausführbaren Dateien
clean:
	rm -f $(OBJ) $(TARGET)
