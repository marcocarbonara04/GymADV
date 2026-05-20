#pragma once
#include <Arduino.h>

// --- EXERCISE DATABASE ---
// 10 muscle groups, up to 8 exercises each

const int NUM_MUSCLES = 11;
const int MAX_EX_PER_MUSCLE = 12;

const char* muscle_names[NUM_MUSCLES] = {
    "Chest", "Back", "Quadriceps", "Hamstrings", "Shoulders",
    "Biceps", "Triceps", "Core", "Glutes", "Calves", "Custom"
};

// Number of exercises per muscle group
const int ex_count[NUM_MUSCLES] = {12, 12, 12, 10, 12, 10, 10, 10, 10, 6, 0};

const char* exercise_db[NUM_MUSCLES][MAX_EX_PER_MUSCLE] = {
    // 0: Chest
    {"Bench Press", "Incline Bench", "Decline Bench", "Dumbbell Fly",
     "Cable Cross", "Pec Deck", "Chest Press", "Push-ups",
     "Low Cable Fly", "High Cable Fly", "Cable Press", "Single Cable Fly"},
    // 1: Back
    {"Lat Pulldown", "Seated Row", "Barbell Row", "T-Bar Row",
     "Pull-ups", "Cable Row", "Deadlift", "Face Pull",
     "Straight Pulldn", "Cable Pullover", "Single Lat Pulldn", "Kneeling Cable Row"},
    // 2: Quadriceps
    {"Squat", "Leg Press", "Leg Extension", "Hack Squat",
     "Front Squat", "Lunges", "Sissy Squat", "Wall Sit",
     "Cable Squat", "Cable Lunges", "Cable Step-up", "Cable Goblet Sq"},
    // 3: Hamstrings
    {"Leg Curl", "Romanian DL", "Stiff Leg DL", "Good Morning",
     "Nordic Curl", "Glute-Ham R.", "Cable Pull-Thr", "Cable Leg Curl",
     "Cable RDL", "Cable SL DL", "", ""},
    // 4: Shoulders
    {"OH Press", "Lateral Raise", "Front Raise", "Arnold Press",
     "Rev Pec Deck", "Upright Row", "Shrugs", "Face Pull",
     "Cable Lat Raise", "Cable Fnt Raise", "Cable Rear Fly", "Cable Upright Row"},
    // 5: Biceps
    {"Barbell Curl", "Dumbbell Curl", "Hammer Curl", "Preacher Curl",
     "Cable Curl", "Conc. Curl", "Cable Hammer", "Cable Overhd Curl",
     "Cable Preacher", "Cable Back Curl", "", ""},
    // 6: Triceps
    {"Tricep Push", "Skull Crush", "Dips", "Overhead Ext",
     "Kickback", "Close Grip BP", "Cable Overhd Ext", "Cable Kickback",
     "Cable Single Push", "Cable Rope Push", "", ""},
    // 7: Core
    {"Crunch", "Plank", "Leg Raise", "Cable Crunch",
     "Ab Machine", "Russian Tw.", "Cable Woodchop", "Cable Pallof",
     "Cable Kneel Twist", "Cable Rev Crunch", "", ""},
    // 8: Glutes
    {"Hip Thrust", "Glute Bridge", "Cable Kickbk", "Bulgarian SS",
     "Step-ups", "Sumo Squat", "Cable Glute Kic", "Cable Pull-Thr",
     "Cable Abduction", "Cable Hip Ext", "", ""},
    // 9: Calves
    {"Calf Raise", "Seated Calf", "Donkey Calf", "Toe Press",
     "Cable Calf Raise", "Cable Donkey Calf", "", "", "", "", "", ""},
    // 10: Custom
    {"", "", "", "", "", "", "", "", "", "", "", ""}
};

// Equipment types for exercise icons
enum EquipType {
    EQ_BARBELL = 0,
    EQ_DUMBBELL,
    EQ_CABLE,
    EQ_MACHINE,
    EQ_BODYWEIGHT,
    EQ_BENCH,
    EQ_BAR,      // pull-up bar
    EQ_PLATE     // plate-loaded
};

