#pragma once

// Máquina de estados de la banda: IDLE, RUNNING y FAULT.
void fsmInit();         // deja el sistema en IDLE con el motor libre
void fsmActualizar();   // lee un evento y lo despacha por la matriz de transiciones
