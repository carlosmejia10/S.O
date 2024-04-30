/**************************************
Autores:  Carlos Mejia
          Juan Paez

Fecha: 28/04/2024
Tema: PROYECTO (Monitoreo de Sensores)
**************************************/
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    // Iniciando variables
    int a=0; // Variable para contar las mediciones
    int opt;
    int tipoSensor = 0; // Tipo de sensor
    int intervaloTiempo = 0; // Intervalo de tiempo entre mediciones
    char* nombreArchivo = nullptr; // Nombre del archivo de datos
    char* nombrePipe = nullptr; // Nombre del pipe

    // Revisando argumentos y asignándolos
    while ((opt = getopt(argc, argv, "s:t:f:p:")) != -1) {
        switch (opt) {
            case 's':
                tipoSensor = atoi(optarg); // Asigna el valor del argumento -s a tipoSensor
                break;
            case 't':
                intervaloTiempo = atoi(optarg); // Asigna el valor del argumento -t a intervaloTiempo
                break;
            case 'f':
                nombreArchivo = optarg; // Asigna el valor del argumento -f a nombreArchivo
                break;
            case 'p':
                nombrePipe = optarg; // Asigna el valor del argumento -p a nombrePipe
                break;
            default:
                std::cerr << "Uso: " << argv[0] << " -s tipoSensor -t intervaloTiempo -f nombreArchivo -p nombrePipe" << std::endl;
                return 1;
        }
    }

    // Abriendo archivo de lectura
    std::ifstream archivoDatos(nombreArchivo);
    if (!archivoDatos.is_open()) {
        std::cerr << "Error al abrir el archivo de datos: " << nombreArchivo << std::endl;
        return 1;
    }

    // Abriendo pipe
    int pipeFd;
    do {
        pipeFd = open(nombrePipe, O_WRONLY | O_NONBLOCK); // Abre el pipe en modo escritura no bloqueante
        if (pipeFd < 0) {
            std::cerr << "Error al abrir el pipe: " << nombrePipe << ", reintentando..." << std::endl;
            sleep(1); // Espera 1 segundo antes de intentar abrir el pipe nuevamente
        }
    } while (pipeFd < 0);

    // Leyendo archivo y escribiendo en pipe
    std::string linea;
    while (std::getline(archivoDatos, linea)) {
        a++; // Incrementa el contador de mediciones
        std::cout << "Medición " << a << ": ";
        write(pipeFd, linea.c_str(), linea.size() + 1); // Escribe la línea en el pipe
        std::cerr << linea << std::endl;
        sleep(intervaloTiempo); // Espera el intervalo de tiempo especificado antes de la siguiente medición
    }

    // Cerrando archivo y pipe
    archivoDatos.close();
    close(pipeFd);

    return 0;
}