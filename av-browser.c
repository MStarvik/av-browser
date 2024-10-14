#include <libgupnp/gupnp.h>
#include <libgupnp-av/gupnp-av.h>
#include <gtk/gtk.h>

#define ITEM_TYPE (item_get_type())
G_DECLARE_FINAL_TYPE(Item, item, ITEM, OBJECT, GObject)

struct _Item {
    GObject parent_instance;
    GUPnPServiceProxy *content_directory;
    char* parent_id;
    char* object_id;
    char* title;
    char* upnp_class;
    char* resource_type;
    char* resource_url;
};

G_DEFINE_TYPE(Item, item, G_TYPE_OBJECT)

static void item_dispose(GObject *object) {
    Item *self = (Item*)object;
    if (self->content_directory) {
        g_object_unref(self->content_directory);
    }
    g_free(self->parent_id);
    g_free(self->object_id);
    g_free(self->title);
    g_free(self->upnp_class);
    g_free(self->resource_type);
    g_free(self->resource_url);
    G_OBJECT_CLASS(item_parent_class)->dispose(object);
}

static void item_class_init(ItemClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = item_dispose;
}

static void item_init(Item *self) {}

Item *item_new(GUPnPServiceProxy *content_directory,const char *object_id) {
    Item *self = g_object_new(ITEM_TYPE, NULL);
    if (content_directory) {
        g_object_ref(content_directory);
    }
    self->content_directory = content_directory;
    self->parent_id = NULL;
    self->object_id = g_strdup(object_id);
    self->title = NULL;
    self->upnp_class = NULL;
    self->resource_type = NULL;
    self->resource_url = NULL;
    return self;
}

void item_set_metadata(Item *self, const char *parent_id, const char *title, const char *upnp_class) {
    g_free(self->parent_id);
    g_free(self->title);
    g_free(self->upnp_class);
    self->parent_id = g_strdup(parent_id);
    self->title = g_strdup(title);
    self->upnp_class = g_strdup(upnp_class);
}

void item_set_resource(Item *self, const char *resource_type, const char *resource_url) {
    g_free(self->resource_type);
    g_free(self->resource_url);
    self->resource_type = g_strdup(resource_type);
    self->resource_url = g_strdup(resource_url);
}

typedef struct {
    GUPnPControlPoint *cp;
    GUPnPDIDLLiteParser *parser;
    GtkWidget *window;
    GtkWidget *up_button;
    GListStore *items;
    Item *current_item;
} Browser;

static char *upnp_class_to_icon(const char *upnp_class) {
    if (g_str_has_prefix(upnp_class, "object.container")) {
        return "folder";
    } else if (g_str_has_prefix(upnp_class, "object.item.audioItem")) {
        return "audio-x-generic";
    } else if (g_str_has_prefix(upnp_class, "object.item.imageItem")) {
        return "image-x-generic";
    } else if (g_str_has_prefix(upnp_class, "object.item.videoItem")) {
        return "video-x-generic";
    } else {
        return "unknown";
    }
}

static void call_action_and_parse_response(GUPnPServiceProxy *proxy, GUPnPServiceProxyAction *action, GUPnPDIDLLiteParser *parser, GError **error) {
    gupnp_service_proxy_call_action(proxy, action, NULL, error);
    if (*error) {
        return;
    }

    char *result;
    gupnp_service_proxy_action_get_result(
        action,
        error,
        "Result", G_TYPE_STRING, &result,
        NULL
    );
    if (*error) {
        return;
    }

    gupnp_didl_lite_parser_parse_didl(parser, result, error);
}

static void browse_metadata(GUPnPServiceProxy *content_directory, GUPnPDIDLLiteParser *parser, const char *object_id) {
    GUPnPServiceProxyAction *action = gupnp_service_proxy_action_new(
        "Browse",
        "ObjectID", G_TYPE_STRING, object_id,
        "BrowseFlag", G_TYPE_STRING, "BrowseMetadata",
        "Filter", G_TYPE_STRING, "*",
        "StartingIndex", G_TYPE_UINT, 0,
        "RequestedCount", G_TYPE_UINT, 0,
        "SortCriteria", G_TYPE_STRING, "",
        NULL
    );

    GError *error = NULL;
    call_action_and_parse_response(content_directory, action, parser, &error);
    if (error) {
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
    }

    gupnp_service_proxy_action_unref(action);
}

