#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <asm-generic/socket.h>

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

void *serverThread();
void *clientThread(void *arg);

int main(int argc, const char *argv[])
{
  // Проверка передаваемых аргументов
  if (argc != 3){
    fprintf(stderr, "[ERROR] Number of arguments is incorrect.\n");
    exit(EXIT_FAILURE);
  }

  // Заполнение полей структуры из данных полученных при запуске
  struct ThreadArgs args;
  strncpy(args.ip, argv[1], sizeof(args.ip)-1);
  args.port = atoi(argv[2]);

  // Создвние UDP-сокета
  serverSocket = socket(AF_INET, SOCK_DGRAM, 0);

  // Проверка что сокет создан 
  if (serverSocket < 0)
  {
    printf("[ERROR] socket error\n");
    exit(0);
  }

  // Включение широковещания
  int broadcastEnable = 1;
  if (setsockopt(serverSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) == -1){
    perror("[ERROR] setting broadcast failed.\n");
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  // Настройка переиспользования адреса и порта (для запуска на одном компьютере в разных терминалах)
  int reuse = 1;
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
    perror("[ERROR] setting address reusing failed.\n");
    close(serverSocket);
    exit(EXIT_FAILURE);
}
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1){
    perror("[ERROR] setting port reusing failed.\n");
    close(serverSocket);
    exit(EXIT_FAILURE);
  }
  
  // Настройка полей структуры sockaddr_in
  args.addr.sin_family = AF_INET;
  args.addr.sin_port = htons(args.port);
  args.addr.sin_addr.s_addr = INADDR_ANY;

  // Привязка сокета к IP-адресу и порту (для сервера)
  int e;
  e = bind(serverSocket, (struct sockaddr*)&args.addr, sizeof(args.addr));
  if (e != 0)
  {
    printf("[ERROR] bind error\n");
    exit(0);
  }

  printf("[STARTING] UDP File Server started.\n\n");

  // Ввод никнейма (один раз)
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

  // Закрытие сокета
  close(serverSocket);

  return 0;
}

void *serverThread(){
  char fullmessage[MESSAGE_SIZE+NICKNAME_SIZE+4];
  struct sockaddr_in clientAddress;
  socklen_t len = sizeof(clientAddress);

  // Контроль потоков ввода-вывода через мьютексы
  pthread_mutex_lock(&printMutex);
  printf("Enter your message: ");
  fflush(stdout);
  pthread_mutex_unlock(&printMutex);

  while(running){
    // Получение сообщения
    int n = recvfrom(serverSocket, fullmessage, sizeof(fullmessage), 0, (struct sockaddr*)&clientAddress, &len);
    if (n < 0) {
      perror("[ERROR] Recieving failed.\n");
      continue;
    }
    
    pthread_mutex_lock(&printMutex);

    // Обработка сообщения о завершении программы
    if (strcmp(fullmessage, "/EXIT") == 0){
      running = 0;
      pthread_mutex_unlock(&printMutex);
      break;
    }
    else{
      // Очистить текущую строку (чтобы избежать дублирования приглашения ввести сообщение)
      printf("\r\033[K");
      printf("\n[NEW MESSAGE FROM IP %s]\n%s\n", inet_ntoa(clientAddress.sin_addr), fullmessage);
      printf("Enter your message: ");
      fflush(stdout);
    }
    
    pthread_mutex_unlock(&printMutex);
  }
  return NULL;
}

void *clientThread(void *arg){
  // Получение указателя на переданную при создании потока структуру
  struct ThreadArgs *args = (struct ThreadArgs*)arg;

  char message[MESSAGE_SIZE], fullMessage[MESSAGE_SIZE+NICKNAME_SIZE+4];
  struct sockaddr_in clientAddress;

  // Настройка полей структуры sockaddr_in для стороны клиента
  clientAddress.sin_family = AF_INET;
  clientAddress.sin_port = htons(args->port);
  // Отправка будет на широковещательный адрес
  clientAddress.sin_addr.s_addr = INADDR_BROADCAST;
  
  while(running){
    // Ввод сообщения
    if (fgets(message, MESSAGE_SIZE, stdin) == NULL) {
        perror("[ERROR] message input failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    // Удаляем символ новой строки
    message[strcspn(message, "\n")] = '\0';

    int fl;
    // Обработка сообщения о завершении работы
    if (strcmp(message, "/EXIT") == 0){
      snprintf(fullMessage, sizeof(fullMessage), "/EXIT");
      // Отправка сообщения о завершении работы
      fl = sendto(serverSocket, fullMessage, strlen(fullMessage) + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
      if (fl == -1) perror("[message ERROR] sendto()");
      running = 0;
      break;
    }
    // Объединение никнейма и сообщения в одну строку
    snprintf(fullMessage, sizeof(fullMessage), "%s: %s\n", args->nickname, message);
    // Отправка сообщения
    fl = sendto(serverSocket, fullMessage, strlen(fullMessage) + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
    if (fl == -1) perror("[message ERROR] sendto()");
  }
  return NULL;
}