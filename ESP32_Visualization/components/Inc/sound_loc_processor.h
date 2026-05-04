#ifndef SOUND_LOC_PROCESSOR_H
#define SOUND_LOC_PROCESSOR_H

/**
 * @brief Inicijalizira procesor lokalizacije zvuka.
 *        Pokreće FreeRTOS task koji sluša UART i parsira pakete kuta.
 */
void sound_loc_processor_init(void);

#endif // SOUND_LOC_PROCESSOR_H