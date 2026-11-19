# iGUI native draw callback (requirements from zenMCAP / lucent-atlas, 2026-11-19)

Context: both tools run on the zen_plataform window with its GL context (GL 3.3 core natively, WebGL2 on the web: batches are ES3-clean). Tests use the fake backend and offscreen EGL. Today both render to their own FBO and show it with Viewport3D::setTexture; that stays for the 3D scene.

Needs, in their order
1. 2D plot and map overlays: no depth, no stencil. Scissor set to the widget's clip; the pixel size of the framebuffer and of the rect given to the callback; a flag "iGUI resets its own state after the callback" so the application does not track what iGUI uses.
2. MSAA: only if the window framebuffer has it; they only need the sample count.
3. Depth inside the callback: NOT needed.
4. No worker-thread context sharing.
5. A query for "the callback will not run" (software path, fake backend) available BEFORE paint, so the widget picks the texture or PaintContext fallback up front and tests stay deterministic.
6. The callback may run every frame without marking anything dirty in iGUI; and it must not be invoked when iGUI decides the frame is unchanged unless the widget asked for a repaint.

Their priorities in iGUI after the theme round: this hook, and cheap idle frames. Rule on their side: heavy drawing goes through the tool's own batches, iGUI is for chrome.
