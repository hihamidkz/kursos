CC := gcc
OBJ := ./obj

.PHONY: all
all: server client

temp: temp.o obj/myreadkey.o obj/myterm.o
	$(CC) -o $@ $^

main: obj/main.o obj/myterm.o obj/mybigchars.o obj/interface.o obj/myreadkey.o
	$(CC) -o $@ $^

server: obj/server.o obj/myterm.o obj/mybigchars.o obj/interface.o obj/myreadkey.o
	$(CC) -o $@ $^ -lm
	
client: obj/client.o obj/myterm.o obj/mybigchars.o obj/interface.o obj/myreadkey.o
	$(CC) -o $@ $^

$(OBJ)/%.o: %.c
	$(CC) -g -c $^ -o $@
	
.PHONY: clean
clean:
	rm -f $(OBJ)/*.o
