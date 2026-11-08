// Headless EGL context (Linux CI / simulator). iOS and Android supply their own context via SDL.
#pragma once
bool eglHeadlessInit();