static void browse_children(GUPnPServiceProxy *content_directory, GUPnPDIDLLiteParser *parser, const char *object_id) {
    GUPnPServiceProxyAction *action = gupnp_service_proxy_action_new(
        "Browse",
        "ObjectID", G_TYPE_STRING, object_id,
        "BrowseFlag", G_TYPE_STRING, "BrowseDirectChildren",
        "Filter", G_TYPE_STRING, "*",
        "StartingIndex", G_TYPE_UINT, 0,
        "RequestedCount", G_TYPE_UINT, 0,
        "SortCriteria", G_TYPE_STRING, "",
        NULL
    );

    GError *error = NULL;
    call_action_and_parse_response(content_directory, action, parser, &error);
    if (error) {
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
    }

    gupnp_service_proxy_action_unref(action);
}

static void browse(Browser *browser, GUPnPServiceProxy *content_directory, const char *object_id) {
    g_object_unref(browser->current_item);
    browser->current_item = item_new(content_directory, object_id);
    g_list_store_remove_all(browser->items);

    browse_metadata(content_directory, browser->parser, object_id);
    browse_children(content_directory, browser->parser, object_id);

    // TODO: browse_metadata and browse_children may fail, but the state has
    //       already been dirtied by the time this happens. We should either
    //       save the previous state and restore it, or just return to the
    //       media servers list on error.
}

static void object_available_cb(GUPnPDIDLLiteParser *parser, GUPnPDIDLLiteObject *object, Browser *browser_data) {
    const char *parent_id = gupnp_didl_lite_object_get_parent_id(object);
    const char *object_id = gupnp_didl_lite_object_get_id(object);

    if (g_strcmp0(object_id, browser_data->current_item->object_id) == 0) {
        // The object is the current item, update its metadata

        item_set_metadata(
            browser_data->current_item,
            parent_id,
            gupnp_didl_lite_object_get_title(object),
            gupnp_didl_lite_object_get_upnp_class(object)
        );

        gtk_window_set_title(GTK_WINDOW(browser_data->window), browser_data->current_item->title);
        gtk_widget_set_sensitive(browser_data->up_button, TRUE);
    } else if (g_strcmp0(parent_id, browser_data->current_item->object_id) == 0) {
        // The object is a child of the current item, add it to the list

        Item *child = item_new(browser_data->current_item->content_directory, object_id);
        item_set_metadata(
            child,
            parent_id,
            gupnp_didl_lite_object_get_title(object),
            gupnp_didl_lite_object_get_upnp_class(object)
        );

        if (g_str_has_prefix(child->upnp_class, "object.item")) {
            GList *resources = gupnp_didl_lite_object_get_resources(object);
            if (resources) {
                GUPnPDIDLLiteResource *resource = resources->data;
                GUPnPProtocolInfo *protocol_info = gupnp_didl_lite_resource_get_protocol_info(resource);
                item_set_resource(
                    child,
                    gupnp_protocol_info_get_mime_type(protocol_info),
                    gupnp_didl_lite_resource_get_uri(resource)
                );
            }
            g_list_free_full(resources, g_object_unref);
        }

        g_list_store_append(browser_data->items, child);
        g_object_unref(child);
    }
}

static void device_proxy_available_cb(GUPnPControlPoint *cp, GUPnPDeviceProxy *proxy, Browser *browser_data) {
    if (browser_data->current_item->content_directory == NULL) {
        // Not currently browsing a media server, show a list of media servers

        GUPnPServiceInfo *content_directory = gupnp_device_info_get_service(GUPNP_DEVICE_INFO(proxy), "urn:schemas-upnp-org:service:ContentDirectory:1");
        if (content_directory) {
            Item *item = item_new(GUPNP_SERVICE_PROXY(content_directory), "0");
            item_set_metadata(
                item,
                "-1",
                gupnp_device_info_get_friendly_name(GUPNP_DEVICE_INFO(proxy)),
                "object.container"
            );
            g_list_store_append(browser_data->items, item);
            g_object_unref(content_directory);
        }
    }
}

static void setup_item_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, Browser *browser_data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_list_item_set_child(list_item, box);

    GtkWidget *icon = gtk_image_new_from_icon_name("folder");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 96);
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    // gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_box_append(GTK_BOX(box), label);
}

