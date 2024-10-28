#include <arpa/inet.h>
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <stdbool.h>
#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <unistd.h>     /* read, write, close */

#include "helpers.h"
#include "parson.h"
#include "requests.h"

typedef enum {
  CMD_REGISTER,
  CMD_LOGIN,
  CMD_ENTER_LIBRARY,
  CMD_GET_BOOKS,
  CMD_GET_BOOK,
  CMD_ADD_BOOK,
  CMD_DELETE_BOOK,
  CMD_LOGOUT,
  CMD_EXIT,
  CMD_INVALID
} Command;

Command get_command(const char *command) {
  if (!strcmp(command, "register")) return CMD_REGISTER;
  if (!strcmp(command, "login")) return CMD_LOGIN;
  if (!strcmp(command, "enter_library")) return CMD_ENTER_LIBRARY;
  if (!strcmp(command, "get_books")) return CMD_GET_BOOKS;
  if (!strcmp(command, "get_book")) return CMD_GET_BOOK;
  if (!strcmp(command, "add_book")) return CMD_ADD_BOOK;
  if (!strcmp(command, "delete_book")) return CMD_DELETE_BOOK;
  if (!strcmp(command, "logout")) return CMD_LOGOUT;
  if (!strcmp(command, "exit")) return CMD_EXIT;
  return CMD_INVALID;
}

bool register_user(char *host_ip, int port) {
  char username[101];
  char password[101];
  getchar();
  printf("username=");
  fgets(username, 101, stdin);
  username[strlen(username) - 1] = '\0';
  printf("password=");
  fgets(password, 101, stdin);
  password[strlen(password) - 1] = '\0';
  if (strchr(username, ' ') != NULL || strchr(password, ' ') != NULL) {
    printf("ERROR: Username and password must not contain spaces.\n");
    return false;
  } else {
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    json_object_set_string(root_object, "username", username);
    json_object_set_string(root_object, "password", password);
    char *message = json_serialize_to_string_pretty(root_value);
    char *response = NULL;
    int sockfd = open_connection(host_ip, port, AF_INET, SOCK_STREAM, 0);
    char *request =
        compute_post_request(host_ip, "/api/v1/tema/auth/register",
                             "application/json", &message, 1, NULL, 0, NULL);
    send_to_server(sockfd, request);
    response = receive_from_server(sockfd);
    if (strstr(response, "400 Bad Request") != NULL) {
      JSON_Value *response_value = json_parse_string(response);
      JSON_Object *response_object = json_value_get_object(response_value);
      const char *error_message =
          json_object_get_string(response_object, "error");
      if (error_message != NULL) {
        printf("ERROR: %s\n", error_message);
      } else {
        printf("ERROR: User already exists.\n");
        return false;
      }
      json_value_free(response_value);
    } else {
      printf("SUCCESS: Utilizator înregistrat cu succes!\n");
    }

    free(response);
    close_connection(sockfd);
    return true;
  }
}

