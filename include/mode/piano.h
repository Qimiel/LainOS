#ifndef __OS__MODE__PIANO_H
#define __OS__MODE__PIANO_H

#include <drivers/speaker.h>


void pianoTUI();
void piano(bool keypress, char key);

os::common::uint16_t JournalFreqVal(char ch, os::common::uint8_t octave);
void JournalPlay(char ch, os::common::uint8_t octave, os::common::uint16_t time);


#endif
