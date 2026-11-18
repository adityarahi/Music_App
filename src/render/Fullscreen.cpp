#include "render/Fullscreen.h"

#include <glad/gl.h>

void drawFullscreen() {
    // Core profile requires some VAO to be bound even with no attributes.
    // Created once and kept for the life of the context.
    static GLuint vao = 0;
    if (!vao) glGenVertexArrays(1, &vao);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
