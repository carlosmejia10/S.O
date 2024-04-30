/**************************************
Autores:  Carlos Mejia
          Juan Paez

Fecha: 28/04/2024
Tema: PROYECTO (Monitoreo de Sensores)
**************************************/
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include "buffer.h"

// Define una estructura para los argumentos del hilo
struct ArgumentosHilo
{
    Buffer *bufferPh;
    Buffer *bufferTemp;
    char *nombreTuberia;
    sem_t sem;
};

bool es_flotante(const std::string &str)
{ // Intenta convertir la cadena a un flotante
    // Si la conversión es exitosa y no quedan caracteres en la cadena, devuelve true
    // Si la conversión falla, devuelve false
    try
    {
        std::size_t pos;
        std::stof(str, &pos);
        // La conversión tiene éxito si no hay caracteres restantes en la cadena
        return pos == str.size();
    }
    catch (...)
    {
        return false;
    }
}

// Función para verificar si una cadena representa un número entero
bool es_entero(const std::string &str)
{ // Intenta convertir la cadena a un entero
    // Si la conversión es exitosa y no quedan caracteres en la cadena, devuelve true
    // Si la conversión falla, devuelve false
    try
    {
        std::size_t pos;
        std::stoi(str, &pos);
        // La conversión tiene éxito si no hay caracteres restantes en la cadena
        return pos == str.size();
    }
    catch (...)
    {
        return false;
    }
}
// Funcion para obtener la hora actual
std::string obtenerHoraActual()
{ // Obtiene la hora actual y la convierte a una cadena en formato HH:MM:SS
    std::time_t horaActual = std::time(nullptr);
    std::tm *horaLocal = std::localtime(&horaActual);
    char cadenaHora[100];
    std::strftime(cadenaHora, sizeof(cadenaHora), "%H:%M:%S", horaLocal);
    return std::string(cadenaHora);
}
// Funcion para recolectar datos de los sensores y manejarlos entre Hilos
void *h_recolector(void *arg)
{ // Recibe los argumentos del hilo, que incluyen los buffers y el nombre de la tubería
    // Abre la tubería y lee datos de ella
    // Si la lectura falla, envía un mensaje a los otros hilos para que terminen y luego termina
    // Si la lectura es exitosa, procesa la línea y la agrega a los buffers correspondientes
    // Si la línea no es un número válido, imprime un error
    // Al final, cierra la tubería

    ArgumentosHilo *argumentos = (ArgumentosHilo *)arg;
    Buffer *bufferPh = argumentos->bufferPh;
    Buffer *bufferTemp = argumentos->bufferTemp;
    char *nombreTuberia = argumentos->nombreTuberia;

    // Abrir el Pipe
    int tuberiaFd = open(nombreTuberia, O_RDONLY);
    if (tuberiaFd < 0)
    {
        sem_post(&argumentos->sem);
        std::cerr << "Fallo al abrir la tuberia: " << nombreTuberia << std::endl;
        return NULL;
    }
    // Leer datos del pipe
    std::string linea;
    while (true)
    {
        char buffer[256];
        int bytesLeidos = read(tuberiaFd, buffer, sizeof(buffer) - 1);
        if (bytesLeidos <= 0)
        {
            // El sensor no esta conectado, esperando 10 segundos
            sleep(10);
            // Enviando mensajes a los otros hilos para que terminen
            bufferPh->add("-1");
            bufferTemp->add("-1");
            // Borrando pipe y terminando el proceso
            unlink(argumentos->nombreTuberia);
            std::cout << "Terminado el procesamiento de mediciones" << std::endl;
            break;
        }
        // Procesar la linea y agregar a los buffers
        buffer[bytesLeidos] = '\0';
        linea = buffer;
        // Revisando si la linea es un entero
        if (es_entero(linea))
        {
            int valor = std::stoi(linea);
            if (valor >= 0)
            {
                bufferTemp->add(linea);
            }
            else
            {
                std::cerr << "Error: se recibió un valor negativo del sensor" << std::endl;
            }
        }
        // Revisando si la linea es un flotante
        else if (es_flotante(linea))
        {
            float valor = std::stof(linea);
            if (valor >= 0.0)
            {
                bufferPh->add(linea);
            }
            else
            {
                std::cerr << "Error: se recibió un valor negativo del sensor" << std::endl;
            }
        }
        else
        {
            std::cerr << "Error: se recibió un valor inválido del sensor" << std::endl;
        }
    }
    // Cerrando el pipe
    close(tuberiaFd);

    return NULL;
}

