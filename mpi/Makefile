CC      = mpicc
CFLAGS  = -Wall -Wextra -O2
TARGET  = impreza
SRC     = impreza.c

# Domyślna liczba procesów i rozmiar kółka przy `make run`
NP          = 6
CIRCLE_SIZE = 3

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	mpirun --oversubscribe -np $(NP) -x CIRCLE_SIZE=$(CIRCLE_SIZE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
