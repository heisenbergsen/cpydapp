#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <array>
#include <libxl.h>

using namespace libxl;

struct Bloque {
    std::vector<std::string> lineas;
};

struct Fecha {
    int anio;
    int mes;
    int dia;

    bool operator==(const Fecha& other) const {
        return anio == other.anio && mes == other.mes && dia == other.dia;
    }
};

struct RegistroCompra {
    Fecha fecha;
    int numeroTienda;
    std::string identificadorProducto;
    std::string nombre;
    int cantidad;
    double monto;
};

class Cola {
private:
    std::queue<Bloque> cola;

public:
    void push(const Bloque& bloque) {
        cola.push(bloque);
    }

    bool pop(Bloque& bloque) {
        if (cola.empty()) {
            return false;
        }
        bloque = std::move(cola.front());
        cola.pop();
        return true;
    }

    bool empty() const {
        return cola.empty();
    }

    size_t size() const {
        return cola.size();
    }
};

struct VentaMes {
    bool ventasEnMes; // Indica si hubo ventas en este mes
    double sumatoriaMontos; // Acumula el monto total de las ventas para este mes
    int sumatoriaCantidades; // Acumula la cantidad total de productos vendidos para este mes

    VentaMes() : ventasEnMes(false), sumatoriaMontos(0.0), sumatoriaCantidades(0) {}
};

struct VentaAnio {
    int year; // Año al que pertenece este objeto
    std::vector<VentaMes> ventasEnAnio; // Vector de ventas por mes

    VentaAnio(int y) : year(y) {
        ventasEnAnio.resize(12); // Inicializa con 12 meses
    }
};

struct ProductoMapa {
    std::string id; // Cambiado a std::string para representar el identificador como texto
    std::vector<std::string> nombres;
    std::vector<VentaAnio> ventasAnuales;

    // Constructor con el identificador como std::string
    ProductoMapa(const std::string& pid) : id(pid) {}
};

using MapaProductos = std::unordered_map<std::string, std::vector<ProductoMapa>>;

struct Canasta {
    std::string anio;
    std::vector<std::string> nombres;
    std::vector<std::string> ids;
    std::array<double, 12> precios;  // Arreglo de 12 precios

    // Constructor
    Canasta(const std::string& a) : anio(a), precios{0} {}

    // Método para agregar un nombre
    void agregarNombre(const std::string& nombre) {
        nombres.push_back(nombre);
    }

    // Método para agregar un ID
    void agregarId(const std::string& id) {
        ids.push_back(id);
    }

    // Método para establecer el precio de un mes específico
    void setPrecio(int mes, double precio) {
        if (mes >= 0 && mes < 12) {
            precios[mes] = precio;
        }
    }

    // Método para obtener el precio de un mes específico
    double getPrecio(int mes) const {
        if (mes >= 0 && mes < 12) {
            return precios[mes];
        }
        return 0.0;  // Retorna 0 si el mes es inválido
    }
};

Fecha obtenerFecha(const std::vector<std::string>& campos) {
    std::string fechaCompleta = campos[0];
    int anio, mes, dia;
    char delim;
    std::stringstream ss(fechaCompleta);
    ss >> anio >> delim >> mes >> delim >> dia;
    return { anio, mes, dia };
}

RegistroCompra procesarRegistro(const std::vector<std::string>& campos) {
    RegistroCompra registro;
    registro.fecha = obtenerFecha(campos);
    registro.identificadorProducto = campos[6];
    registro.nombre = campos[8];

    try {
        registro.numeroTienda = std::stoi(campos[2]);
        registro.cantidad = std::stoi(campos[7]);
        registro.monto = std::stod(campos[9]);
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Error de conversión en algún campo.");
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("Valor fuera de rango en algún campo numérico.");
    }

    return registro;
}


std::vector<std::string> procesarLinea(const std::string& linea) {
    std::vector<std::string> campos;
    std::string campo = "";
    bool dentroDeCampo = false;
    int contadorComillas = 0;
    bool campoFinalizado = false;

    for (char c : linea) {
        if (c == '"') {
            contadorComillas++;
            dentroDeCampo = !dentroDeCampo;
            if (contadorComillas % 2 == 0) {
                campos.push_back(campo);
                campoFinalizado = false;
                campo = "";
            }
        } else if (c == ';' && !dentroDeCampo) {
            if (campoFinalizado == true) {
                campos.push_back(campo);
            }
            campoFinalizado = true;
            campo = "";
        } else {
            campo += c;
        }
    }

    if (!campo.empty() && campoFinalizado) {
        campos.push_back(campo);
    }

    for (auto& c : campos) {
        if (!c.empty() && c.front() == '"' && c.back() == '"') {
            c = c.substr(1, c.length() - 2);
        }
        if (c.empty() || std::all_of(c.begin(), c.end(), isspace)) {
            c = "";
        }
    }

    return campos;
}

