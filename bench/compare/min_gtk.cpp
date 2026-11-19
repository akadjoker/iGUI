// The smallest GTK 3 application: a window with a label. Only for the size of the binary.
#include <gtk/gtk.h>

int main(int argc, char** argv)
{
    gtk_init(&argc, &argv);
    GtkWidget* w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(w), 400, 300);
    gtk_container_add(GTK_CONTAINER(w), gtk_label_new("Hello"));
    g_signal_connect(w, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    gtk_widget_show_all(w);
    gtk_main();
    return 0;
}