bool login(char *host_ip, int port, char ***cookies, int *cookie_count) {
  char username[101];
  char password[101];
  printf("username=");
  scanf(" %[^\n]", username);
  printf("password=");
  scanf(" %[^\n]", password);

  if (strchr(username, ' ') != NULL || strchr(password, ' ') != NULL) {
    printf("ERROR: Username and password must not contain spaces.\n");
    return false;
  }

  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root_object = json_value_get_object(root_value);
  json_object_set_string(root_object, "username", username);
  json_object_set_string(root_object, "password", password);
  char *message = json_serialize_to_string_pretty(root_value);
  char *response = NULL;
  int sockfd = open_connection(host_ip, port, AF_INET, SOCK_STREAM, 0);
  char *request =
      compute_post_request(host_ip, "/api/v1/tema/auth/login",
                           "application/json", &message, 1, NULL, 0, NULL);

  send_to_server(sockfd, request);
  response = receive_from_server(sockfd);
  if (strstr(response, "400 Bad Request") != NULL) {
    JSON_Value *response_value = json_parse_string(strstr(response, "{"));
    JSON_Object *response_object = json_value_get_object(response_value);
    const char *error_message =
        json_object_get_string(response_object, "error");
    if (error_message != NULL) {
      printf("ERROR: %s\n", error_message);
    } else {
      printf("ERROR: Invalid credentials.\n");
    }
    json_value_free(response_value);
    free(response);
    close_connection(sockfd);
    return false;
  }

  if (strstr(response, "200 OK") != NULL) {
    char *set_cookie_header = strstr(response, "Set-Cookie: ");
    if (set_cookie_header != NULL) {
      set_cookie_header += strlen("Set-Cookie: ");
      char *cookie_end = strstr(set_cookie_header, "\r\n");
      size_t cookie_len = cookie_end - set_cookie_header;

      char *cookie_string = (char *)malloc(cookie_len + 1);
      strncpy(cookie_string, set_cookie_header, cookie_len);
      cookie_string[cookie_len] = '\0';

      int count = 0;
      char *temp = cookie_string;
      while ((temp = strstr(temp, "; ")) != NULL) {
        count++;
        temp += 2;
      }
      count++;

      *cookies = (char **)malloc(count * sizeof(char *));
      *cookie_count = count;

      int i = 0;
      char *token = strtok(cookie_string, "; ");
      while (token != NULL) {
        (*cookies)[i] = strdup(token);
        token = strtok(NULL, "; ");
        i++;
      }

      printf("SUCCESS: Utilizatorul a fost logat cu succes\n");
      free(cookie_string);
    }
  } else {
    printf("ERROR: Unexpected response.\n");
    free(response);
    close_connection(sockfd);
    return false;
  }

  free(response);
  close_connection(sockfd);
  return true;
}

bool enter_library(char *host_ip, int port, char **cookies, int cookie_count,
                   char **token) {
  char *response = NULL;
  int sockfd = open_connection(host_ip, port, AF_INET, SOCK_STREAM, 0);
  char *request = compute_get_request(host_ip, "/api/v1/tema/library/access",
                                      NULL, cookies, cookie_count, NULL);
  send_to_server(sockfd, request);
  response = receive_from_server(sockfd);
  if (strstr(response, "HTTP/1.1 200 OK") != NULL) {
    JSON_Value *response_value = json_parse_string(strstr(response, "{"));
    JSON_Object *response_object = json_value_get_object(response_value);
    const char *token_val = json_object_get_string(response_object, "token");
    if (token_val != NULL) {
      *token = (char *)malloc(strlen(token_val) + 1);
      strcpy(*token, token_val);
      printf("SUCCESS: Acces permis.\n");
    }
    json_value_free(response_value);
  } else {
    printf("ERROR: Acces interzis.\n");
    free(response);
    close_connection(sockfd);
    return false;
  }

  free(response);
  close_connection(sockfd);
  return true;
}

bool get_books(char *host_ip, int port, char **cookies, int cookie_count,
               char *token) {
  char *response = NULL;
  int sockfd = open_connection(host_ip, port, AF_INET, SOCK_STREAM, 0);
  char *request = compute_get_request(host_ip, "/api/v1/tema/library/books",
                                      NULL, NULL, 0, token);
  send_to_server(sockfd, request);
  response = receive_from_server(sockfd);
  if (strstr(response, "HTTP/1.1 200 OK") != NULL) {
    JSON_Value *response_value = json_parse_string(strstr(response, "["));
    JSON_Array *response_array = json_value_get_array(response_value);
    for (int i = 0; i < json_array_get_count(response_array); i++) {
      JSON_Object *book = json_array_get_object(response_array, i);
      printf("id=%d\n", (int)json_object_get_number(book, "id"));
    }
  } else {
    printf("ERROR: No books available at the moment!\n");
    free(response);
    close_connection(sockfd);
    return false;
  }

  free(response);
  close_connection(sockfd);
  return true;
}

