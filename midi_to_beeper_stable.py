# Convert MIDI to song_t structs for Super-2023-Swadge-FW
# Outputs to stdout

from dataclasses import dataclass, field
from typing import List
import sys
from mido import MidiFile

pitch_to_str = {
    0: 'SILENCE',
    12: 'C_0',
    13: 'C_SHARP_0',
    14: 'D_0',
    15: 'D_SHARP_0',
    16: 'E_0',
    17: 'F_0',
    18: 'F_SHARP_0',
    19: 'G_0',
    20: 'G_SHARP_0',
    21: 'A_0',
    22: 'A_SHARP_0',
    23: 'B_0',
    24: 'C_1',
    25: 'C_SHARP_1',
    26: 'D_1',
    27: 'D_SHARP_1',
    28: 'E_1',
    29: 'F_1',
    30: 'F_SHARP_1',
    31: 'G_1',
    32: 'G_SHARP_1',
    33: 'A_1',
    34: 'A_SHARP_1',
    35: 'B_1',
    36: 'C_2',
    37: 'C_SHARP_2',
    38: 'D_2',
    39: 'D_SHARP_2',
    40: 'E_2',
    41: 'F_2',
    42: 'F_SHARP_2',
    43: 'G_2',
    44: 'G_SHARP_2',
    45: 'A_2',
    46: 'A_SHARP_2',
    47: 'B_2',
    48: 'C_3',
    49: 'C_SHARP_3',
    50: 'D_3',
    51: 'D_SHARP_3',
    52: 'E_3',
    53: 'F_3',
    54: 'F_SHARP_3',
    55: 'G_3',
    56: 'G_SHARP_3',
    57: 'A_3',
    58: 'A_SHARP_3',
    59: 'B_3',
    60: 'C_4',
    61: 'C_SHARP_4',
    62: 'D_4',
    63: 'D_SHARP_4',
    64: 'E_4',
    65: 'F_4',
    66: 'F_SHARP_4',
    67: 'G_4',
    68: 'G_SHARP_4',
    69: 'A_4',
    70: 'A_SHARP_4',
    71: 'B_4',
    72: 'C_5',
    73: 'C_SHARP_5',
    74: 'D_5',
    75: 'D_SHARP_5',
    76: 'E_5',
    77: 'F_5',
    78: 'F_SHARP_5',
    79: 'G_5',
    80: 'G_SHARP_5',
    81: 'A_5',
    82: 'A_SHARP_5',
    83: 'B_5',
    84: 'C_6',
    85: 'C_SHARP_6',
    86: 'D_6',
    87: 'D_SHARP_6',
    88: 'E_6',
    89: 'F_6',
    90: 'F_SHARP_6',
    91: 'G_6',
    92: 'G_SHARP_6',
    93: 'A_6',
    94: 'A_SHARP_6',
    95: 'B_6',
    96: 'C_7',
    97: 'C_SHARP_7',
    98: 'D_7',
    99: 'D_SHARP_7',
    100: 'E_7',
    101: 'F_7',
    102: 'F_SHARP_7',
    103: 'G_7',
    104: 'G_SHARP_7',
    105: 'A_7',
    106: 'A_SHARP_7',
    107: 'B_7',
    108: 'C_8',
    109: 'C_SHARP_8',
    110: 'D_8',
    111: 'D_SHARP_8',
    112: 'E_8',
    113: 'F_8',
    114: 'F_SHARP_8',
    115: 'G_8',
    116: 'G_SHARP_8',
    117: 'A_8',
    118: 'A_SHARP_8',
    119: 'B_8',
}

@dataclass
class Note:
    note: str
    timeMs: int
    dutyCyclePct: float

    def __str__(self):
        return f"{{.note = {self.note}, .timeMs = {int(self.timeMs)}}}"

@dataclass
class Song:
    notes: List['Note']
    numNotes: int = field(init=False)
    shouldLoop: bool

    def __post_init__(self):
        self.numNotes = len(self.notes)

    def to_struct(self, name='song'):
        return \
"""const song_t {} =
{{
    .notes =
    {{
        {}
    }},
    .numNotes = {},
    .shouldLoop = {}
}};
""".format(name, "        ".join([f"{note},\n" for note in self.notes]), self.numNotes, str(self.shouldLoop).lower())

def main():
    if len(sys.argv) < 2:
        print(f"Format: python {sys.argv[0]} <input.mid> > <output.c>", file=sys.stderr)
        print(f"This program outputs to the terminal (stdout), so you can write it to a file as shown above.", file=sys.stderr)
        sys.exit()

    mid = MidiFile(sys.argv[1])

    notes = []

    last_note_event_delta = 0
    last_note_on = None
    note_still_on = False
    current_duty_cycle = 0.5
    for msg in mid:
        if msg.type == 'note_on':
            # If more than one note is on, cut off the previous note
            if note_still_on and last_note_event_delta + msg.time > 0 and last_note_on is not None:
                notes.append(Note(note = pitch_to_str[last_note_on.note], timeMs = 1000 * (last_note_event_delta + msg.time), dutyCyclePct = current_duty_cycle))
            # If no note but delta between note off and note on, then there was silence
            elif not note_still_on and last_note_event_delta + msg.time > 0:
                notes.append(Note(note = 'SILENCE', timeMs = 1000 * (last_note_event_delta + msg.time), dutyCyclePct = current_duty_cycle))
            last_note_event_delta = 0
            last_note_on = msg
            note_still_on = True
        elif msg.type == 'note_off':
            # Ignore note off it doesn't match with a note on
            if not note_still_on or (last_note_on is not None and msg.note != last_note_on.note):
                last_note_event_delta += msg.time
            else:
                # If a note on event happened previously, make a note with the time between the note on and note off events
                if note_still_on and last_note_event_delta + msg.time > 0 and last_note_on is not None:
                    notes.append(Note(note = pitch_to_str[last_note_on.note], timeMs = 1000 * (last_note_event_delta + msg.time), dutyCyclePct = current_duty_cycle))
                last_note_event_delta = 0           
                note_still_on = False
        elif msg.type == 'control_change' and msg.control == 1:
            # if modulation wheel, change the duty cycle percent
            # map value from 0 to 127 to 0.5 to 0.0001
            current_duty_cycle = (msg.value) / (127) * (0.5)
            last_note_event_delta += msg.time
        else:
            last_note_event_delta += msg.time

    song = Song(notes=notes, shouldLoop=True)

    print(song.to_struct(name='song'))


if __name__ == '__main__':
    main()