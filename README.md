# Mystery shack opengl

Aplicación en C++ y OpenGL en Visual Studio para cargar un modelo 3D en formato `.glb`, navegar por la escena, aplicar iluminación y trazar/recorrer una curva de Bézier cúbica.

## Generalidades
*Lenguaje y API: C++, OpenGL 3.3 Core.
Librerías principales: GLFW (ventana, contexto y entrada), GLAD (cargador de funciones OpenGL), GLM (matemática vectorial y matricial) y `tiny_gltf` (lectura de archivos `.glb`).

 **Controles:**
  * `W`, `A`, `S`, `D`: desplazamiento horizontal de la cámara.
  * `Q` / `E`: bajar o subir verticalmente.
  * Mouse: rotación de vista (`yaw` y `pitch`).
  * Flechas Arriba / Abajo: órbita vertical alrededor del punto objetivo (`orbitTarget`).
  * `ESC`: cerrar ventana.

## Arquitectura del código
1. Gestión de ventana y entrada: `framebuffer_size_callback`, `mouse_callback`, `orbitarVertical` y `processInput` controlan el tamaño del *viewport*, la orientación de la cámara y el movimiento independiente de los FPS mediante `deltaTime`.
2. Carga de modelo y buffers (`cargarModeloGLBConMateriales`): lee el archivo `.glb` con `tiny_gltf`, extrae posiciones, coordenadas UV e índices por primitiva, y configura `VAO`, `VBO` y `EBO` en cada `SubMalla` (además de intentar vincular texturas incrustadas si existen).
3. Shaders e iluminación:** un programa de shaders transforma vértices y normales al espacio de mundo y calcula iluminación (ambiental, difusa y especular) junto con el color base/UV; un segundo programa renderiza líneas simples (`GL_LINE_STRIP`).
4. Curva de Bézier (`cubicBezier`): evalúa el polinomio cúbico con cuatro puntos de control ($P_0, P_1, P_2, P_3$) para generar los puntos de la trayectoria en escena y animar una posición dinámica (`t`) en el bucle principal.
5. Bucle de renderizado (`main`): limpia los buffers de color y profundidad, actualiza las matrices `projection`, `view` y `model`, dibuja las submallas y la curva de Bézier, e intercambia buffers.
