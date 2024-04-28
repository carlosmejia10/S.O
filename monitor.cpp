/*
Nombres:  Nicolas Sanchez
          Miguel Duarte

Fecha: 27/04/2024
Tema: Monitoreo de Sensores
*/
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include "buffer.cpp"


struct ThreadArgs {
    Buffer* bufferPh;
    Buffer* bufferTemp;
    char* pipeName;
    sem_t sem;
};

bool is_float(const std::string& str) {
    try {
        std::size_t pos;
        std::stof(str, &pos);
        // La conversión tiene éxito si no hay caracteres restantes en la cadena
        return pos == str.size();
    } catch (...) {
        return false;
    }
}

// Función para verificar si una cadena representa un número entero
bool is_integer(const std::string& str) {
    try {
        std::size_t pos;
        std::stoi(str, &pos);
        // La conversión tiene éxito si no hay caracteres restantes en la cadena
        return pos == str.size();
    } catch (...) {
        return false;
    }
}
//Funcion para obtener la hora actual
std::string getCurrentTime() {
    std::time_t currentTime = std::time(nullptr);
    std::tm* localTime = std::localtime(&currentTime);
    char timeString[100];
    std::strftime(timeString, sizeof(timeString), "%H:%M:%S", localTime);
    return std::string(timeString);
}
// Funcion para recolectar datos de los sensores y manejarlos entre Hilos
void* h_recolector(void* arg) {

    ThreadArgs* args = (ThreadArgs*)arg;
    Buffer* bufferPh = args->bufferPh;
    Buffer* bufferTemp = args->bufferTemp;
    char* pipeName = args->pipeName;

    // Abrir el Pipe
    int pipeFd = open(pipeName, O_RDONLY);
    if (pipeFd < 0) {
        sem_post(&args->sem);
        std::cerr << "Failed to open pipe: " << pipeName << std::endl;
        return NULL;
    }
    // Leer datos del pipe
    std::string line;
    while (true) {
        char buffer[256];
        int bytesRead = read(pipeFd, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            // El sensor no esta conectado, esperando 10 segundos
            sleep(10);
            // Enviando mensajes a los otros hilos para que terminen
            bufferPh->add("-1");
            bufferTemp->add("-1");
            // Borrando pipe y terminando el proceso
            unlink(args->pipeName);
            std::cout << "Finished processing measurements" << std::endl;
            break;
        }
        // Procesar la linea y agregar a los buffers
        buffer[bytesRead] = '\0';
        line = buffer;
        // Revisando si la linea es un entero
        if (is_integer(line)) {
            int value = std::stoi(line);
            if (value >= 0) {
                bufferTemp->add(line);
            } else {
                std::cerr << "Error: received negative value from sensor" << std::endl;
            }
        }
        // Revisando si la linea es un flotante
        else if (is_float(line)) {
            float value = std::stof(line);
            if (value >= 0.0) {
                bufferPh->add(line);
            } else {
                std::cerr << "Error: received negative value from sensor" << std::endl;
            }
        }
        else {
            std::cerr << "Error: received invalid value from sensor" << std::endl;
        }
    }
    // Cerrando el pipe
    close(pipeFd);

    return NULL;
}

void* h_ph(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    Buffer* bufferPh = args->bufferPh;
    int sem_val;
    sem_getvalue(&args->sem, &sem_val);
    // Abrir el archivo
    std::ofstream filePh("file-ph.txt");
    if (!filePh.is_open()) {
        std::cerr << "Failed to open file: file-ph.txt" << std::endl;
        return NULL;
    }

    if (sem_val > 0){
        filePh.close();
        bufferPh->~Buffer();
        return NULL;
    }

    // Escritura de datos del buffer al archivo
    std::string data;
  int a=0;
    while ((data = bufferPh->remove()) != "-1") {
      a++;
        float value = std::stof(data);
        if(value >=8.0 || value <= 6.0){
            std::cout << "Alerta: el valor de ph es: " << value << std::endl;
        }
        filePh <<"Medicion de PH "<< a <<": "<< value << " ||" << " " << "Hora de la medicion: " << getCurrentTime() << std::endl;
    }

    // Cerrando archivo
    filePh.close();
    bufferPh->~Buffer();

    return NULL;
}

void* h_temperatura(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    Buffer* bufferTemp = args->bufferTemp;
    int sem_val;
    sem_getvalue(&args->sem, &sem_val);
    // Abrir el archivo
    std::ofstream fileTemp("file-temp.txt");
    if (!fileTemp.is_open()) {
        std::cerr << "Failed to open file: file-temp.txt" << std::endl;
        return NULL;
    }
    if (sem_val > 0){
        fileTemp.close();
        bufferTemp->~Buffer();
        return NULL;
    }
    // Escribir datos del buffer al archivo
    std::string data;
  int b=0;
    while ((data = bufferTemp->remove()) != "-1") {
        b++;
        int value = std::stoi(data);
        if(value >=31.6 || value <= 20){
            std::cout << "Alerta: el valor de temperatura es: " << value << std::endl;
        }
        fileTemp <<"Medicion de temperatura "<< b <<": "<< value << " ||" << " " <<"Hora de la medicion: " << getCurrentTime() << std::endl;
    }

    // Cerrando archivo
    fileTemp.close();
    bufferTemp->~Buffer();
    return NULL;
}

int main(int argc, char *argv[]) {
    // Iniciando variables

    int opt;
    int bufferSize = 0;
    char* fileTemp = nullptr;
    char* filePh = nullptr;
    char* pipeName = nullptr;
    //Revisando argumentos y asignandolos
    while ((opt = getopt(argc, argv, "b:t:h:p:")) != -1) {
        switch (opt) {
            case 'b':
                bufferSize = atoi(optarg);
                break;
            case 't':
                fileTemp = optarg;
                break;
            case 'h':
                filePh = optarg;
                break;
            case 'p':
                pipeName = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -b bufferSize -t fileTemp -h filePh -p pipeName" << std::endl;
                return 1;
        }
    }

    // Creando Pipe
    if (mkfifo(pipeName, 0666) < 0) {
        std::cerr << "Failed to create pipe: " << pipeName << std::endl;
        return 1;
    }

    // Abriendo pipe
    int pipeFd = open(pipeName, O_RDONLY);
    if (pipeFd < 0) {
        std::cerr << "Failed to open pipe: " << pipeName << std::endl;
        return 1;
    }

    // Creando buffers
    Buffer bufferPh(bufferSize);
    Buffer bufferTemp(bufferSize);

    ThreadArgs args;
    args.bufferPh = &bufferPh;
    args.bufferTemp = &bufferTemp;
    args.pipeName = pipeName;
    sem_init(&args.sem, 0, 0);
    // Creando Hilos
    pthread_t threadRecolector, threadPh, threadTemp;
    pthread_create(&threadRecolector, NULL, h_recolector, &args);
    pthread_create(&threadPh, NULL, h_ph, &args);
    pthread_create(&threadTemp, NULL, h_temperatura, &args);
    // Uniendo Hilos
    pthread_join(threadRecolector, NULL);
    pthread_join(threadPh, NULL);
    pthread_join(threadTemp, NULL);

    // Cerrando pipe y destru-yendo semaforo
    close(pipeFd);
    sem_destroy(&args.sem);
    return 0;
}