bool quedanLineasPorLeer(const std::string& nombre_archivo, std::streampos posicion_actual) {
    std::ifstream archivo(nombre_archivo);
    std::string linea;

    if (archivo.is_open()) {
        archivo.seekg(posicion_actual); // Mover la posición del archivo a la posición actual

        // Intentar leer una línea para verificar si quedan líneas por leer
        if (std::getline(archivo, linea)) {
            archivo.close();
            return true; // Quedan líneas por leer
        }

        archivo.close();
    } else {
        std::cerr << "No se pudo abrir el archivo" << std::endl;
    }

    return false; // No quedan líneas por leer o error al abrir el archivo
}

void leerCSV(const std::string& nombre_archivo, Cola& cola, int tamano_bloque, std::streampos& posicion_actual) {
    std::ifstream archivo(nombre_archivo);
    std::string linea;
    Bloque bloque_actual;

    if (archivo.is_open()) {
        archivo.seekg(posicion_actual); // Mover la posición del archivo a la posición actual guardada

        while (std::getline(archivo, linea) && bloque_actual.lineas.size() < tamano_bloque) {
            if (std::count(linea.begin(), linea.end(), '"') % 2 != 0) {
                std::string linea_siguiente;
                if (std::getline(archivo, linea_siguiente)) {
                    linea += linea_siguiente;
                }
            }

            bloque_actual.lineas.push_back(linea);
        }

        if (!bloque_actual.lineas.empty()) {
            cola.push(bloque_actual);
        }

        posicion_actual = archivo.tellg(); // Actualizar la posición actual del archivo

        archivo.close();
    } else {
        std::cerr << "No se pudo abrir el archivo" << std::endl;
    }
}

