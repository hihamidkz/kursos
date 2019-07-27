CC := gcc
OBJ := ./obj
ifndef WALL
WALL := -Wall
endif

.PHONY: all
all: server client reserve_server

temp: temp.o obj/myreadkey.o obj/myterm.o
	$(CC) -o $@ $^

main: obj/main.o obj/myterm.o obj/mybigchars.o obj/interface.o obj/myreadkey.o
	$(CC) -o $@ $^

server: obj/server_main.o obj/server.o obj/myterm.o obj/mybigchars.o obj/interface.o obj/myreadkey.o
	$(CC) -o $@ $^ -lm -lpthread

client: obj/client.o obj/myterm.o obj/mybigchars.o obj/interface.o obj/myreadkey.o
	$(CC) -o $@ $^

reserve_server: obj/reserve_server.o obj/myterm.o obj/mybigchars.o obj/interface.o obj/myreadkey.o obj/reserve_server_main.o obj/server.o
	$(CC) -o $@ $^ -lpthread

$(OBJ)/%.o: %.c
	$(CC) $(WALL) -g -c $^ -o $@

$(OBJ)/server_main.o: server_main.c
	$(CC) $(WALL) -pthread -g -c $^ -o $@

$(OBJ)/server.o: server.c
	$(CC) $(WALL) -pthread -g -c $^ -o $@

.PHONY: clean
clean:
	rm -f $(OBJ)/*.o