bool get_book(char *host_ip, int port, char **cookies, int cookie_count,
              char *token, int book_id) {
  char *response = NULL;
  char url[100];
  snprintf(url, 100, "/api/v1/tema/library/books/%d", book_id);
  int sockfd = open_connection(host_ip, port, AF_INET, SOCK_STREAM, 0);
  char *request = compute_get_request(host_ip, url, NULL, NULL, 0, token);
  send_to_server(sockfd, request);
  response = receive_from_server(sockfd);
  if (strstr(response, "HTTP/1.1 200 OK") != NULL) {
    JSON_Value *response_value = json_parse_string(strstr(response, "{"));
    char *json_string = json_serialize_to_string_pretty(response_value);
    printf("%s\n", json_string);
    json_value_free(response_value);
  } else if (strstr(response, "HTTP/1.1 404 Not Found") != NULL) {
    printf("ERROR: Book not found.\n");
  } else {
    printf("ERROR: Acces interzis.\n");
  }

  free(response);
  close_connection(sockfd);
  return true;
}

bool add_book(char *host_ip, int port, char **cookies, int cookie_count,
              char *token) {
  char title[101], author[101], genre[101], publisher[101], page_count_str[101];
  int page_count;

  printf("title=");
  getchar();
  fgets(title, 101, stdin);
  title[strlen(title) - 1] = '\0';

  printf("author=");
  fgets(author, 101, stdin);
  author[strlen(author) - 1] = '\0';

  printf("genre=");
  fgets(genre, 101, stdin);
  genre[strlen(genre) - 1] = '\0';

  printf("publisher=");
  fgets(publisher, 101, stdin);
  publisher[strlen(publisher) - 1] = '\0';

  printf("page_count=");
  fgets(page_count_str, 101, stdin);
  page_count_str[strlen(page_count_str) - 1] = '\0';
  page_count = atoi(page_count_str);

  if (strcmp(title, "") == 0 || strcmp(author, "") == 0 ||
      strcmp(genre, "") == 0 || strcmp(publisher, "") == 0 ||
      strcmp(page_count_str, "") == 0) {
    printf("ERROR: Invalid input.\n");
    return false;
  }
  if (page_count <= 0) {
    printf("ERROR: Invalid page count.\n");
    return false;
  }

  if (token == NULL) {
    printf("ERROR: You do not have access to the library!\n");
    return false;
  }

  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root_object = json_value_get_object(root_value);
  json_object_set_string(root_object, "title", title);
  json_object_set_string(root_object, "author", author);
  json_object_set_string(root_object, "genre", genre);
  json_object_set_number(root_object, "page_count", page_count);
  json_object_set_string(root_object, "publisher", publisher);
  char *Payload = json_serialize_to_string_pretty(root_value);

  char *response = NULL;
  int sockfd = open_connection(host_ip, port, AF_INET, SOCK_STREAM, 0);
  char *request =
      compute_post_request(host_ip, "/api/v1/tema/library/books",
                           "application/json", &Payload, 1, NULL, 0, token);
  send_to_server(sockfd, request);
  response = receive_from_server(sockfd);

  if (strstr(response, "HTTP/1.1 200 OK") != NULL) {
    printf("SUCCESS: Cartea a fost adăugată cu succes.\n");
  } else {
    printf("ERROR: Adăugarea cărții a eșuat.\n");
  }

  free(Payload);
  free(response);
  json_value_free(root_value);
  close_connection(sockfd);
  return true;
}

