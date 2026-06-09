# How it works

The renderer follows a standard OpenGL pipeline:

1. **Scene setup** — geometry loaded and uploaded to GPU as VBOs
2. **Shader compilation** — vertex and fragment shaders compiled at runtime
3. **Render loop** — each frame clears buffer, binds shaders, draws geometry, swaps buffers
4. **Input handling** — GLFW captures keyboard/mouse for camera movement

## Project Structure

```
CTF---backroom/
├── src/
│   ├── main.cpp
│   ├── renderer.cpp
│   └── shader.cpp
├── shaders/
│   ├── vertex.glsl
│   └── fragment.glsl
└── CMakeLists.txt
```