void descartarPrimeraLinea(const std::string& nombre_archivo, std::streampos& posicion_actual) {
    std::ifstream archivo(nombre_archivo);
    std::string linea;
    if (archivo.is_open()) {
        archivo.seekg(posicion_actual); // Mover la posición del archivo a la posición actual guardada
        std::getline(archivo, linea);
        posicion_actual = archivo.tellg(); // Actualizar la posición actual del archivo
        archivo.close();
    } else {
        std::cerr << "No se pudo abrir el archivo" << std::endl;
    }
}
// Función para procesar un bloque de datos
void procesarBloque(const Bloque& bloque, MapaProductos& productos, MapaProductos& canasta) {
    for (const auto& linea : bloque.lineas) {
        std::vector<std::string> campos = procesarLinea(linea);
        RegistroCompra registro;
        try {
            registro = procesarRegistro(campos);
        } catch (const std::runtime_error& e) {
            std::cerr << "Error al procesar registro: " << e.what() << std::endl;
            continue; // Salta este registro y pasa al siguiente
        }
        // Organización por año
        int anio = registro.fecha.anio;

        // Crear la clave para el producto (considerando colisiones)
        std::string key = registro.identificadorProducto;

        // Buscar el producto en el vector correspondiente al año
        bool encontrado = false;
        for (auto& producto : productos[key]) {
            if (producto.id == registro.identificadorProducto) {
                // Producto encontrado, buscar el nombre correspondiente
                for (size_t i = 0; i < producto.nombres.size(); ++i) {
                    if (producto.nombres[i] == registro.nombre) {
                        // Nombre encontrado, buscar el año correspondiente
                        for (auto& ventaAnio : producto.ventasAnuales) {
                            if (ventaAnio.year == anio) {
                                // Año encontrado, actualizar ventas mensuales
                                int mes = registro.fecha.mes - 1; // Meses de 0 a 11 en el arreglo
                                ventaAnio.ventasEnAnio[mes].ventasEnMes = true;
                                ventaAnio.ventasEnAnio[mes].sumatoriaMontos += registro.monto;
                                ventaAnio.ventasEnAnio[mes].sumatoriaCantidades += registro.cantidad;
                                ///////////////inyeccion de verificacion
                                bool todosLosMesesConVentas = true;
                                for (int mes = 0; mes < 11; ++mes) {
                                    if (!ventaAnio.ventasEnAnio[mes].ventasEnMes) {
                                        todosLosMesesConVentas = false; // Si encontramos un mes sin ventas, retornamos false
                                    }
                                }
                                if (todosLosMesesConVentas == true) {
                                    // Copiar el producto al mapa canasta
                                    std::string key = registro.identificadorProducto;
                                    
                                    // Buscar el producto en el mapa de origen (productos)
                                    auto it = std::find_if(productos[key].begin(), productos[key].end(),
                                        [&registro](const ProductoMapa& p) { return p.id == registro.identificadorProducto; });
                                    
                                    if (it != productos[key].end()) {
                                        // Producto encontrado, crear una copia
                                        ProductoMapa productoCopiado = *it;
                                        
                                        // Buscar si ya existe en el mapa de destino (canasta)
                                        auto it_canasta = std::find_if(canasta[key].begin(), canasta[key].end(),
                                            [&registro](const ProductoMapa& p) { return p.id == registro.identificadorProducto; });
                                        
                                        if (it_canasta != canasta[key].end()) {
                                            // Si ya existe, actualizarlo
                                            *it_canasta = productoCopiado;
                                        } else {
                                            // Si no existe, agregarlo
                                            canasta[key].push_back(productoCopiado);
                                        }
                                    }
                                }
                                encontrado = true;
                                break;
                            }
                        }
                        if (!encontrado) {
                            // Año no encontrado, agregar nuevo año de ventas
                            VentaAnio nuevaVentaAnio(anio);
                            int mes = registro.fecha.mes - 1; // Meses de 0 a 11 en el arreglo
                            nuevaVentaAnio.ventasEnAnio[mes].ventasEnMes = true;
                            nuevaVentaAnio.ventasEnAnio[mes].sumatoriaMontos += registro.monto;
                            nuevaVentaAnio.ventasEnAnio[mes].sumatoriaCantidades += registro.cantidad;
                            producto.ventasAnuales.push_back(nuevaVentaAnio);
                            encontrado = true;
                        }
                        break;
                    }
                }
                if (!encontrado) {
                    // Nombre no encontrado, agregar nuevo nombre y año de ventas
                    producto.nombres.push_back(registro.nombre);
                    VentaAnio nuevaVentaAnio(anio);
                    int mes = registro.fecha.mes - 1; // Meses de 0 a 11 en el arreglo
                    nuevaVentaAnio.ventasEnAnio[mes].ventasEnMes = true;
                    nuevaVentaAnio.ventasEnAnio[mes].sumatoriaMontos += registro.monto;
                    nuevaVentaAnio.ventasEnAnio[mes].sumatoriaCantidades += registro.cantidad;
                    producto.ventasAnuales.push_back(nuevaVentaAnio);
                    encontrado = true;
                }
                break;
            }
        }

        if (!encontrado) {
            // Producto no encontrado, crear nuevo producto y añadir nombre y año de ventas
            ProductoMapa nuevoProducto(registro.identificadorProducto);
            nuevoProducto.nombres.push_back(registro.nombre);
            VentaAnio nuevaVentaAnio(anio);
            int mes = registro.fecha.mes - 1; // Meses de 0 a 11 en el arreglo
            nuevaVentaAnio.ventasEnAnio[mes].ventasEnMes = true;
            nuevaVentaAnio.ventasEnAnio[mes].sumatoriaMontos += registro.monto;
            nuevaVentaAnio.ventasEnAnio[mes].sumatoriaCantidades += registro.cantidad;
            nuevoProducto.ventasAnuales.push_back(nuevaVentaAnio);
            productos[key].push_back(nuevoProducto);
        }
    }
}

