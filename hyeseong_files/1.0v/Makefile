# 컴파일러 및 플래그
CC = gcc
CFLAGS = -I. -I/usr/local/include -Wall -Wextra -g
LDFLAGS = -pthread -L/usr/local/lib -lcjson

# 디렉토리 설정
SRCDIR = .
OBJDIR = obj

# 소스 및 오브젝트 파일
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

# 실행 파일 이름
TARGET = server

# 기본 빌드 규칙
all: $(TARGET)

# 실행 파일 빌드 규칙
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# 오브젝트 파일 빌드 규칙
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 청소 규칙
clean:
	rm -rf $(OBJDIR) $(TARGET)

# 디버그 규칙
debug: CFLAGS += -DDEBUG
debug: clean all