static void bind_item_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, Browser *browser_data) {
    GtkWidget *box = gtk_list_item_get_child(list_item);
    Item *item = gtk_list_item_get_item(list_item);
    
    GtkWidget *icon = gtk_widget_get_first_child(box);
    if (browser_data->current_item->content_directory) {
        gtk_image_set_from_icon_name(GTK_IMAGE(icon), upnp_class_to_icon(item->upnp_class));
    } else {
        gtk_image_set_from_icon_name(GTK_IMAGE(icon), "folder-remote");
    }

    GtkWidget *label = gtk_widget_get_last_child(box);
    gtk_label_set_text(GTK_LABEL(label), item->title);
}

static void activate_item_cb(GtkGridView *grid_view, guint position, Browser *browser_data) {
    Item *item = g_list_model_get_item(G_LIST_MODEL(browser_data->items), position);

    if (g_str_has_prefix(item->upnp_class, "object.container")) {
        browse(browser_data, item->content_directory, item->object_id);
    } else if (g_str_has_prefix(item->upnp_class, "object.item")) {
        GAppInfo *app_info = g_app_info_get_default_for_type(item->resource_type, TRUE);
        if (app_info) {
            g_info("Open \"%s\" with \"%s\"", item->resource_url, g_app_info_get_id(app_info));
            GList *uris = g_list_append(NULL, item->resource_url);
            g_app_info_launch_uris(app_info, uris, NULL, NULL);
            g_list_free(uris);
        } else {
            g_error("No default applications for \"%s\"", item->resource_type);
        }
    }
}

static void up_button_clicked_cb(GtkButton *button, Browser *browser_data) {
    if (browser_data->current_item->parent_id) {
        if (g_strcmp0(browser_data->current_item->parent_id, "-1") == 0) {
            // The current item is the root, go back to media servers list

            g_object_unref(browser_data->current_item);
            browser_data->current_item = item_new(NULL, "-1");
            g_list_store_remove_all(browser_data->items);

            gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(browser_data->cp), FALSE);
            gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(browser_data->cp), TRUE);

            gtk_window_set_title(GTK_WINDOW(browser_data->window), "Media Servers");
            gtk_widget_set_sensitive(browser_data->up_button, FALSE);
        } else {
            // browse replaces the current item, so we need to keep a reference to it
            Item *old_current = g_object_ref(browser_data->current_item);

            browse(browser_data, old_current->content_directory, old_current->parent_id);

            g_object_unref(old_current);
        }
    }
}

static void activate_cb(GtkApplication *app, Browser *browser_data) {
    GError *error = NULL;

    browser_data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(browser_data->window), "Media Servers");
    gtk_window_set_default_size(GTK_WINDOW(browser_data->window), 800, 600);

    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(browser_data->window), header_bar);

    browser_data->up_button = gtk_button_new_from_icon_name("go-up-symbolic");
    gtk_widget_set_sensitive(browser_data->up_button, FALSE);
    g_signal_connect(browser_data->up_button, "clicked", G_CALLBACK(up_button_clicked_cb), browser_data);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), browser_data->up_button);

    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_window_set_child(GTK_WINDOW(browser_data->window), scrolled_window);

    browser_data->items = g_list_store_new(ITEM_TYPE);
    GtkSingleSelection *model = gtk_single_selection_new(G_LIST_MODEL(browser_data->items));

    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_item_cb), browser_data);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_item_cb), browser_data);

    GtkWidget *grid_view = gtk_grid_view_new(GTK_SELECTION_MODEL(model), factory);
    g_signal_connect(grid_view, "activate", G_CALLBACK(activate_item_cb), browser_data);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), grid_view);

    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
        css_provider,
        "gridview {"
        "  padding: 10px;"
        "}"
        "gridview child {"
        "  margin: 5px 10px;"
        "  border-radius: 10px;"
        "}"
    );

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    g_object_unref(css_provider);

    GUPnPContext *context = gupnp_context_new_for_address(NULL, 0, 0, &error);
    if (error) {
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
        return;
    }

    browser_data->parser = gupnp_didl_lite_parser_new();
    g_signal_connect(browser_data->parser, "object-available", G_CALLBACK(object_available_cb), browser_data);

    browser_data->cp = gupnp_control_point_new(context, "urn:schemas-upnp-org:device:MediaServer:1");
    g_signal_connect(browser_data->cp, "device-proxy-available", G_CALLBACK(device_proxy_available_cb), browser_data);
    gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(browser_data->cp), TRUE);

    gtk_window_present(GTK_WINDOW(browser_data->window));
}

int main(int argc, char **argv) {
    Browser browser_data = {
        .current_item = item_new(NULL, "-1")
    };

    GtkApplication *app = gtk_application_new("no.mstarvik.av-browser", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate_cb), &browser_data);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}