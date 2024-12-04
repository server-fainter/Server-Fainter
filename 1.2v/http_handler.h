// http_handler.h
#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include "client_manager.h"

void handle_http_request(ClientManager *manager, Client *client);
bool is_complete_http_request(const char *buffer);
bool is_http_request(const char *data, size_t length);

#endif // HTTP_HANDLER_H