// Map each exercise to equipment type
const uint8_t exercise_equip[NUM_MUSCLES][MAX_EX_PER_MUSCLE] = {
    {EQ_BARBELL, EQ_BARBELL, EQ_BARBELL, EQ_DUMBBELL, EQ_CABLE, EQ_MACHINE, EQ_MACHINE, EQ_BODYWEIGHT, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE}, // Chest
    {EQ_CABLE, EQ_CABLE, EQ_BARBELL, EQ_BARBELL, EQ_BAR, EQ_CABLE, EQ_BARBELL, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE},               // Back
    {EQ_BARBELL, EQ_MACHINE, EQ_MACHINE, EQ_MACHINE, EQ_BARBELL, EQ_DUMBBELL, EQ_BODYWEIGHT, EQ_BODYWEIGHT, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE}, // Quads
    {EQ_MACHINE, EQ_BARBELL, EQ_BARBELL, EQ_BARBELL, EQ_BODYWEIGHT, EQ_MACHINE, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE, 0, 0},                   // Hams
    {EQ_BARBELL, EQ_DUMBBELL, EQ_DUMBBELL, EQ_DUMBBELL, EQ_MACHINE, EQ_BARBELL, EQ_DUMBBELL, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE},  // Shoulders
    {EQ_BARBELL, EQ_DUMBBELL, EQ_DUMBBELL, EQ_BENCH, EQ_CABLE, EQ_DUMBBELL, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE, 0, 0},                       // Biceps
    {EQ_CABLE, EQ_BARBELL, EQ_BODYWEIGHT, EQ_DUMBBELL, EQ_DUMBBELL, EQ_BARBELL, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE, 0, 0},                   // Triceps
    {EQ_BODYWEIGHT, EQ_BODYWEIGHT, EQ_BODYWEIGHT, EQ_CABLE, EQ_MACHINE, EQ_PLATE, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE, 0, 0},                 // Core
    {EQ_BARBELL, EQ_BODYWEIGHT, EQ_CABLE, EQ_DUMBBELL, EQ_BODYWEIGHT, EQ_BARBELL, EQ_CABLE, EQ_CABLE, EQ_CABLE, EQ_CABLE, 0, 0},                 // Glutes
    {EQ_MACHINE, EQ_MACHINE, EQ_MACHINE, EQ_MACHINE, EQ_CABLE, EQ_CABLE, 0, 0, 0, 0, 0, 0},                                                     // Calves
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}                                                                                                        // Custom
};

