GTK_FLAGS = `pkg-config --cflags --libs gtk4`
GUPNP_FLAGS = `pkg-config --cflags --libs gupnp-1.6 gupnp-av-1.0`

all: av-browser

av-browser: av-browser.c
	gcc -o $@ $^ $(GTK_FLAGS) $(GUPNP_FLAGS)

clean:
	rm -f av-browser
