#include <rts/application.h>

#include <devkit/gfx/frame_buffer.h>
#include <devkit/io/window.h>

namespace rts {

int run(const char* title)
{
    dk::io::Window window;
    window.config(dk::io::Window::Title(title));
    window.open(1);

    while (window.isOpen()) {
        window.beginFrame();
        dk::gfx::backBuffer().clear(dk::gfx::Clear::Color, dk::colors::black);
        if (dk::io::key::esc)
            window.close();
        window.endFrame();
    }

    return 0;
}

} // namespace rts
