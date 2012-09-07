BIN := prop_test

SOURCES := \
	property.c \
	load_file.c

CFLAGS := -I.

SOURCES += tests/test.c

OBJECTS = $(SOURCES:.c=.o)

all: $(SOURCES) $(BIN)

$(BIN): $(OBJECTS)
	@echo "Install   : $(BIN)"
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	@echo "Compile   : $(BIN) <= $(notdir $<)"
	@$(CC) -c $(CFLAGS) $< -o $@
