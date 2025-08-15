#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <fcntl.h>
#define MESSAGE_SIZE 1000
#define NICKNAME_SIZE 256


int serverSocket;
pthread_mutex_t printMutex = PTHREAD_MUTEX_INITIALIZER;

volatile int running = 1;

struct ThreadArgs{
  struct sockaddr_in addr;
  char nickname[NICKNAME_SIZE];
  char ip[INET_ADDRSTRLEN];
  int port;
};

void *serverThread(void *arg);
void *clientThread(void *arg);

int main(int argc, char *argv[])
{
  if (argc != 3){
    fprintf(stderr, "[ERROR] Number of arguments is incorrect.\n");
    exit(EXIT_FAILURE);
  }

  struct ThreadArgs args;
  strncpy(args.ip, argv[1], sizeof(args.ip)-1);
  args.port = atoi(argv[2]);
  //args.addr = {0};

  //struct sockaddr_in serverAddress = {0};

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

  // привязка сокета к IP-адресу и порту (для сервера)
  int reuse = 1;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

  // Настройка адреса
  args.addr.sin_family = AF_INET;
  args.addr.sin_port = htons(args.port);
  args.addr.sin_addr.s_addr = INADDR_ANY;

  int e;
  e = bind(serverSocket, (struct sockaddr*)&args.addr, sizeof(args.addr));
  if (e != 0)
  {
    printf("[ERROR] bind error\n");
    exit(0);
  }

  printf("[STARTING] UDP File Server started.\n");

  
  //args.addr = serverAddress;

  printf("Enter your nickname: ");
  if (fgets(args.nickname, NICKNAME_SIZE, stdin) == NULL) {
        perror("[ERROR] nickname input failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
  args.nickname[strcspn(args.nickname, "\n")] = '\0';

  // создание потоков
  pthread_t tidServer, tidClient;

  if (pthread_create(&tidServer, NULL, serverThread, &args.addr) != 0) {
        perror("[ERROR] pthread_create(server) failed.\n");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&tidClient, NULL, clientThread, &args) != 0) {
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
  //struct ThreadArgs *args = (struct ThreadArgs*)arg;
  char fullmessage[MESSAGE_SIZE+NICKNAME_SIZE+4];
  struct sockaddr_in clientAddress;
  int len = sizeof(clientAddress);

  while(running){
    // добавить проверку на получение
    recvfrom(serverSocket, fullmessage, sizeof(fullmessage), 0, (struct sockaddr*)&clientAddress, &len);
    
    pthread_mutex_lock(&printMutex);

    if (strcmp(fullmessage, "/EXIT") == 0){
      printf("[EXIT] Exit command received.\n");
      running = 0;
      break;
    }
    printf("\n[NEW MESSAGE FROM IP %s]\n%s\n", inet_ntoa(clientAddress.sin_addr), fullmessage);

    printf("Enter your message: ");
    fflush(stdout);
    pthread_mutex_unlock(&printMutex);
  }
  return NULL;
}

void *clientThread(void *arg){
  struct ThreadArgs *args = (struct ThreadArgs*)arg;
  char message[MESSAGE_SIZE], fullMessage[MESSAGE_SIZE+NICKNAME_SIZE+4];
  
  struct sockaddr_in clientAddress;
  clientAddress.sin_family = AF_INET;
  clientAddress.sin_port = htons(args->port);
  clientAddress.sin_addr.s_addr = INADDR_BROADCAST;
  //inet_pton(AF_INET, args->ip, &clientAddress.sin_addr);
  
  while(running){
    pthread_mutex_lock(&printMutex);
    printf("Enter your message: ");
    fflush(stdout);
    pthread_mutex_unlock(&printMutex);
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
    snprintf(fullMessage, sizeof(fullMessage), "%s: %s\n", args->nickname, message);
    fl = sendto(serverSocket, fullMessage, strlen(fullMessage) + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
    if (fl == -1) perror("[message ERROR] sendto()");
   
  }
    

  return NULL;
}