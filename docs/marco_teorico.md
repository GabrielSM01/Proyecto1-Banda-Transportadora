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

## Referencias
- Espressif, ESP32 Technical Reference Manual, capítulo "LED PWM Controller (LEDC)".
- Espressif, ESP32 Series Datasheet, secciones de GPIO e IO MUX.