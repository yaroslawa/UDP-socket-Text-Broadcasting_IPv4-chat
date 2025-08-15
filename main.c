#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
//#include <fcntl.h>
#define MESSAGE_SIZE 1000
#define NICKNAME_SIZE 256


// нужно выводить IP отправителя-клиента!!!!!

int serverSocket;
pthread_mutex_t printMutex = PTHREAD_MUTEX_INITIALIZER;

// Defining the IP and Port
char *ip = "255.255.255.255";
const int port = 8000;

volatile int running = 1;

void *serverThread(void *arg);
void *clientThread(void *arg);

int main()
{
  // Defining variables

  struct sockaddr_in serverAddress = {0};

  // Создвние UDP-сокета
  serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
  if (serverSocket < 0)
  {
    printf("[ERROR] socket error\n");
    exit(0);
  }
  // Включение широковещания
  // добавить обработку ошибки
  int broadcastEnable = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
  
  // Настройка адреса
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  // привязка сокета к IP-адресу и порту (для сервера)
  int reuse = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
  int e;
  e = bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
  if (e != 0)
  {
    printf("[ERROR] bind error\n");
    exit(0);
  }

  printf("[STARTING] UDP File Server started.\n");

  // создание потоков
  pthread_t tidServer, tidClient;

  if (pthread_create(&tidServer, NULL, serverThread, &serverAddress) != 0) {
        perror("[ERROR] pthread_create(server) failed.\n");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&tidClient, NULL, clientThread, &serverAddress) != 0) {
        perror("[ERROR] pthread_create(client) failed.\n");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

  // Ожидание завершения потоков
  pthread_join(tidServer, NULL);
  pthread_join(tidClient, NULL);

  printf("[CLOSING] Disconnecting from the server.\n");

  close(serverSocket);

  return 0;
}

void *serverThread(void *arg){
  struct sockaddr_in serverAddress = *(struct sockaddr_in*)arg;
  char fullmessage[MESSAGE_SIZE+NICKNAME_SIZE+4];
  struct sockaddr_in clientAddress;
  int len = sizeof(clientAddress);

  while(running){
    // добавить проверку на получение
    recvfrom(serverSocket, fullmessage, sizeof(fullmessage), 0, (struct sockaddr*)&clientAddress, &len);
    
    pthread_mutex_lock(&printMutex);

    printf("\n[YOU GOT A NEW MESSAGE]\n");
    if (strcmp(fullmessage, "/EXIT") == 0){
      printf("[EXIT] Exit command received.\n");
      running = 0;
      break;
    }
    printf("%s\n", fullmessage);

    printf("Enter your message: ");
    fflush(stdout);
    pthread_mutex_unlock(&printMutex);
  }
  return NULL;
}

void *clientThread(void *arg){
  struct sockaddr_in serverAddress = *(struct sockaddr_in*)arg;
  char message[MESSAGE_SIZE], nickname[NICKNAME_SIZE], fullMessage[MESSAGE_SIZE+NICKNAME_SIZE+4];
  
  struct sockaddr_in clientAddress;
  clientAddress.sin_family = AF_INET;
  clientAddress.sin_port = htons(port);
  inet_pton(AF_INET, ip, &clientAddress.sin_addr);

  printf("Enter your nickname: ");
    if (fgets(nickname, NICKNAME_SIZE, stdin) == NULL) {
        perror("[ERROR] nickname input failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    nickname[strcspn(nickname, "\n")] = '\0';
  
  while(running){
    printf("Enter your message: ");
    if (fgets(message, MESSAGE_SIZE, stdin) == NULL) {
        perror("[ERROR] message input failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    // Удаляем символ новой строки
    message[strcspn(message, "\n")] = '\0';
    int fl;
    if (strcmp(message, "/EXIT") == 0){
      snprintf(fullMessage, sizeof(fullMessage), "/EXIT\n");
      fl = sendto(serverSocket, fullMessage, strlen(fullMessage) + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
      if (fl == -1) perror("[message ERROR] sendto()");
      running = 0;
      break;
    }
    snprintf(fullMessage, sizeof(fullMessage), "%s: %s\n", nickname, message);
    fl = sendto(serverSocket, fullMessage, strlen(fullMessage) + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
    if (fl == -1) perror("[message ERROR] sendto()");
   
  }
    

  return NULL;
}