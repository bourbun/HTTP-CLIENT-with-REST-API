#include "requests.h"

#include <arpa/inet.h>
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <stdio.h>
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <unistd.h>     /* read, write, close */

#include "helpers.h"

char *compute_get_request(char *host, char *url, char *query_params,
                          char **cookies, int cookies_count, char *token) {
  char *message = (char *)calloc(BUFLEN, sizeof(char));
  char *line = (char *)calloc(LINELEN, sizeof(char));

  // Step 1: write the method name, URL, request params (if any) and protocol
  // type
  if (query_params != NULL) {
    sprintf(line, "GET %s?%s HTTP/1.1", url, query_params);
  } else {
    sprintf(line, "GET %s HTTP/1.1", url);
  }

  compute_message(message, line);

  // Step 2: add the host
  memset(line, 0, LINELEN);
  sprintf(line, "Host: %s", host);
  compute_message(message, line);

  // Step 3 (optional): add headers and/or cookies, according to the protocol
  // format
  if (cookies != NULL) {
    for (int i = 0; i < cookies_count; i++) {
      memset(line, 0, LINELEN);
      sprintf(line, "Cookie: %s", cookies[i]);
      compute_message(message, line);
    }
  }
  if (token != NULL) {
    strcpy(line, "Authorization: Bearer ");
    strcat(line, token);
    compute_message(message, line);
  }
  // Step 4: add final new line
  compute_message(message, "");
  return message;
}

char *compute_post_request(char *host, char *url, char *content_type,
                           char **body_data, int body_data_fields_count,
                           char **cookies, int cookies_count, char *token) {
  char *message = calloc(BUFLEN, sizeof(char));
  char *line = calloc(LINELEN, sizeof(char));
  char *body_data_buffer = calloc(LINELEN, sizeof(char));

  // Step 1: write the method name, URL and protocol type
  sprintf(line, "POST %s HTTP/1.1", url);
  compute_message(message, line);

  // Step 2: add the host
  memset(line, 0, LINELEN);
  sprintf(line, "Host: %s", host);
  compute_message(message, line);

  /* Step 3: add necessary headers (Content-Type and Content-Length are
     mandatory) in order to write Content-Length you must first compute the
     message size
  */

  int size = 0;
  for (int i = 0; i < body_data_fields_count - 1; i++) {
    strcat(body_data_buffer, body_data[i]);
    strcat(body_data_buffer, "&");
    size += strlen(body_data[i]);
    size += 1;
  }
  strcat(body_data_buffer, body_data[body_data_fields_count - 1]);
  size += strlen(body_data[body_data_fields_count - 1]);

  sprintf(line, "Content-Type: %s\r\nContent-Length: %d", content_type, size);
  compute_message(message, line);

  // Step 4 (optional): add cookies
  if (cookies != NULL) {
    sprintf(line, "Cookie: %s", cookies[0]);
    compute_message(message, line);
    for (int i = 0; i < cookies_count; i++) {
      memset(line, 0, LINELEN);
      sprintf(line, "%s", cookies[i]);
      compute_message(message, line);
    }
  }
  if (token != NULL) {
    strcpy(line, "Authorization: Bearer ");
    strcat(line, token);
    compute_message(message, line);
  }
  // Step 5: add new line at end of header
  compute_message(message, "");

  // Step 6: add the actual payload data

  memset(line, 0, LINELEN);
  strcat(message, body_data_buffer);

  free(line);
  return message;
}

char *compute_delete_request(char *host, char *url, char *query_params,
                             char **cookies, int cookies_count, char *token) {
  char *message = (char *)calloc(BUFLEN, sizeof(char));
  char *line = (char *)calloc(LINELEN, sizeof(char));

  if (query_params != NULL) {
    sprintf(line, "DELETE %s?%s HTTP/1.1", url, query_params);
  } else {
    sprintf(line, "DELETE %s HTTP/1.1", url);
  }

  compute_message(message, line);

  sprintf(line, "Host: %s", host);
  compute_message(message, line);

  if (cookies != NULL) {
    for (int i = 0; i < cookies_count; i++) {
      memset(line, 0, LINELEN);
      sprintf(line, "%s", cookies[i]);
      compute_message(message, line);
    }
  }
  if (token != NULL) {
    strcpy(line, "Authorization: Bearer ");
    strcat(line, token);
    compute_message(message, line);
  }
  compute_message(message, "");
  return message;
}
