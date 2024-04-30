# Compilador
CC = g++

# Opciones de compilación
CFLAGS = -Wall -Wextra -pedantic -std=c++11

# Nombre de los ejecutables
SENSOR_EXEC = sensor
MONITOR_EXEC = monitor

# Archivos fuente
SENSOR_SRCS = sensor.cpp buffer.cpp
MONITOR_SRCS = monitor.cpp buffer.cpp

# Objetos
SENSOR_OBJS = $(SENSOR_SRCS:.cpp=.o)
MONITOR_OBJS = $(MONITOR_SRCS:.cpp=.o)

# Reglas de compilación
all: $(SENSOR_EXEC) $(MONITOR_EXEC)

$(SENSOR_EXEC): $(SENSOR_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(MONITOR_EXEC): $(MONITOR_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SENSOR_OBJS) $(MONITOR_OBJS) $(SENSOR_EXEC) $(MONITOR_EXEC)