// --- Helper: draw a human body silhouette (front view) ---
// Draws at gx,gy in a 50x60 box. Highlights the target muscle in 'color'.
inline void drawMuscleIcon(M5Canvas &c, int gx, int gy, int mi, uint16_t color) {
    uint16_t BG    = M5Cardputer.Display.color565(20, 20, 22);
    uint16_t SKIN  = M5Cardputer.Display.color565(180, 150, 120);
    uint16_t SKIN2 = M5Cardputer.Display.color565(160, 130, 100); // shadow skin
    uint16_t BODY  = M5Cardputer.Display.color565(65, 65, 72);
    uint16_t OUTLINE = M5Cardputer.Display.color565(45, 45, 50);
    uint16_t SHADE = M5Cardputer.Display.color565(35, 35, 40);
    
    // Background card
    c.fillRoundRect(gx, gy, 50, 60, 6, BG);
    c.drawRoundRect(gx, gy, 50, 60, 6, OUTLINE);
    
    int cx = gx + 25;
    int ty = gy + 4; // top of head
    
    // --- BODY SILHOUETTE (front view anatomical) ---
    // Head
    c.fillCircle(cx, ty + 5, 5, SKIN);
    c.drawCircle(cx, ty + 5, 5, SKIN2);
    // Neck
    c.fillRect(cx - 2, ty + 10, 4, 4, SKIN);
    
    // Torso (trapezoid shape)
    c.fillTriangle(cx - 2, ty + 13, cx - 12, ty + 18, cx - 10, ty + 36, BODY);
    c.fillTriangle(cx + 2, ty + 13, cx + 12, ty + 18, cx + 10, ty + 36, BODY);
    c.fillRect(cx - 10, ty + 20, 20, 16, BODY);
    
    // Waist
    c.fillRect(cx - 8, ty + 36, 16, 4, BODY);
    
    // Legs
    c.fillRoundRect(cx - 8, ty + 39, 6, 14, 2, BODY);
    c.fillRoundRect(cx + 2, ty + 39, 6, 14, 2, BODY);
    
    // Feet
    c.fillRect(cx - 9, ty + 52, 8, 2, SKIN2);
    c.fillRect(cx + 1, ty + 52, 8, 2, SKIN2);
    
    // Arms (resting at sides)
    c.fillRoundRect(cx - 15, ty + 15, 4, 16, 2, SKIN);
    c.fillRoundRect(cx + 11, ty + 15, 4, 16, 2, SKIN);
    // Forearms
    c.fillRoundRect(cx - 15, ty + 30, 4, 10, 2, SKIN);
    c.fillRoundRect(cx + 11, ty + 30, 4, 10, 2, SKIN);
    
    // --- MUSCLE HIGHLIGHTS ---
    switch(mi) {
        case 0: // CHEST - pectorals
            c.fillRoundRect(cx - 10, ty + 15, 9, 7, 3, color);
            c.fillRoundRect(cx + 1, ty + 15, 9, 7, 3, color);
            // Add pec separation line
            c.drawLine(cx, ty + 16, cx, ty + 21, SHADE);
            // Slight shading under pecs
            c.drawLine(cx - 9, ty + 21, cx - 2, ty + 22, SHADE);
            c.drawLine(cx + 2, ty + 22, cx + 9, ty + 21, SHADE);
            break;
            
        case 1: // BACK - lats (V-taper shown from front)
            c.fillTriangle(cx - 12, ty + 17, cx - 10, ty + 34, cx - 5, ty + 22, color);
            c.fillTriangle(cx + 12, ty + 17, cx + 10, ty + 34, cx + 5, ty + 22, color);
            // Show lat spread
            c.drawLine(cx - 12, ty + 17, cx - 11, ty + 32, SHADE);
            c.drawLine(cx + 12, ty + 17, cx + 11, ty + 32, SHADE);
            break;
            
        case 2: // QUADRICEPS - front thighs
            c.fillRoundRect(cx - 8, ty + 39, 7, 11, 3, color);
            c.fillRoundRect(cx + 1, ty + 39, 7, 11, 3, color);
            // Quad separation
            c.drawLine(cx - 5, ty + 40, cx - 5, ty + 49, SHADE);
            c.drawLine(cx + 4, ty + 40, cx + 4, ty + 49, SHADE);
            break;
            
        case 3: // HAMSTRINGS - back of thighs (shown as inner shade)
            c.fillRoundRect(cx - 7, ty + 41, 5, 10, 2, color);
            c.fillRoundRect(cx + 2, ty + 41, 5, 10, 2, color);
            // Hamstring tendon lines
            c.drawLine(cx - 5, ty + 50, cx - 3, ty + 42, SHADE);
            c.drawLine(cx + 5, ty + 50, cx + 3, ty + 42, SHADE);
            break;
            
        case 4: // SHOULDERS - deltoids (3 heads)
            c.fillCircle(cx - 12, ty + 16, 4, color);
            c.fillCircle(cx + 12, ty + 16, 4, color);
            // Delt separation (anterior/medial/posterior)
            c.drawLine(cx - 14, ty + 15, cx - 10, ty + 18, SHADE);
            c.drawLine(cx + 14, ty + 15, cx + 10, ty + 18, SHADE);
            break;
            
        case 5: // BICEPS
            c.fillRoundRect(cx - 16, ty + 17, 5, 10, 3, color);
            c.fillRoundRect(cx + 11, ty + 17, 5, 10, 3, color);
            // Bicep peak
            c.fillCircle(cx - 14, ty + 19, 2, color);
            c.fillCircle(cx + 13, ty + 19, 2, color);
            break;
            
        case 6: // TRICEPS (back of arms, shown as lower arm shade)
            c.fillRoundRect(cx - 15, ty + 22, 4, 10, 2, color);
            c.fillRoundRect(cx + 11, ty + 22, 4, 10, 2, color);
            // Horseshoe shape hint
            c.drawLine(cx - 14, ty + 23, cx - 14, ty + 30, SHADE);
            c.drawLine(cx + 13, ty + 23, cx + 13, ty + 30, SHADE);
            break;
            
        case 7: // CORE - abs (6-pack)
            c.fillRoundRect(cx - 6, ty + 24, 12, 14, 2, color);
            // Ab lines (6 pack)
            c.drawLine(cx, ty + 25, cx, ty + 37, SHADE);
            c.drawLine(cx - 5, ty + 28, cx + 5, ty + 28, SHADE);
            c.drawLine(cx - 5, ty + 32, cx + 5, ty + 32, SHADE);
            c.drawLine(cx - 4, ty + 36, cx + 4, ty + 36, SHADE);
            break;
            
        case 8: // GLUTES
            c.fillRoundRect(cx - 9, ty + 36, 8, 6, 3, color);
            c.fillRoundRect(cx + 1, ty + 36, 8, 6, 3, color);
            // Glute separation
            c.drawLine(cx, ty + 37, cx, ty + 41, SHADE);
            break;
            
        case 9: // CALVES - lower legs
            c.fillRoundRect(cx - 7, ty + 46, 5, 7, 2, color);
            c.fillRoundRect(cx + 2, ty + 46, 5, 7, 2, color);
            // Calf muscle shape (diamond)
            c.fillCircle(cx - 5, ty + 48, 2, color);
            c.fillCircle(cx + 4, ty + 48, 2, color);
            break;
    }
    
    // Subtle label at bottom
    c.setTextColor(M5Cardputer.Display.color565(100, 100, 105));
    c.setTextSize(1);
    const char* shortNames[] = {"PEC","LAT","QAD","HAM","DLT","BIC","TRI","ABS","GLT","CLF"};
    c.drawString(shortNames[mi], gx + 14, gy + 53);
}

