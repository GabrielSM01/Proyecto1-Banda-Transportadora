# Marco teórico

## Periféricos de la IdeaBoard

### Hardware PWM: por qué configurar LEDC_TIMER_0 antes de los canales

El periférico LEDC del ESP32 separa dos bloques: los temporizadores, que generan la base de tiempo, y los canales, que comparan esa cuenta contra un valor de ciclo de trabajo (duty) y manejan un pin de salida. Un canal no tiene reloj propio: depende del temporizador que se le asigna (`timer_sel`). Por eso el temporizador se configura primero. Sin él, el canal no tiene periodo ni escala contra la cual comparar su duty.

El temporizador fija dos cosas acopladas entre sí. La frecuencia depende de la fuente de reloj, del divisor y de la resolución R:

$$f_{PWM} = \frac{f_{clk}}{N_{div} \cdot 2^{R}}$$

Con el reloj APB de 80 MHz, 1 kHz y R = 10 bits, el divisor resulta:

$$N_{div} = \frac{80 \times 10^{6}}{1000 \cdot 2^{10}} = 78{,}125$$

La resolución máxima posible a una frecuencia dada es $R_{max} = \lfloor \log_2 (f_{clk} / f_{PWM}) \rfloor$, que a 1 kHz da 16 bits, así que 10 bits entran sin problema. Con 10 bits el duty tiene 1024 pasos, o sea una resolución de aproximadamente 0,1 % para controlar la velocidad.

Los canales de IO12 e IO14 comparten `LEDC_TIMER_0`, por lo que tienen exactamente el mismo periodo y resolución. Esto mantiene sincronizadas las dos entradas del L9110S, algo necesario para el freno, donde ambas se llevan al 100 %. La frecuencia de 1 kHz es suficientemente alta para que la inductancia del motor suavice la corriente y suficientemente baja para no penalizar al driver con pérdidas de conmutación.

### Resistencias pull-up: por qué IO34 no funcionaría con el interruptor

Los pines IO34 a IO39 del ESP32 son solo de entrada y, a nivel de silicio, no tienen las resistencias internas de pull-up ni de pull-down que sí tienen los demás GPIO, como IO33. En esos pines, `pinMode(pin, INPUT_PULLUP)` no tiene ningún efecto.

El interruptor está conectado entre el pin y GND. Al cerrarse lleva el pin a 0 V y la lectura es correcta. Pero al abrirse, el pin queda flotante (alta impedancia), sin ningún camino hacia 3,3 V. Un pin flotante captura ruido y acoplamientos, y su nivel lógico queda indefinido. El resultado son flancos de bajada falsos que disparan la interrupción sin que nadie presione nada, y no hay un nivel alto confiable de reposo.

Por eso, con IO34 haría falta una resistencia externa de pull-up (típicamente 10 kΩ a 3,3 V). Así el reposo queda definido en alto, y al cerrar el interruptor la corriente es de solo unos 0,33 mA. En IO33, en cambio, el pull-up interno (del orden de decenas de kΩ) cumple esa misma función sin componentes adicionales.

## Arquitectura de memoria y restricciones del silicio

### Caché de flash e IRAM_ATTR

El ESP32 no ejecuta el programa directamente desde la flash SPI externa. Las instrucciones y los datos de solo lectura pasan por un controlador de caché que traduce direcciones virtuales a páginas de la flash. Cuando el sistema escribe o borra la flash (por ejemplo al guardar datos en NVS o aplicar una actualización OTA), el bus SPI queda ocupado por esa operación y la flash no puede atender lecturas. Por eso el controlador de flash deshabilita la caché mientras dura la operación.

Si en ese intervalo llega una interrupción y su rutina reside en flash, la CPU intenta buscar una instrucción que la caché no puede servir. El resultado es una excepción irrecuperable ("Cache disabled but cached memory region accessed"), un Guru Meditation Error y el reinicio del sistema. El fallo es intermitente, porque solo ocurre si la interrupción coincide con la ventana de escritura, lo que lo hace difícil de reproducir.

`IRAM_ATTR` coloca la función en la IRAM, una RAM interna del chip que no depende de la caché ni de la flash, así que la ISR puede ejecutarse en cualquier momento. La misma regla vale para todo lo que la ISR llama: debe estar en IRAM o ser inline, y los datos que toca deben estar en RAM. Por eso la ISR de este proyecto no llama a funciones de la biblioteca `Serial`, que residen en flash. Además, una función en IRAM no sufre fallos de caché, que agregarían una latencia variable al atender la interrupción.

### Lecturas fragmentadas (torn reads)

