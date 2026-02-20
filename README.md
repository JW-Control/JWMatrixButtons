# JWMatrixButtons

Librería ligera para **leer una matriz de botones** (filas/columnas) con **debounce**, **eventos (press/release/repeat)** y helpers para manejar “ejes” (subir/bajar o izq/der) con **aceleración por mantenido**.

Pensada para proyectos tipo PLC/HMI donde quieres que el **sketch decida** qué hacer con los botones (sin acoplarse a GLCD, menús, etc.).

---

## Características

- Lectura de matriz **R×C** (hasta 8×8) con tiempos de “settle” configurables.
- **Debounce** por tecla con ventana configurable.
- Generación de eventos:
  - `EV_PRESS`
  - `EV_RELEASE`
  - `EV_REPEAT` (al mantener presionado, con aceleración)
- Repeat configurable:
  - habilitar/deshabilitar por botón
  - retardo inicial
  - perfil de aceleración (umbrales, step y delay)
- Helper `applyAxis()` para aplicar UP/DOWN o LEFT/RIGHT a una variable con:
  - **wrap** en los extremos (solo en `PRESS`)
  - **snap a step** en `REPEAT` para “saltos” más limpios

---

## Instalación (manual)

1. Crea una carpeta en tu Arduino libraries:
   - `Documents/Arduino/libraries/JWMatrixButtons/`
2. Copia dentro:
   - `JWMatrixButtons.h`
   - `JWMatrixButtons.cpp`
   - `library.properties`
3. Reinicia Arduino IDE.

> Si el IDE te dice “invalid library: no header files found”, casi siempre es porque **el `.h` no está en la raíz** de la carpeta de la librería o el nombre de carpeta no coincide.

---

## Cableado típico (idea general)

- **Filas**: pines configurados como `OUTPUT`.
- **Columnas**: pines configurados como `INPUT`.
- Cada tecla conecta una fila con una columna.
- Si tus columnas tienen **pulldown externo**, usa `invertLogic=false`.
- Si tus columnas usan **pullup** (interno o externo) y la tecla baja a GND, probablemente querrás `invertLogic=true`.

> En ESP32: IO34/35/36/39 son *input-only* y no tienen `INPUT_PULLUP`, así que normalmente se usan con resistencias externas.

---

## Concepto de uso

La librería no “hace menús”, solo te da eventos y estados. Tú eliges si un botón:
- cambia de página
- incrementa un valor
- invierte un eje
- entra/sale de modo edición
- etc.

---

## API rápida

### Tipos
```cpp
JWMatrixButtons::EvType      // EV_PRESS, EV_RELEASE, EV_REPEAT
JWMatrixButtons::BtnEvent    // { id, type, mult, held_ms }
JWMatrixButtons::BtnMapItem  // { id, row, col }
```

### Inicialización
```cpp
bool begin(const uint8_t* rowPins, uint8_t nRows,
           const uint8_t* colPins, uint8_t nCols,
           const BtnMapItem* map, uint8_t mapLen,
           uint8_t buttonCount,
           bool invertLogic=false,
           uint32_t debounceMs=35);
```

### En loop
```cpp
void update(); // ideal cada 3–10 ms
```

### Estado/eventos del último `update()`
```cpp
bool pressed(uint8_t id);
bool released(uint8_t id);
bool isDown(uint8_t id);

uint8_t eventCount() const;
bool getEvent(uint8_t idx, BtnEvent &out) const;
```

### Repeat
```cpp
void setRepeatEnabled(uint8_t id, bool enabled);
void setRepeatInitialDelay(uint32_t ms);

void setRepeatProfile(uint16_t thr1, uint16_t thr2, uint16_t thr3,
                      int16_t s1, int16_t s2, int16_t s3, int16_t s4,
                      uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);
```

**Defaults (perfil de fábrica):**
- initialDelay = **300 ms**
- thresholds: **7 / 15 / 25**
- steps: **1 / 10 / 100 / 1000**
- delays: **180 / 170 / 150 / 120 ms**

### Helper de eje
```cpp
bool applyAxis(uint32_t* val, uint32_t minv, uint32_t maxv,
               uint8_t decId, uint8_t incId,
               bool circularWrapOnPress=true,
               bool snapToStepOnRepeat=true) const;
```

- Devuelve `true` si **modificó** `*val` en este ciclo.
- `circularWrapOnPress`:
  - si estás en `maxv` y haces `INC` (PRESS) → salta a `minv`
  - si estás en `minv` y haces `DEC` (PRESS) → salta a `maxv`
- `snapToStepOnRepeat`:
  - cuando el evento es `EV_REPEAT`, alinea el cálculo al múltiplo del step para que los saltos sean “redondos”.

---

## Ejemplo 1: lectura básica + eventos

