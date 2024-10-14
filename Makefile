CFLAGS += -Os -Wall -Wextra
CFLAGS += -DG_LOG_DOMAIN=\"av-browser\"
LDFLAGS += -s

CFLAGS += `pkg-config --cflags gtk4`
LDLIBS += `pkg-config --libs gtk4`

CFLAGS += `pkg-config --cflags gupnp-1.6 gupnp-av-1.0`
LDLIBS += `pkg-config --libs gupnp-1.6 gupnp-av-1.0`

all: av-browser

av-browser: av-browser.o

.PHONY: clean
clean:
	rm -f av-browser