El ESP32 tiene núcleos Xtensa de 32 bits: cada instrucción de carga o almacenamiento mueve como máximo una palabra de 32 bits. Un `uint64_t` ocupa dos palabras, así que leerlo requiere dos cargas separadas y escribirlo, dos almacenamientos. Ninguna de las dos operaciones es atómica.

Si la ISR actualiza el contador justo entre las dos cargas del bucle principal, este combina una mitad vieja con una mitad nueva. Ejemplo: el contador vale `0x00000000FFFFFFFF` y la ISR lo incrementa a `0x0000000100000000`. Si el bucle leyó la palabra baja antes del incremento (`0xFFFFFFFF`) y la alta después (`0x00000001`), obtiene `0x00000001FFFFFFFF`, un valor que el contador nunca tuvo, con un error de unas $2^{32}$ cuentas.

La solución es proteger la lectura con una sección crítica. En este proyecto el contador es un `uint32_t`, que se lee con una sola carga, y aun así se lee dentro del spinlock para obtener la bandera y el contador coherentes entre sí.

### La palabra clave volatile

Con optimización `-O2` el compilador asigna variables a registros de la CPU para evitar accesos a memoria. Analiza el flujo de ejecución visible en el código y, como ninguna instrucción del bucle modifica una bandera global, asume que no cambia. La ISR no forma parte de ese flujo, así que el compilador no sabe que puede escribir esa variable.

Por ejemplo, en `while (!flag) { }` sin `volatile` el compilador carga `flag` una sola vez en un registro (por ejemplo `a2`) antes del bucle y lo evalúa siempre contra ese valor. El resultado equivale a `while (1)`: aunque la ISR escriba un 1 en la RAM, el bucle sigue comparando el registro obsoleto y nunca sale.

`volatile` obliga a leer la variable de memoria en cada acceso y a escribirla en cada asignación, y evita que el compilador reordene los accesos entre variables volatile. Pero no garantiza atomicidad: `contador++` sigue siendo leer, sumar y escribir, tres pasos que una interrupción puede partir. Por eso el proyecto combina `volatile` con el spinlock: el primero resuelve la visibilidad y el segundo, la exclusión mutua.

## Concurrencia: spinlocks y mutexes

### Spinlock frente a mutex

Un mutex de FreeRTOS (`SemaphoreHandle_t`) es un objeto del kernel con una cola de espera. Si está ocupado, la tarea que lo pide pasa al estado Bloqueado y el planificador ejecuta otra hasta que se libere. Soporta herencia de prioridad y solo puede usarse desde contexto de tarea, porque una ISR no es una tarea: no tiene identidad para ser dueña del mutex ni puede ponerse a dormir.

Un spinlock (`portMUX_TYPE`) es un cerrojo basado en una instrucción atómica del hardware (en Xtensa, comparar e intercambiar). Quien lo pide queda en espera activa, girando hasta que el cerrojo se libera, sin pasar por el planificador. Como el ESP32 tiene dos núcleos, el spinlock garantiza la exclusión también entre ambos. Además, `portENTER_CRITICAL` enmascara las interrupciones del núcleo local mientras se sostiene el cerrojo, por lo que la sección crítica no puede ser interrumpida en ese núcleo.

Entre `loop()` y una ISR es obligatorio un spinlock por este caso: si `loop()` tuviera un mutex tomado y llegara la interrupción, la ISR intentaría tomarlo y tendría que esperar. Pero la ISR interrumpió a `loop()`, que no puede continuar ni liberar el mutex hasta que la ISR termine. Ninguna de las dos avanza: es un bloqueo mutuo. Con un spinlock, `loop()` enmascara las interrupciones locales mientras lo sostiene y la ISR no puede interrumpirla a mitad de la sección. Si la ISR corre en el otro núcleo, solo espera unos ciclos. Por eso las secciones críticas deben ser muy cortas.

## Física del rebote y depuración

### Física del contacto

En un microinterruptor, al accionarlo una lámina metálica móvil choca contra el contacto fijo. A escala microscópica las superficies no son lisas: están formadas por irregularidades (asperezas), y el contacto eléctrico real ocurre solo en unos pocos puntos. Con el impacto, la lámina rebota elásticamente contra el contacto y vibra como un sistema masa-resorte amortiguado. En cada rebote las asperezas se separan y se vuelven a tocar, por lo que la resistencia de contacto pasa repetidamente de casi cero a circuito abierto. También pueden aparecer pequeños arcos eléctricos en esas separaciones. La vibración se extingue típicamente en 1 a 10 ms, y más en contactos desgastados.

