# Practice Replayer - Mod de Geode para Geometry Dash

## ¿Qué hace?

Este mod te permite **grabar tus inputs durante la práctica** de un nivel y **reproducirlos automáticamente**. Ideal para guardar una run perfecta y dejar que el juego la replique solo.

---

## Funciones

| Función | Descripción |
|---|---|
| **Grabar** | Graba todos tus inputs (clicks/saltos) mientras juegas en práctica |
| **Guardar** | Guarda la grabación en disco para usarla después |
| **Reproducir** | Reproduce la última grabación guardada automáticamente |
| **Activar/Desactivar** | Activa o desactiva el mod sin salir del juego |
| **Ocultar botón** | Hace invisible el botón del mod en el menú de pausa |

---

## Cómo usarlo

1. **Entra a un nivel en modo práctica**
2. **Abre el menú de pausa** (botón de pausa)
3. Verás el panel **"Practice Replayer"** en la esquina superior derecha
4. Presiona **"Grabar"** para empezar a grabar tus inputs
5. Reanuda el juego y juega normalmente
6. Cuando quieras guardar, pausa y presiona **"Guardar"**
7. Para reproducir la grabación, presiona **"Reproducir"** en el menú de pausa
8. Usa **"Mod: ON/OFF"** para activar o desactivar el mod
9. Usa **"Ocultar botón"** para hacer invisible el botón del panel

---

## Cómo compilar

### Requisitos previos

1. Instala **CMake** (https://cmake.org)
2. Instala el **CLI de Geode**:
   ```bash
   # Windows (PowerShell como administrador)
   winget install GeodeSDK.GeodeCLI
   ```
3. Instala el SDK de Geode:
   ```bash
   geode sdk install
   ```
4. En Windows: instala **Visual Studio 2022** con el componente "Desarrollo de escritorio con C++"

### Compilar el mod

```bash
# Clona / abre la carpeta del mod
cd PracticeReplayer

# Configura CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compila
cmake --build build --config Release
```

El archivo `.geode` resultante aparecerá en la carpeta `build/`.

### Instalar el mod

Copia el archivo `.geode` a la carpeta de mods de Geometry Dash:

- **Windows:** `C:\Users\TU_USUARIO\AppData\Local\GeometryDash\geode\mods\`

O usa el CLI:
```bash
geode package install build/PracticeReplayer.geode
```

---

## Compatibilidad

- **Geometry Dash:** 2.2081
- **Geode SDK:** 5.5.0
- **Plataformas:** Windows, macOS, Android

---

## Estructura de archivos

```
PracticeReplayer/
├── src/
│   └── main.cpp        ← Código principal del mod
├── CMakeLists.txt      ← Configuración de compilación
├── mod.json            ← Metadatos del mod (nombre, ID, versión)
└── README.md           ← Este archivo
```
