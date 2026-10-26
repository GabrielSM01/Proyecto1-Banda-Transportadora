#pragma once
#include <stdint.h>

// Microinterruptor de paro de emergencia en IO33.
// La interrupción levanta una bandera; el bucle principal la consulta.
void botonInit();
bool botonTomarEvento();     // true si hubo una pulsación válida; limpia la bandera
uint32_t botonContador();    // pulsaciones válidas acumuladas desde el arranque
