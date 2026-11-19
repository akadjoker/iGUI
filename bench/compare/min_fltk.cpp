// The smallest FLTK application: a window with a label. Only for the size of the binary.
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Window.H>

int main(int argc, char** argv)
{
    Fl_Window w(400, 300, "min");
    Fl_Box b(10, 10, 100, 24, "Hello");
    w.show(argc, argv);
    return Fl::run();
}