void *h_ph(void *arg)
{ // Recibe los argumentos del hilo, que incluyen el buffer y el semáforo
    // Abre el archivo para escribir los datos del pH
    // Si no puede abrir el archivo, imprime un error y termina
    // Escribe los datos del buffer al archivo, junto con la hora actual
    // Si el valor del pH está fuera del rango normal, imprime una alerta
    // Al final, cierra el archivo y destruye el buffer
    ArgumentosHilo *argumentos = (ArgumentosHilo *)arg;
    Buffer *bufferPh = argumentos->bufferPh;
    int valor_sem;
    sem_getvalue(&argumentos->sem, &valor_sem);
    // Abrir el archivo
    std::ofstream archivoPh("archivo-ph.txt");
    if (!archivoPh.is_open())
    {
        std::cerr << "Fallo al abrir el archivo: archivo-ph.txt" << std::endl;
        return NULL;
    }

    if (valor_sem > 0)
    {
        archivoPh.close();
        bufferPh->~Buffer();
        return NULL;
    }

    // Escritura de datos del buffer al archivo
    std::string datos;
    int a = 0;
    while ((datos = bufferPh->remove()) != "-1")
    {
        a++;
        float valor = std::stof(datos);
        if (valor >= 8.0 || valor <= 6.0)
        {
            std::cout << "Alerta: el valor de ph es: " << valor << std::endl;
        }
        archivoPh << "Medicion de PH " << a << ": " << valor << " ||"
                  << " "
                  << "Hora de la medicion: " << obtenerHoraActual() << std::endl;
    }

    // Cerrando archivo
    archivoPh.close();
    bufferPh->~Buffer();

    return NULL;
}

void *h_temperatura(void *arg)
{ // Similar a h_ph, pero para los datos de temperatura
    ArgumentosHilo *argumentos = (ArgumentosHilo *)arg;
    Buffer *bufferTemp = argumentos->bufferTemp;
    int valor_sem;
    sem_getvalue(&argumentos->sem, &valor_sem);
    // Abrir el archivo
    std::ofstream archivoTemp("archivo-temp.txt");
    if (!archivoTemp.is_open())
    {
        std::cerr << "Fallo al abrir el archivo: archivo-temp.txt" << std::endl;
        return NULL;
    }
    if (valor_sem > 0)
    {
        archivoTemp.close();
        bufferTemp->~Buffer();
        return NULL;
    }
    // Escribir datos del buffer al archivo
    std::string datos;
    int b = 0;
    while ((datos = bufferTemp->remove()) != "-1")
    {
        b++;
        int valor = std::stoi(datos);
        if (valor >= 31.6 || valor <= 20)
        {
            std::cout << "Alerta: el valor de temperatura es: " << valor << std::endl;
        }
        archivoTemp << "Medicion de temperatura " << b << ": " << valor << " ||"
                    << " "
                    << "Hora de la medicion: " << obtenerHoraActual() << std::endl;
    }

    // Cerrando archivo
    archivoTemp.close();
    bufferTemp->~Buffer();
    return NULL;
}

int main(int argc, char *argv[])
{

    // Inicia las variables y procesa los argumentos de la línea de comandos
    // Crea la tubería y abre la tubería
    // Si falla, imprime un error y termina
    // Crea los buffers y los argumentos del hilo
    // Crea los hilos y los une
    // Al final, cierra la tubería y destruye el semáforo

    int opt;
    int tamañoBuffer = 0;
    char *archivoTemp = nullptr;
    char *archivoPh = nullptr;
    char *nombreTuberia = nullptr;
    // Revisando argumentos y asignandolos
    while ((opt = getopt(argc, argv, "b:t:h:p:")) != -1)
    {
        switch (opt)
        {
        case 'b':
            tamañoBuffer = atoi(optarg);
            break;
        case 't':
            archivoTemp = optarg;
            break;
        case 'h':
            archivoPh = optarg;
            break;
        case 'p':
            nombreTuberia = optarg;
            break;
        default:
            std::cerr << "Uso: " << argv[0] << " -b tamañoBuffer -t archivoTemp -h archivoPh -p nombreTuberia" << std::endl;
            return 1;
        }
    }

    // Creando Pipe
    if (mkfifo(nombreTuberia, 0666) < 0)
    {
        std::cerr << "Fallo al crear la tuberia: " << nombreTuberia << std::endl;
        return 1;
    }

    // Abriendo pipe
    int tuberiaFd = open(nombreTuberia, O_RDONLY);
    if (tuberiaFd < 0)
    {
        std::cerr << "Fallo al abrir la tuberia: " << nombreTuberia << std::endl;
        return 1;
    }

    // Creando buffers
    Buffer bufferPh(tamañoBuffer);
    Buffer bufferTemp(tamañoBuffer);

    ArgumentosHilo argumentos;
    argumentos.bufferPh = &bufferPh;
    argumentos.bufferTemp = &bufferTemp;
    argumentos.nombreTuberia = nombreTuberia;
    sem_init(&argumentos.sem, 0, 0);
    // Creando Hilos
    pthread_t hiloRecolector, hiloPh, hiloTemp;
    pthread_create(&hiloRecolector, NULL, h_recolector, &argumentos);
    pthread_create(&hiloPh, NULL, h_ph, &argumentos);
    pthread_create(&hiloTemp, NULL, h_temperatura, &argumentos);
    // Uniendo Hilos
    pthread_join(hiloRecolector, NULL);
    pthread_join(hiloPh, NULL);
    pthread_join(hiloTemp, NULL);

    // Cerrando pipe y destru-yendo semaforo
    close(tuberiaFd);
    sem_destroy(&argumentos.sem);
    return 0;
}