// --- Equipment mini-icon (24x24, drawn at ex,ey) ---
inline void drawEquipIcon(M5Canvas &c, int ex, int ey, uint8_t eqType, uint16_t accentCol) {
    uint16_t IRON  = M5Cardputer.Display.color565(150, 150, 160);
    uint16_t PLATE = M5Cardputer.Display.color565(80, 80, 90);
    uint16_t GRIP  = M5Cardputer.Display.color565(60, 60, 65);
    uint16_t CABLE = M5Cardputer.Display.color565(170, 170, 180);
    uint16_t FRAME = M5Cardputer.Display.color565(55, 55, 60);
    uint16_t PAD   = M5Cardputer.Display.color565(100, 60, 30);
    
    switch(eqType) {
        case EQ_BARBELL: // Barbell (horizontal bar + plates)
            c.fillRect(ex + 3, ey + 11, 18, 2, IRON);
            c.fillRoundRect(ex + 1, ey + 8, 4, 8, 1, PLATE);
            c.fillRoundRect(ex + 19, ey + 8, 4, 8, 1, PLATE);
            c.fillRect(ex + 0, ey + 9, 2, 6, accentCol);
            c.fillRect(ex + 22, ey + 9, 2, 6, accentCol);
            break;
            
        case EQ_DUMBBELL: // Dumbbell (compact shape)
            c.fillRect(ex + 8, ey + 10, 8, 4, IRON);
            c.fillRoundRect(ex + 3, ey + 8, 6, 8, 2, PLATE);
            c.fillRoundRect(ex + 15, ey + 8, 6, 8, 2, PLATE);
            c.fillRect(ex + 4, ey + 9, 4, 6, accentCol);
            c.fillRect(ex + 16, ey + 9, 4, 6, accentCol);
            break;
            
        case EQ_CABLE: // Cable machine (tower + cable)
            c.fillRect(ex + 18, ey + 2, 4, 20, FRAME);
            c.fillRect(ex + 17, ey + 2, 6, 3, FRAME);
            c.drawLine(ex + 20, ey + 5, ex + 20, ey + 12, CABLE);
            c.drawLine(ex + 20, ey + 12, ex + 10, ey + 18, CABLE);
            c.fillRoundRect(ex + 6, ey + 16, 8, 4, 1, GRIP);
            c.fillCircle(ex + 20, ey + 5, 2, accentCol);
            // Weight stack
            for(int i = 0; i < 4; i++) {
                c.fillRect(ex + 2, ey + 4 + i * 4, 10, 3, (i < 2) ? accentCol : PLATE);
            }
            break;
            
        case EQ_MACHINE: // Machine (seat + pad frame)
            c.fillRect(ex + 16, ey + 2, 3, 20, FRAME);
            c.fillRoundRect(ex + 4, ey + 14, 14, 4, 1, PAD);
            c.fillRect(ex + 6, ey + 18, 3, 4, FRAME);
            c.fillRect(ex + 14, ey + 18, 3, 4, FRAME);
            // Lever arm
            c.drawLine(ex + 17, ey + 4, ex + 8, ey + 8, IRON);
            c.fillCircle(ex + 17, ey + 4, 2, accentCol);
            c.fillRoundRect(ex + 4, ey + 6, 8, 3, 1, GRIP);
            // Weight stack indicator
            c.fillRect(ex + 19, ey + 6, 4, 10, PLATE);
            c.fillRect(ex + 19, ey + 6, 4, 4, accentCol);
            break;
            
        case EQ_BODYWEIGHT: // Bodyweight (person silhouette)
            c.fillCircle(ex + 12, ey + 4, 3, accentCol);
            c.fillRoundRect(ex + 10, ey + 8, 4, 8, 2, accentCol);
            // Arms spread
            c.drawLine(ex + 10, ey + 10, ex + 5, ey + 14, accentCol);
            c.drawLine(ex + 14, ey + 10, ex + 19, ey + 14, accentCol);
            // Legs
            c.drawLine(ex + 11, ey + 16, ex + 8, ey + 22, accentCol);
            c.drawLine(ex + 13, ey + 16, ex + 16, ey + 22, accentCol);
            break;
            
        case EQ_BENCH: // Bench (flat bench)
            c.fillRoundRect(ex + 2, ey + 12, 20, 4, 1, PAD);
            c.fillRect(ex + 4, ey + 16, 3, 6, FRAME);
            c.fillRect(ex + 17, ey + 16, 3, 6, FRAME);
            // Uprights
            c.fillRect(ex + 2, ey + 4, 2, 12, FRAME);
            c.fillRect(ex + 1, ey + 4, 4, 2, IRON);
            // Barbell on top
            c.fillRect(ex + 0, ey + 3, 24, 1, IRON);
            c.fillRect(ex + 0, ey + 1, 3, 4, accentCol);
            c.fillRect(ex + 21, ey + 1, 3, 4, accentCol);
            break;
            
        case EQ_BAR: // Pull-up bar
            c.fillRect(ex + 1, ey + 4, 22, 2, IRON);
            c.fillRect(ex + 2, ey + 2, 2, 4, FRAME);
            c.fillRect(ex + 20, ey + 2, 2, 4, FRAME);
            // Person hanging
            c.fillCircle(ex + 12, ey + 10, 2, accentCol);
            c.fillRect(ex + 11, ey + 12, 2, 5, accentCol);
            c.drawLine(ex + 11, ey + 12, ex + 8, ey + 6, accentCol);
            c.drawLine(ex + 13, ey + 12, ex + 16, ey + 6, accentCol);
            c.drawLine(ex + 11, ey + 17, ex + 9, ey + 22, accentCol);
            c.drawLine(ex + 13, ey + 17, ex + 15, ey + 22, accentCol);
            break;
            
        case EQ_PLATE: // Plate (circular weight)
            c.drawCircle(ex + 12, ey + 12, 9, IRON);
            c.drawCircle(ex + 12, ey + 12, 8, PLATE);
            c.fillCircle(ex + 12, ey + 12, 6, accentCol);
            c.fillCircle(ex + 12, ey + 12, 2, GRIP);
            // Weight text
            c.setTextColor(IRON);
            c.setTextSize(1);
            break;
    }
}