bool delete_book(char *host_ip, int port, char **cookies, int cookie_count,
                 char *token, int book_id) {
  char *response = NULL;
  char url[100];
  snprintf(url, 100, "/api/v1/tema/library/books/%d", book_id);
  int sockfd = open_connection(host_ip, port, AF_INET, SOCK_STREAM, 0);
  char *request = compute_delete_request(host_ip, url, NULL, NULL, 0, token);
  send_to_server(sockfd, request);
  response = receive_from_server(sockfd);
  if (strstr(response, "HTTP/1.1 200 OK") != NULL) {
    printf("SUCCESS: Cartea cu id %d a fost ștearsă cu succes.\n", book_id);
  } else if (strstr(response, "HTTP/1.1 404 Not Found") != NULL) {
    printf("ERROR: Cartea cu id %d nu a fost găsită.\n", book_id);
  } else {
    printf("ERROR: Acces interzis.\n");
  }

  free(response);
  close_connection(sockfd);
  return true;
}

bool logout(char *host_ip, int port, char **cookies, int cookie_count) {
  char *response = NULL;
  int sockfd = open_connection(host_ip, port, AF_INET, SOCK_STREAM, 0);
  char *request = compute_get_request(host_ip, "/api/v1/tema/auth/logout", NULL,
                                      cookies, cookie_count, NULL);
  send_to_server(sockfd, request);
  response = receive_from_server(sockfd);
  if (strstr(response, "HTTP/1.1 200 OK") != NULL) {
    printf("Utilizatorul s-a delogat cu succes!\n");
  } else {
    printf("ERROR: Nu sunteți autentificat.\n");
  }

  free(response);
  close_connection(sockfd);
  return true;
}

int main(int argc, char *argv[]) {
  char command[51];
  int PORT = 8080;
  char HOST[15] = "34.246.184.49";
  char **cookies = NULL;
  int cookie_count = 0;
  char *token = NULL;

  while (1) {
    scanf(" %[^\n]", command);
    Command cmd = get_command(command);

    switch (cmd) {
      case CMD_REGISTER:
        register_user(HOST, PORT);
        break;
      case CMD_LOGIN:
        login(HOST, PORT, &cookies, &cookie_count);
        break;
      case CMD_ENTER_LIBRARY:
        if (cookies == NULL || cookie_count == 0) {
          printf("ERROR You do not have access to enter the library!\n");
          break;
        } else {
          enter_library(HOST, PORT, cookies, cookie_count, &token);
          break;
        }
      case CMD_GET_BOOKS:
        if (token == NULL) {
          printf("ERROR You do not have access to the library!\n");
          break;
        } else {
          get_books(HOST, PORT, cookies, cookie_count, token);
          break;
        }
      case CMD_GET_BOOK:
        if (token == NULL) {
          printf("ERROR You do not have access to the library!\n");
          break;
        } else {
          int book_id;
          printf("id=");
          scanf("%d", &book_id);
          get_book(HOST, PORT, cookies, cookie_count, token, book_id);
          break;
        }
      case CMD_ADD_BOOK:
        add_book(HOST, PORT, cookies, cookie_count, token);
        break;
      case CMD_DELETE_BOOK:
        if (token == NULL) {
          printf("ERROR You do not have access to the library!\n");
          break;
        } else {
          int book_id;
          printf("id=");
          scanf("%d", &book_id);
          delete_book(HOST, PORT, cookies, cookie_count, token, book_id);
          break;
        }
      case CMD_LOGOUT:
        if (cookies == NULL || cookie_count == 0) {
          printf("ERROR You are not logged in!\n");
          break;
        } else {
          logout(HOST, PORT, cookies, cookie_count);
          for (int i = 0; i < cookie_count; i++) {
            free(cookies[i]);
          }
          free(cookies);
          cookies = NULL;
          cookie_count = 0;
          free(token);
          token = NULL;
          break;
        }
      case CMD_EXIT:
        exit(0);
        break;
      case CMD_INVALID:
        printf("ERROR Invalid command.\n");
    }
    printf("\n");
  }

  if (cookies) {
    for (int i = 0; i < cookie_count; i++) {
      free(cookies[i]);
    }
    free(cookies);
  }

  return 0;
}
