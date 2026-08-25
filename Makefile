CC = gcc
LDFLAGS = -I. -ldl -lpthread -lm
ifeq ($(build),release)
	CFLAGS = -O3
	LDFLAGS += -DNDEBUG=1
else
	CFLAGS = -Og -g
endif
CFLAGS += -std=gnu99 -Wall -Wextra -Werror -pedantic
RM = rm -rf

OBJECTS = pug.o mdb.o midl.o module.o mongoose.o parson.o
OBJECTS := $(addprefix objects/,$(OBJECTS))

all: objects $(OBJECTS)

objects:
	@echo "Create 'objects' folder ..."
	@mkdir -p objects

test_pug:
	@$(CC) $(CFLAGS) pug.c -o $@ $(LDFLAGS) -DTEST_PUG=1

test_app:
	@$(CC) $(CFLAGS) app.c parson.c mdb.c midl.c module.c -o $@ $(LDFLAGS) -DTEST_APP=1

objects/.o: .c
ifeq ($(build),release)
	@echo "Build release '$@' object ..."
else
	@echo "Build '$@' object ..."
endif
	@$(CC) -c $(CFLAGS) $< -o $@ $(LDFLAGS)
objects/%.o: %.c
ifeq ($(build),release)
	@echo "Build release '$@' object ..."
else
	@echo "Build '$@' object ..."
endif
	@$(CC) -c $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	@echo "Cleanup ..."
	@$(RM) $(OBJECTS) test_pug test_app
