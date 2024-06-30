# Nombre del ejecutable
EXECUTABLE = programa

# Archivos fuente
SOURCES = main.cpp

# Compilador de C++
CXX = g++

# Flags del compilador
CXXFLAGS = -fopenmp

# Includes para LibXL
INCLUDES = -I/usr/local/include

# Flags de enlace para LibXL
LFLAGS = -L/usr/local/lib -Wl,-rpath,/usr/local/lib

# Librerías
LIBS = -lxl 

# Regla para compilar el ejecutable
$(EXECUTABLE): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LFLAGS) -o $(EXECUTABLE) $(SOURCES) $(LIBS)

# Regla para limpiar los archivos objeto y el ejecutable
clean:
	rm -f $(EXECUTABLE)

# Regla para ejecutar el programa
run: $(EXECUTABLE)
	./$(EXECUTABLE)