void imprimirCanastaBasica(const MapaProductos& productos, int year) {
    std::cout << "Canasta básica del año " << year << std::endl;

    // Contadores para el total de productos y sumas para cálculo de promedio
    int totalProductos = 0;
    std::vector<double> sumatoriaMontos(12, 0.0); // Suma de montos por mes
    std::vector<int> sumatoriaCantidades(12, 0);  // Suma de cantidades por mes
    std::vector<bool> productoPresenteEnMes(12, true); // Indica si el producto está presente en cada mes

    // Recorrer el mapa de productos
    for (const auto& pair : productos) {
        const std::vector<ProductoMapa>& productosLista = pair.second;

        for (const ProductoMapa& producto : productosLista) {
            totalProductos++;

            // Reiniciar la bandera de presencia del producto en todos los meses
            for (int mes = 0; mes < 12; ++mes) {
                productoPresenteEnMes[mes] = false;
            }

            // Recorrer ventas por año para encontrar el año deseado
            for (const VentaAnio& ventaAnio : producto.ventasAnuales) {
                if (ventaAnio.year == year) {
                    // Recorrer ventas por mes
                    for (int mes = 0; mes < 12; ++mes) {
                        const VentaMes& ventaMes = ventaAnio.ventasEnAnio[mes];
                        if (ventaMes.ventasEnMes) {
                            sumatoriaMontos[mes] += ventaMes.sumatoriaMontos;
                            sumatoriaCantidades[mes] += ventaMes.sumatoriaCantidades;
                            productoPresenteEnMes[mes] = true;
                        }
                    }
                    break; // Salir del bucle de ventas anuales una vez encontrado el año deseado
                }
            }

            // Verificar si el producto está presente en todos los meses
            bool productoEnCanasta = true;
            for (int mes = 0; mes < 12; ++mes) {
                if (!productoPresenteEnMes[mes]) {
                    productoEnCanasta = false;
                    break;
                }
            }

            // Si el producto está en todos los meses, imprimir información
            if (productoEnCanasta) {
                std::cout << "Producto ID: " << producto.id << std::endl;
                std::cout << "Cantidad: " << producto.nombres.size() << std::endl;

                // Imprimir el costo unitario promedio por mes
                for (int mes = 0; mes < 12; ++mes) {
                    if (sumatoriaCantidades[mes] > 0) {
                        double precioPromedio = sumatoriaMontos[mes] / sumatoriaCantidades[mes];
                        std::cout << "Mes: " << mes + 1 << std::endl;
                        std::cout << "Costo unitario promedio: " << std::fixed << std::setprecision(2) << precioPromedio << std::endl;
                    }
                }

                std::cout << std::endl; // Separador entre productos
            }
        }
    }
}

