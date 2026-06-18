#include <mode/piano.h>

using namespace os;
using namespace os::common;
using namespace os::drivers;


void TUI(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, bool);
void putcharTUI(unsigned char, unsigned char, unsigned char, uint8_t, uint8_t);
void printfTUI(char*, uint8_t, uint8_t, uint8_t, uint8_t);

void sleep(uint32_t);
void makeBeep(uint32_t);


void pianoTUI() {
	
	TUI(0x09, 0x0c, 20, 8, 60, 16, true);
	printfTUI("Piano mode, press ctrl+c to exit.", 0x0f, 0x0c, 0, 0);
	printfTUI("Use the top 2 rows of keys ('1' - '=') and ('q' - ']')", 0x0f, 0x0c, 0, 1);
	printfTUI("to play notes. (1 = C, 2 = C# etc.) Hold shift/caps to shift down 2 octaves.", 0x0f, 0x0c, 0, 2);
	printfTUI("Lain piano program.", 0x0f, 0x09, 21, 9);
	printfTUI("Last notes played: ", 0x0f, 0x09, 21, 11);
}


void JournalPlay(char ch, uint8_t octave, uint16_t time) {

	makeBeep(JournalFreqVal(ch, octave));
	sleep(time);
}

//use for music things outside of piano mode :)
uint16_t JournalFreqVal(char ch, uint8_t octave) {

	uint16_t JournalFreq = 0;
	
	switch (octave) {
		
		case 3:
			switch (ch) {
			case 'C': JournalFreq = 131; break;
			case 'c': JournalFreq = 139; break;
			case 'D': JournalFreq = 147; break;
			case 'd': JournalFreq = 156; break;
			case 'E': JournalFreq = 165; break;
			case 'F': JournalFreq = 175; break;
			case 'f': JournalFreq = 185; break;
			case 'G': JournalFreq = 196; break;
			case 'g': JournalFreq = 208; break;
			case 'A': JournalFreq = 220; break;
			case 'a': JournalFreq = 233; break;
			case 'B': JournalFreq = 247; break;
			default: break;
			}
			break;
		case 4:
			switch (ch) {
			case 'C': JournalFreq = 262; break;
			case 'c': JournalFreq = 277; break;
			case 'D': JournalFreq = 294; break;
			case 'd': JournalFreq = 311; break;
			case 'E': JournalFreq = 330; break;
			case 'F': JournalFreq = 349; break;
			case 'f': JournalFreq = 370; break;
			case 'G': JournalFreq = 392; break;
			case 'g': JournalFreq = 415; break;
			case 'A': JournalFreq = 440; break;
			case 'a': JournalFreq = 466; break;
			case 'B': JournalFreq = 494; break;
			default: break;
			}
			break;
		case 5:
			switch (ch) {
			case 'C': JournalFreq = 523; break;
			case 'c': JournalFreq = 554; break;
			case 'D': JournalFreq = 587; break;
			case 'd': JournalFreq = 622; break;
			case 'E': JournalFreq = 659; break;
			case 'F': JournalFreq = 698; break;
			case 'f': JournalFreq = 740; break;
			case 'G': JournalFreq = 784; break;
			case 'g': JournalFreq = 831; break;
			case 'A': JournalFreq = 880; break;
			case 'a': JournalFreq = 932; break;
			case 'B': JournalFreq = 988; break;
			default: break;
			}
			break;
		case 6:
			switch (ch) {
			case 'C': JournalFreq = 1047; break;
			case 'c': JournalFreq = 1109; break;
			case 'D': JournalFreq = 1175; break;
			case 'd': JournalFreq = 1245; break;
			case 'E': JournalFreq = 1319; break;
			case 'F': JournalFreq = 1397; break;
			case 'f': JournalFreq = 1480; break;
			case 'G': JournalFreq = 1568; break;
			case 'g': JournalFreq = 1661; break;
			case 'A': JournalFreq = 1760; break;
			case 'a': JournalFreq = 1865; break;
			case 'B': JournalFreq = 1976; break;
			default: break;
			}
			break;
		default:
			break;
	}


	return JournalFreq;
}

