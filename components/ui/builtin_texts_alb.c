#include "builtin_texts.h"
#include <stdatomic.h>

static _Atomic(builtin_text_case_t) s_current_case = CASE_TXT_01;

const char* get_builtin_text(void)
{
    switch ((builtin_text_case_t)atomic_load(&s_current_case)) {
        case CASE_TXT_01: // Grandfather’s Chair
            return "I guarded his naps, his books and still have the smell of pipe tobacco.\n"
                   "The cushion remembers every sigh after a long day.\n"
                   "Now the evening sunbeams keep his place warm.";
        case CASE_TXT_02: // Old Family Car
            return "I carried laughter, groceries, and arguments - in that order.\n"
                   "My back seat knew every song from the road to the sea.\n"
                   "Now I rest under lacy shadows of the tree crowns, dreaming of another trip.";
        case CASE_TXT_03: // The Family Kitchen
            return "Here, mornings begin before anyone wakes.\n"
                   "The smell of dough is older than the recipes.\n"
                   "Even silence here tastes like home.";
        case CASE_TXT_04: // Child’s Drawing on the Fridge
            return "I'm uneven, colorful, and absolutely unique.\n"
                   "They called me masterpiece before they learned perspective.\n"
                   "The fridge hums softly to keep me company.";
        case CASE_TXT_05: // Photo from the Beach
            return "The sea laughed louder than we did.\n"
                   "Someone lost a sandal and found a shell instead.\n"
                   "Salt still hides in the frame corners.";
        case CASE_TXT_06: // Grandmother’s Sewing Box
            return "My drawers rattle with tiny stories. Blue, green, and red.\n"
                   "She mended more hearts than sleeves here.\n"
                   "When the lid closes, I hum her favorite tune.";
        case CASE_TXT_07: // Child’s First Bicycle
            return "My tires once drew circles of freedom on the pavement.\n"
                   "The first fall hurt less than the pride of getting up.\n"
                   "Now I wait for smaller feet to find balance again.";
        case CASE_TXT_08: // Backyard Swing
            return "I remember laughter so light it could lift me higher.\n"
                   "Summer evenings smelled like grass and ice cream.\n"
                   "Now I sway by myself, practicing patience.";
        case CASE_TXT_09: // Family Dog
            return "I guarded the nights and the sandwiches.\n"
                   "Every bark had someone's name in it.\n"
                   "Now I sleep, still listening for footsteps.";
        case CASE_TXT_10: // Birthday Candles
            return "They counted the years. I just counted wishes.\n"
                   "Wax dripped faster than time.\n"
                   "If you close your eyes, I can still glow for a second.";
        case CASE_TXT_11: // Graduation Cap
            return "I flew once, higher than expected.\n"
                   "Applause filled the air, then silence took over.\n"
                   "I still smell of rain and new beginnings.";
        case CASE_TXT_12: // Family House Key
            return "I fit the same lock for thirty years.\n"
                   "Every turn meant: someone's home.\n"
                   "Now I jingle softly, remembering doors I opened.";
        case CASE_TXT_13: // Old Christmas Ornament
            return "I've seen more winters than I can count.\n"
                   "Hands change, laughter repeats.\n"
                   "When the lights return, I shine like memory itself.";
        case CASE_TXT_14: // Photo of the Family Reunion
            return "Half of them blinked, the other half joked about it.\n"
                   "No one noticed how perfect that moment really was.\n"
                   "If you listen closely, I'm still laughing.";
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
