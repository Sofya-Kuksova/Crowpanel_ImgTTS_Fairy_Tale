#include "builtin_texts.h"
#include <stdatomic.h>

static _Atomic(builtin_text_case_t) s_current_case = CASE_TXT_01;

const char* get_builtin_text(void)
{
    switch ((builtin_text_case_t)atomic_load(&s_current_case)) {
        case CASE_TXT_01: // Old Beacon, Norway
            return "I still turn on the light, even when no one is watching.\n"
                   "The sea speaks to me with the language of the wind.\n"
                   "Every storm calls out the names of those who never returned.";
        case CASE_TXT_02: // Clocks Yard, Prague
            return "Here, time moves more gently, as if tiptoeing.\n"
                   "I hear the rain dripping on the bricks eyelids.\n"
                   "Sometimes the clock hands stop not because of a breakage, but for musing.";
        case CASE_TXT_03: // Deserted Railway Station, Argentina
            return "I can still hear the echo of the trains, even though they've been gone for twenty years.\n"
                   "The tickets are damp, but the names on them are still legible.\n"
                   "The journey is over, but the anticipation is not.";
        case CASE_TXT_04: // Village Covered with Snow, Japan
            return "I hide in the snow, like a letter that was never sent.\n"
                   "The smoke from the chimneys paints short fairy tales in the sky.\n"
                   "And while the snow falls, the world here is in no hurry to grow up.";
        case CASE_TXT_05: // Bridge across the River, Georgia
            return "I connect the shores, though I am immovable myself.\n"
                   "The water beneath me knows more stories than the people above.\n"
                   "When morning comes, I am the first to see the sun.";
        case CASE_TXT_06: // Graffiti alley, Lisbon
            return "Every morning I wake up with new colors.\n"
                   "Some write their prayers on my walls, others their jokes.\n"
                   "And while the city sleeps, I keep them all together.";
        case CASE_TXT_07: // Desert Well, Morocco
            return "My stones are warm even at night.\n"
                   "I remember the palms that scooped up water like hope.\n"
                   "Now only the stars gaze into my depths.";
        case CASE_TXT_08: // Misty Field, Poland
            return "I smell of damp earth and anticipation.\n"
                   "Footsteps in the fog speak louder than thoughts.\n"
                   "When it clears, no one will remember I was here.";
        case CASE_TXT_09: // Tree by the Seaside, Iceland
            return "The wind bends me, but I still hold the sky.\n"
                   "The sea is my neighbor, silent and eternal.\n"
                   "We grow old together, not counting the years.";
        case CASE_TXT_10: // Old Garden Bridge, China
            return "I see reflections that are centuries younger than me.\n"
                   "Frogs croak like ancient philosophers.\n"
                   "The only thing changes here is the color of the leaves.";
        case CASE_TXT_11: // Post Office, Scotland
            return "Once I was opened with the smell of paper and distant roads.\n"
                   "Letters were sent, and I was left waiting for answers.\n"
                   "Now I am just counting the raindrops on the window.";
        case CASE_TXT_12: // Caves in the Sea, Greece
            return "I breathe the sea, and the waves sing ancient names to me.\n"
                   "It's chilly inside me, dazzling outside.\n"
                   "Whoever enters here, takes away salt on their skin and silence in their hearts.";
        case CASE_TXT_13: // Roofless House, Portugal
            return "My window now looks straight into the clouds.\n"
                   "The rain is my only guest, but it's polite.\n"
                   "Sometimes the wind brings back the smell of old bread and laughter.";
        case CASE_TXT_14: // Sunrise on the Plateau, Mongolia
            return "I wake up when the sun is still deciding whether to rise.\n"
                   "The air here smells of freedom and milk.\n"
                   "And every sunrise sounds like a promise.";
        default:
            return 0;
    }
}

void builtin_text_next(void)
{
    builtin_text_case_t cur = atomic_load(&s_current_case);
    builtin_text_case_t nxt = (cur + 1) % CASE_TXT_COUNT;
    atomic_store(&s_current_case, nxt);
}

void builtin_text_set(builtin_text_case_t c)
{
    if (c >= 0 && c < CASE_TXT_COUNT) {
        atomic_store(&s_current_case, c);
    }
}

builtin_text_case_t builtin_text_get(void)
{
    return atomic_load(&s_current_case);
}