El pin del microcontrolador sigue ese comportamiento y muestra varias transiciones entre 0 V y 3,3 V hasta que la lámina se asienta. Un microcontrolador a 160 MHz tiene un periodo de reloj de 6,25 ns, y cada tramo de la vibración dura microsegundos o más, es decir, miles de veces un ciclo. Por eso el hardware detecta cada transición como un flanco independiente, y una sola pulsación humana aparece como una ráfaga de 20 a 50 cambios de nivel lógico. Medición en el laboratorio con el microinterruptor del proyecto: _[completar con el número de flancos observado]_.

### Depuración por ciclos con CCOUNT

Una ISR debe ser corta y determinista, porque mientras se ejecuta puede retrasar otras interrupciones y al resto del sistema. `delay()` es inaceptable en ella: espera de forma bloqueante y, en el core Arduino del ESP32, se apoya en el planificador de FreeRTOS, que no está disponible en contexto de interrupción. `millis()` no bloquea y en el ESP32 puede invocarse desde una ISR, pero sigue siendo una mala elección: es una capa de software que consulta el temporizador del sistema (`esp_timer`), convierte un contador de 64 bits a milisegundos con una división, y su resolución es de 1 ms. En otras arquitecturas, como AVR, `millis()` directamente deja de avanzar dentro de una ISR.

`XTHAL_GET_CCOUNT()` lee el registro especial CCOUNT del núcleo Xtensa con una sola instrucción (`rsr.ccount`). Ese registro de 32 bits aumenta en uno con cada ciclo de reloj de la CPU, así que entrega una marca de tiempo en un ciclo, sin llamadas ni bloqueos y con una resolución de 6,25 ns a 160 MHz. Como solo se usa dentro de la ISR, que corre siempre en el mismo núcleo, las marcas son coherentes entre sí.

### Aritmética modular y el desborde de CCOUNT

CCOUNT es de 32 bits: cuenta de 0 a $2^{32}-1$ y vuelve a 0. A 160 MHz esto ocurre cada

$$\frac{2^{32}}{160 \times 10^{6}\ \text{Hz}} \approx 26{,}84\ \text{s}$$

y el umbral de 25 ms equivale a $0{,}025 \cdot 160 \times 10^{6} = 4\,000\,000$ ciclos.

Sean $T_1$ y $T_2$ los ciclos reales transcurridos desde el reinicio en dos disparos consecutivos (enteros que no se desbordan), y $c_1 = T_1 \bmod 2^{32}$, $c_2 = T_2 \bmod 2^{32}$ los valores que lee el registro. Se cumple

$$c_2 - c_1 \equiv T_2 - T_1 \pmod{2^{32}}$$

El estándar de C define la aritmética de enteros sin signo como aritmética módulo $2^{N}$ (ISO/IEC 9899, sección 6.2.5), así que la resta de `uint32_t` devuelve el único representante de ese resultado en $[0,\ 2^{32}-1]$. Si el tiempo real transcurrido cumple $0 \le T_2 - T_1 < 2^{32}$, ese representante es exactamente $T_2 - T_1$, haya o no desborde entre las dos lecturas.

Ejemplo numérico con desborde: $c_1 = 2^{32} - 10^{6} = 4\,293\,967\,296$ y $c_2 = 3\,000\,000$ después del desborde. La resta da $3\,000\,000 - 4\,293\,967\,296 = -4\,290\,967\,296$, que módulo $2^{32}$ es

$$-4\,290\,967\,296 + 4\,294\,967\,296 = 4\,000\,000$$

ciclos, justo los transcurridos ($10^{6}$ hasta el desborde más $3 \times 10^{6}$ después).

Limitación conocida: la deducción exige que entre dos disparos válidos pasen menos de 26,84 s. Si el tiempo real entre dos pulsaciones cae en los 25 ms posteriores a un múltiplo exacto de ese periodo, la resta daría un valor menor al umbral y se descartaría una pulsación legítima. Es una ventana de unos 25 ms cada 26,84 s, y la siguiente pulsación se acepta con normalidad.

## Referencias
- Espressif, ESP32 Technical Reference Manual, capítulo "LED PWM Controller (LEDC)".
- Espressif, ESP32 Series Datasheet, secciones de GPIO e IO MUX.
- Espressif, ESP-IDF Programming Guide: "Memory Types", "Interrupt Allocation" y "FreeRTOS (IDF)", sección de secciones críticas.
- Cadence Tensilica, Xtensa Instruction Set Architecture Reference Manual: registro CCOUNT e instrucciones de lectura de registros especiales.
- ISO/IEC 9899, sección 6.2.5 (aritmética de enteros sin signo).

