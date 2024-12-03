CC = gcc
CFLAGS = -pthread -Wall -g

# 소스 파일
SERVER_SRCS = main.c server.c clientmanager.c task_queue.c thread_pool.c
CLIENT_SRCS = client.c

# 오브젝트 파일
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# 실행 파일 이름
SERVER_TARGET = server
CLIENT_TARGET = client

# 서버 빌드
$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS)

# 클라이언트 빌드
$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS)

# 개별 소스 파일 컴파일
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 클린 규칙
clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) $(SERVER_TARGET) $(CLIENT_TARGET)