void piano(bool keypress, char key) {

        Speaker speaker;
        uint16_t JournalFreq = 0;
	char* JournalChar = "";

        if (keypress) {

                //key mappings to freq  
                switch (key) {

                        //speaker.NoSound();
			
			//lower octave #1

			//c
			case '!':
                                JournalFreq = 131;
                                JournalChar = "C-3 ";
				break;
			//c#
			case '@':
                                JournalFreq = 139;
                                JournalChar = "C#-3";
                                break;
			//d
			case '#':
                                JournalFreq = 147;
                                JournalChar = "D-3 ";
                                break;
			//d#
			case '$':
                                JournalFreq = 156;
                                JournalChar = "D#-3";
                                break;
			//e
			case '%':
                                JournalFreq = 165;
                                JournalChar = "E-3 ";
                                break;
			//f
			case '^':
                                JournalFreq = 175;
                                JournalChar = "F-3 ";
                                break;
			//f#
			case '&':
                                JournalFreq = 185;
                                JournalChar = "F#-3";
                                break;
			//g
			case '*':
                                JournalFreq = 196;
                                JournalChar = "G-3 ";
                                break;
			//g#
			case '(':
                                JournalFreq = 208;
                                JournalChar = "G#-3";
                                break;
			//a
			case ')':
                                JournalFreq = 220;
                                JournalChar = "A-3 ";
                                break;
			//a#
			case '_':
                                JournalFreq = 233;
                                JournalChar = "A#-3";
                                break;
			//b
			case '+':
                                JournalFreq = 247;
                                JournalChar = "B-3 ";
                                break;



			//lower octave #2

			//c
			case 'Q':
                                JournalFreq = 262;
                                JournalChar = "C-4 ";
                                break;
			//c#
			case 'W':
                                JournalFreq = 277;
                                JournalChar = "C#-4";
                                break;
			//d
			case 'E':
                                JournalFreq = 294;
                                JournalChar = "D-4 ";
                                break;
			//d#
			case 'R':
                                JournalFreq = 311;
                                JournalChar = "D#-4";
                                break;
			//e
			case 'T':
                                JournalFreq = 330;
                                JournalChar = "E-4 ";
                                break;
			//f
			case 'Y':
                                JournalFreq = 349;
                                JournalChar = "F-4 ";
                                break;
			//f#
			case 'U':
                                JournalFreq = 370;
                                JournalChar = "F#-4";
                                break;
			//g
			case 'I':
                                JournalFreq = 392;
                                JournalChar = "G-4 ";
                                break;
			//g#
			case 'O':
                                JournalFreq = 415;
                                JournalChar = "G#-4";
                                break;
			//a
			case 'P':
                                JournalFreq = 440;
                                JournalChar = "A-4 ";
                                break;
			//a#
			case '{':
                                JournalFreq = 466;
                                JournalChar = "A#-4";
                                break;
			//b
			case '}':
                                JournalFreq = 494;
                                JournalChar = "B-4 ";
                                break;



                        //upper octave #1
			
			//c
                        case '1':
                                JournalFreq = 523;
                                JournalChar = "C-5 ";
                                break;
                        //c#
                        case '2':
                                JournalFreq = 554;
                                JournalChar = "C#-5";
                                break;
                        //d
                        case '3':
                                JournalFreq = 587;
                                JournalChar = "D-5 ";
                                break;
                        //d#
                        case '4':
                                JournalFreq = 622;
                                JournalChar = "D#-5";
                                break;
                        //e
                        case '5':
                                JournalFreq = 659;
                                JournalChar = "E-5 ";
                                break;
                        //f
                        case '6':
                                JournalFreq = 698;
                                JournalChar = "F-5 ";
                                break;
                        //f#
                        case '7':
                                JournalFreq = 740;
                                JournalChar = "F#-5";
                                break;
                        //g
                        case '8':
                                JournalFreq = 784;
                                JournalChar = "G-5 ";
                                break;
                        //g#
                        case '9':
                                JournalFreq = 831;
                                JournalChar = "G#-5";
                                break;
                        //a
                        case '0':
                                JournalFreq = 880;
                                JournalChar = "A-5 ";
                                break;
                        //a#
                        case '-':
                                JournalFreq = 932;
                                JournalChar = "A#-5";
                                break;
                        //b
                        case '=':
                                JournalFreq = 988;
                                JournalChar = "B-5 ";
                                break;



                        //upper octave #2

                        //c
                        case 'q':
                                JournalFreq = 1047;
                                JournalChar = "C-6 ";
                                break;
                        //c#
                        case 'w':
                                JournalFreq = 1109;
                                JournalChar = "C#-6";
                                break;
                        //d
                        case 'e':
                                JournalFreq = 1175;
                                JournalChar = "D-6 ";
                                break;
                        //d#
			case 'r':
                                JournalFreq = 1245;
                                JournalChar = "D#-6";
                                break;
                        //e
                        case 't':
                                JournalFreq = 1319;
                                JournalChar = "E-6 ";
                                break;
                        //f
                        case 'y':
                                JournalFreq = 1397;
                                JournalChar = "F-6 ";
                                break;
                        //f#
                        case 'u':
                                JournalFreq = 1480;
                                JournalChar = "F#-6";
                                break;
                        //g
                        case 'i':
                                JournalFreq = 1568;
                                JournalChar = "G-6 ";
                                break;
                        //g#
                        case 'o':
                                JournalFreq = 1661;
                                JournalChar = "G#-6 ";
                                break;
                        //a
                        case 'p':
                                JournalFreq = 1760;
                                JournalChar = "A-6 ";
                                break;
                        //a#
                        case '[':
                                JournalFreq = 1865;
                                JournalChar = "A#-6";
                                break;
                        //b
                        case ']':
                                JournalFreq = 1976;
                                JournalChar = "B-6 ";
                                break;
                        default:
				return;
                                break;
                }
                printfTUI(JournalChar, 0x0f, 0x09, 39, 11);
		speaker.PlaySound(JournalFreq);
        }

        //stop sound when key not pressed
        if (!keypress) {

		sleep(1);
                speaker.NoSound();
        }
}