```cpp
#include <Arduino.h>
#include <JWMatrixButtons.h>

static const uint8_t ROWS[] = {25, 26};
static const uint8_t COLS[] = {35, 34, 39, 36};

enum BtnId : uint8_t { BTN_A, BTN_B, BTN__COUNT };

static const JWMatrixButtons::BtnMapItem MAP[] = {
  {BTN_A, 0, 0},
  {BTN_B, 1, 1},
};

JWMatrixButtons btn;

void setup() {
  Serial.begin(115200);

  btn.begin(ROWS, 2, COLS, 4,
            MAP, sizeof(MAP)/sizeof(MAP[0]),
            BTN__COUNT,
            false, 35);

  btn.setRepeatEnabled(BTN_A, true);
}

void loop() {
  btn.update();

  if (btn.pressed(BTN_A))  Serial.println("A press");
  if (btn.released(BTN_A)) Serial.println("A release");

  // Lectura “fine-grain” de eventos:
  JWMatrixButtons::BtnEvent ev;
  for (uint8_t i=0; i<btn.eventCount(); i++) {
    if (btn.getEvent(i, ev)) {
      // ev.id, ev.type, ev.mult, ev.held_ms
    }
  }

  delay(5);
}
```

---

## Ejemplo 1.1: ESP32 (2 núcleos) — `update()` en un task

En ESP32 puedes ejecutar el escaneo y la generación de eventos en un **task de FreeRTOS** (pinned al núcleo que prefieras). Con esto:

- No necesitas llamar `btn.update()` en tu `loop()`.
- Si tu firmware tiene otras tareas pesadas, la botonera puede seguir leyendo de forma constante.

```cpp
void setup() {
  Serial.begin(115200);

  btn.begin(ROWS, 2, COLS, 4,
            MAP, sizeof(MAP)/sizeof(MAP[0]),
            BTN__COUNT,
            false, 35);

  // core: 0 o 1
  // stackBytes: por defecto 4096
  // priority: 1 suele bastar
  // periodMs: cada cuánto corre update() (5 ms recomendado)
  btn.startTask(0, 4096, 1, 5);
}

void loop() {
  // ya NO necesitas btn.update();

  if (btn.pressed(BTN_A)) Serial.println("A press");
  if (btn.released(BTN_A)) Serial.println("A release");

  delay(5);
}
```

**Nota:** en esta versión, `pressed()` / `released()` son **latcheados** (quedan pendientes hasta que los leas), así que es ideal para modo task.

---

## Ejemplo 2: páginas (izq/der) y edición (up/down) con wrap

Este es el patrón típico que estabas usando: en “modo páginas” navegas; al entrar a edición, UP/DOWN modifican un valor con aceleración.

```cpp
// Páginas con wrap en PRESS, y “snap” en REPEAT:
btn.applyAxis(&pageIdx, 0, NUM_PAGES-1, BTN_LEFT, BTN_RIGHT, true, true);

// Valor editable con wrap en PRESS:
btn.applyAxis(&editVal, 0, 9999, BTN_DOWN, BTN_UP, true, true);
```

---

## Punteros vs referencias (por qué `&`)

En C++ tienes dos formas comunes de permitir que una función **modifique una variable**:

### 1) Pasar por referencia
```cpp
bool applyAxis(uint32_t& val, ...);
applyAxis(editVal, ...);   // NO usas &
```

- La función **siempre** recibe una variable válida.
- No puedes pasar “nada”. (No existe referencia nula.)
- Muy cómodo cuando estás 100% seguro que siempre habrá variable.

### 2) Pasar por puntero
```cpp
bool applyAxis(uint32_t* val, ...);
applyAxis(&editVal, ...);  // SÍ usas &
```

- Puedes pasar `nullptr` para “desactivar” el target:
  ```cpp
  btn.applyAxis(nullptr, ...); // no hace nada
  ```
- Permite APIs más genéricas (por ejemplo, seleccionar a qué variable apuntar en runtime).
- Requiere validar `if (!val) return false;` (la librería lo hace).

**En tu caso**, como siempre existe la variable (nunca será `nullptr`), *sí*, una versión por **referencia** sería totalmente válida y un poquito más cómoda de llamar.

> Nota: el operador `&` en `&editVal` NO “convierte en puntero” permanentemente tu variable; solo toma su **dirección** para esta llamada.

---

## Ajustes recomendados

- Llama `update()` frecuente (3–10 ms).  
  Si metes delays grandes o tareas pesadas, el debounce y repeat se vuelven “toscos”.
- Si la matriz te da lecturas raras:
  - sube `debounceMs` (ej. 50–70 ms)
  - aumenta `setScanDelays(settleUs, betweenRowsUs)` (ej. 200–300 us)

---

## Límites internos

- `MAX_ROWS = 8`, `MAX_COLS = 8`
- `MAX_BTNS = 32`
- `MAX_EVENTS = 40` por ciclo de `update()`

---

## Licencia

Define aquí tu licencia (MIT/BSD/Apache-2.0/etc.) y añádela como `LICENSE` en el repo.