void imprimirMapa(const MapaProductos& mapa) {
    for (const auto& [categoria, productos] : mapa) {
        std::cout << "Categoría id: " << categoria << std::endl;
        
        for (const auto& producto : productos) {
            std::cout << "  Producto ID: " << producto.id << std::endl;
            
            if (!producto.nombres.empty()) {
                std::cout << "    Nombres:";
                for (const auto& nombre : producto.nombres) {
                    std::cout << " " << nombre;
                }
                std::cout << std::endl;
            }
            
            for (const auto& ventaAnio : producto.ventasAnuales) {
                std::cout << "    Año: " << ventaAnio.year << std::endl;
                for (int mes = 0; mes < 12; ++mes) {
                    const auto& ventaMes = ventaAnio.ventasEnAnio[mes];
                    if (ventaMes.ventasEnMes) {
                        std::cout << "      Mes " << mes + 1 << ": "
                                  << "Monto: " << std::fixed << std::setprecision(2) << ventaMes.sumatoriaMontos
                                  << ", Cantidad: " << ventaMes.sumatoriaCantidades << std::endl;
                    }
                }
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

void procesarMapaYCanastas(const MapaProductos& mapa, std::vector<Canasta>& canastas) {
    for (const auto& [categoria, productos] : mapa) {
        for (const auto& producto : productos) {
            for (const auto& ventaAnio : producto.ventasAnuales) {
                // Verificar si todos los meses tienen ventas
                bool todosLosMesesConVentas = true;
                for (int mes = 0; mes < 12; ++mes) {
                    if (!ventaAnio.ventasEnAnio[mes].ventasEnMes) {
                        todosLosMesesConVentas = false;
                        break;
                    }
                }

                if (todosLosMesesConVentas) {
                    // Buscar o crear la canasta para este año
                    auto it = std::find_if(canastas.begin(), canastas.end(),
                        [&ventaAnio](const Canasta& c) { return c.anio == std::to_string(ventaAnio.year); });

                    Canasta* canasta;
                    if (it == canastas.end()) {
                        // Crear nueva canasta
                        canastas.emplace_back(std::to_string(ventaAnio.year));
                        canasta = &canastas.back();
                    } else {
                        canasta = &(*it);
                    }

                    // Agregar nombre e ID a la canasta
                    canasta->agregarNombre(producto.nombres[0]); // Asumiendo que usamos el primer nombre
                    canasta->agregarId(producto.id);

                    // Calcular y sumar precios para cada mes
                    for (int mes = 0; mes < 12; ++mes) {
                        const auto& ventaMes = ventaAnio.ventasEnAnio[mes];
                        if (ventaMes.sumatoriaCantidades > 0) {
                            double precioUnitario = ventaMes.sumatoriaMontos / ventaMes.sumatoriaCantidades;
                            canasta->precios[mes] += precioUnitario;
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    MapaProductos productos;//mapa completo todos los registros
    MapaProductos canastaMap;//solo productos que pertenecen a la canasta
    std::vector<Canasta> misCanastas;//vector con los datos importantes de canastas
    const int TAMANO_BLOQUE = 100000;//bloque para lectura csv
    Cola cola_bloques;//cola para bloques
    int cantidadBloques = 0;//sin usar, es para procesar mas de un bloque a la vez
    int agrupar = 0;//contador para imprimir bloques leidos
    std::vector<int> AñosCanastas; //contiene los años para los que existe una canasta
    std::vector<double> PreciosCanasta;//contiene los 12 precios de la canasta para un año, se reutiliza
    std::vector<double> paridadAño(12, 0.0);//contiene la paridad de los 12 meses del año, se reutiliza
    std::vector<int> diasMes(12, 0);
    
    if (argc < 2) {
        std::cout << "Uso: " << argv[0] << " <nombre_archivo_excel>" << std::endl;
        return 1;
    }
    std::string nombreArchivo = argv[1];
    
    
    std::streampos posicion_actual = 0; // Variable para almacenar la posición actual en el archivo
    descartarPrimeraLinea("pd.csv", posicion_actual);

    while (quedanLineasPorLeer("pd.csv", posicion_actual)) {
        leerCSV("pd.csv", cola_bloques, TAMANO_BLOQUE, posicion_actual);
        cantidadBloques++;
        agrupar++;

        if (agrupar == 10) {
            //std::cout << "Bloques leídos hasta ahora: " << cantidadBloques << std::endl;
            agrupar = 0; // Reiniciar el contador
            //if(cantidadBloques>500){imprimirMapa(productos);}
        }

        Bloque bloque;
        if (!cola_bloques.pop(bloque)) {
            std::cout << "Se han procesado todas las líneas del archivo." << std::endl;
            break;
        }
        procesarBloque(bloque, productos, canastaMap);
    }
    procesarMapaYCanastas(canastaMap, misCanastas);
    // guardar años canastas
    for (const auto& canasta : misCanastas) {
        AñosCanastas.push_back(std::stoi(canasta.anio));
    }
    //ordenar años
    std::sort(AñosCanastas.begin(), AñosCanastas.end());
 
    Book* book = xlCreateXMLBook();
    if (book) {
        if (book->load(nombreArchivo.c_str())) {
            Sheet* sheet = book->getSheet(0);  // Obtiene la primera hoja
            if (sheet) {
                // Obtener el nombre de la hoja (opcional)
                const char* sheetName = sheet->name();
                //std::cout << "Leyendo la hoja: " << (sheetName ? sheetName : "Sin nombre") << std::endl;

                // ... [El resto del código para procesar la hoja]
                for (double value : AñosCanastas) {
                    for(int i = 0; i <= 11; ++i){
                        paridadAño[i]=0.0;
                    }
                    for (const auto& canasta : misCanastas) {
                        if (value == std::stoi(canasta.anio)) {
                            // Cargar precios a un vector
                            PreciosCanasta.clear();
                            for (int i = 0; i < 12; ++i) {
                                PreciosCanasta.push_back(canasta.precios[i]);
                            }
                            
                            // Calcular inflación para este año
                            int yearObjetivo = std::stoi(canasta.anio);
                            //paridadAño.clear();
                            double fechaeXl ;
                            double precioeXl;
                            for (int row = 6; row <= 3733; ++row) {
                                //CellType cellType = sheet->cellType(row, col);
                                int year, month, day;
                                book->dateUnpack(sheet->readNum(row, 0), &year, &month, &day);
                                if(year==yearObjetivo){
                                    precioeXl= sheet->readNum(row, 1);
                                    paridadAño[month-1]+=precioeXl;
                                    diasMes[month-1]=diasMes[month-1]+1;
                                }
                            }
                            
                            for(int i = 0; i <= 11; ++i){
                                //paridadAño[i]=paridadAño[i]/diasMes[i];
                            }
                                                        // Imprimir resultados
                            std::cout << "Precios Canasta: ";
                            for (auto price : PreciosCanasta) {
                                std::cout << price << " ";
                            }
                            std::cout << std::endl;
                            
                            std::cout << "Paridad Año: ";
                            for (auto pari : paridadAño) {
                                std::cout << pari << " ";
                            }
                            std::cout << std::endl;
                        }
                    }
                }
            }
        }
    }
    
    std::cout << "finalized." << std::endl;
    return 0;
}


