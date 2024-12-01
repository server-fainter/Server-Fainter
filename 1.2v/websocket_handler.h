#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include "client_manager.h"

ssize_t process_websocket_frame(ClientManager *manager);

#endif // WEBSOCKET_HANDLER_H