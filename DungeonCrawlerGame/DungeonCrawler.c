// TODO: Armor, Items and Shield
// TODO: Battle Logic
// TODO: Dungeon Exploration

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NCURSES_STATIC
// Compiles with -lpdcurses on Windows and -lncurses with everything else
#ifdef _WIN32
    #include <ncurses/curses.h>
#else
    #include <ncurses.h> // standard on Linux/macOS
#endif

/* Because Weapon needs unit and unit needs weapon, 
this ensures the compiler is quiet;*/
typedef struct Unit Unit; 

// Structures:

typedef struct Item {
    void (*ItemEffect) (Unit*, Unit*);
} Item;

typedef enum {
    OFFHAND_NONE = 0,
    OFFHAND_WEAPON,
    OFFHAND_SHIELD
} OffhandType;

typedef struct Weapon {
    char Name[32];
    unsigned short int Type : 3; //0 = Unused for Now, 1 = Light, 2 = Versatile, 3 = Heavy, 4 = Breaker, 5 = Projectile, 6 = Firearm, 7 = Loaded;]
    unsigned int DamageType : 2; //0 = Physical, 1 = Magical. 2 or 3 means the weapon is mixed and can choose wheter to do physical or magical;
    char AmmoType[10]; //If the weapon type is 5 or above (consumes ammo), this is the name of the type of ammo it consumes, which will be used for comparisons;
    short int StatBonus; //As long as it is not offhand, grants a bonus to Atk (if damagetype is 0) or Mag (if Damagetype is 1);
    //If mixed, tries to assign equally, but gives priority to granting the bigger part to Atk if 2 or Mag if 3;
    void (*SpecialEffect) (Unit*, Unit*);
} Weapon;

typedef struct Shield {
    unsigned int Type : 3; //000(0) L.Shield, 001(1) L.Barrier 010(2) M.Shield, 011(3) M.Barrier, 100(4) H.Shield, 101(5) H.Barrier;
    void (*SpecialEffect) (Unit*, Unit*);
    short int StatBonus;
} Shield;

typedef struct Offhand {
    OffhandType SlotType; 
    union {
        Weapon *Weapon;
        Shield *Shield;
    };
} Offhand;

typedef struct Unit {
    char Name[32];
    int HP; //During battles, HP will be normally counted as 3*Fort, save circumstances.
    int Armor; //Armor is normally Def*2 unless the user is wearing [F.Plate] armor
    int Ward; //Ward is normally Res*2 unless the user is wearing [Ceremonial] armor
    short int Attack;
    short int Magic;
    short int Fortitude;
    short int Defense;
    short int Resistance;
    short int Speed;
    Weapon *EquippedWeapon;
    Offhand EquippedOffhand;
    void (*Passive) (Unit*, Unit*);
} Unit;

// Main Engine functions:

short int OverflowControlShort (int V) {
    if (V > 10000) {
        V = 10000;
    }
    else if (V < -10000) {
        V = -10000;
    }
    return (short int) V;
}

int MultiplesOfFive (int V, int FlagX) {
    //0 -> Rounds up
    //1 -> Rounds down
    //Anything else -> Rounds to closest
    switch (FlagX) {
        case 0:
            return ((V + 4) / 5) * 5;
        case 1:
            return (V / 5) * 5;
        default:
            return((V + 2) / 5) * 5;
    }
}

Unit CreateUnit(char *Name,
short int Attack,short int Magic,short int Fortitude,short int Defense,
short int Resistance,short int Speed, Weapon *InitWeapon, void (*Passive)(Unit*, Unit*)) {
    Unit NewUnit;
    strcpy(NewUnit.Name, Name);
    NewUnit.Attack = Attack;
    NewUnit.Magic = Magic;
    NewUnit.Fortitude = Fortitude;
    NewUnit.Defense = Defense;
    NewUnit.Resistance = Resistance;
    NewUnit.Speed = Speed;

    NewUnit.HP = Fortitude * 3;
    NewUnit.Armor = Defense * 2;   // Default rule (can be overwritten by F.Plate later)
    NewUnit.Ward = Resistance * 2; // Default rule (can be overwritten by Ceremonial later)
    
    // Equipment Bindings
    NewUnit.EquippedWeapon = InitWeapon;
    NewUnit.EquippedOffhand.SlotType = OFFHAND_NONE;
    NewUnit.EquippedOffhand.Weapon = NULL;
    
    //Passive
    NewUnit.Passive = Passive;
    return NewUnit;
}

// Debug and Formatting Functions

void PrintUnitStats(WINDOW *win, Unit U, const char **StatNames) {
    wclear(win); 
    box(win, 0, 0); 
    short int L = 0;
    short int M = 0;
    
    mvwprintw(win, 2, 2, "============================================================================");
    mvwprintw(win, 3, 2, "%s's Stats: %s", U.Name);
    mvwprintw(win, 5+M, 2, "HP    : %d/%d", U.HP, U.HP); M++;
    mvwprintw(win, 5+M, 2, "Armor : %d/%d", U.Armor, U.Armor); M++;
    mvwprintw(win, 5+M, 2, "Ward  : %d/%d", U.Armor, U.Armor); M++;
    mvwprintw(win, 5+M, 2, "             ", U.Armor, U.Armor); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d", StatNames[L++], U.Attack); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d", StatNames[L++], U.Magic); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d", StatNames[L++], U.Fortitude); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d", StatNames[L++], U.Defense); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d", StatNames[L++], U.Resistance); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d", StatNames[L++], U.Speed); M++;
    mvwprintw(win, 5+M, 2, "============================================================================");
    wrefresh(win);
}

void EnforceMinimumSize() {
    while (LINES < 24 || COLS < 80) {
        clear();
        attron(A_BOLD);
        mvprintw(0, 0, "Terminal is too small!");
        attroff(A_BOLD);
        
        mvprintw(2, 0, "Current size : %d x %d", COLS, LINES);
        mvprintw(3, 0, "Required size: 80 x 24");
        mvprintw(5, 0, "Please resize the window...");
        
        refresh();
        timeout(100); 
        getch();      
    }
    timeout(-1); 
    clear();
    refresh();
}

// Cleans the entire window and draws a new empty border
void FormattedCleanWindow(WINDOW *win) {
    wclear(win);
    box(win, 0, 0);
    wrefresh(win);
}

// Stat Assignment Menu
void AssignInitialStats(WINDOW *win, int *Stats, const char **StatNames) {
    int Cursor = 0;
    int Points = 30;
    keypad(win, TRUE);

    while (1) {
        FormattedCleanWindow(win);
        
        mvwprintw(win, 2, 18, "Use Arrow Keys or WASD to assign initial Stat Points");
        mvwprintw(win, 4, 31, "Remaining: [%2d/30]", Points);
        
        for (int i = 0; i < 6; i++) {
            if (i == Cursor) {
                mvwprintw(win, 8 + i, 25, "--->"); // Cursor placement
            }
            // Using %-4s pads the stat name properly to align the brackets
            mvwprintw(win, 8 + i, 32, "[%-4s] %2d", StatNames[i], Stats[i]);
        }
        
        mvwprintw(win, 18, 26, "- Press Enter to Continue -");
        wrefresh(win);
        
        int ch = wgetch(win);
        
        if (ch == KEY_UP || ch == 'w' || ch == 'W') {
            Cursor--;
            if (Cursor < 0) Cursor = 5; // Wrap to bottom
        } 
        else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
            Cursor++;
            if (Cursor > 5) Cursor = 0; // Wrap to top
        } 
        else if (ch == KEY_LEFT || ch == 'a' || ch == 'A') {
            int min_val = (Cursor == 2) ? 5 : 0; // FORT is index 2
            if (Stats[Cursor] > min_val) {
                Stats[Cursor] = Stats[Cursor] - 5;
                Points = Points + 5;
            }
        } 
        else if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') {
            if (Points > 0) {
                Stats[Cursor] = Stats[Cursor] + 5;
                Points = Points - 5;
            }
        } 
        // 10 and 13 are standard ASCII carriage returns/newlines used by Enter keys
        else if (ch == '\n' || ch == KEY_ENTER || ch == 10 || ch == 13) {
            if (Points > 0) {
                mvwprintw(win, 20, 5, "You haven't assigned all Points. Are you sure you want to proceed? (Y/N)");
                wrefresh(win);
                int confirm = wgetch(win);
                if (confirm == 'y' || confirm == 'Y') {
                    break;
                }
            } else {
                break; // Proceed normally if 0 Points remain
            }
        }
    }
}

void TitleScreen(WINDOW *win) {
    FormattedCleanWindow(win);
    mvwprintw(win, 11, 29, "[N]onsense presents...");
    wrefresh(win);
    napms(1500); // 1.5 second pause
    FormattedCleanWindow(win);

    const char *TitleArt[] = {
        "  ####",
        "  ## ##",
        "  #   #",
        "  #   #    #  #    # ##     ## #    ##      ##     # ##",
        "  #   #    #  #    ####    # ##    ####    ####    ####",
        "  #   #    #  #    #  #    #  #    #  #    #  #    #  #",
        "  #   #    #  #    #  #    #  #    ####    #  #    #  #",
        "  #   #    #  #    #  #    ####    #       #  #    #  #",
        "  #   #    #  #    #  #    ###     #  #    #  #    #  #",
        "  #  ##    # ##    #  #    ##      # ##    # ##    #  #",
        "  ####     ####    #  #    ####    ###     ###     #  #",
        "      ###                            #",
        "     ## ##                           #",
        "     #   #                           #",
        "     #   #    # #   ###      #  #    #     ##     # #",
        "     #        ###   # ##   # ## #    #    ####    ###",
        "     #        #     # ##   # ## #    #    #  #    #",
        "     #            ## # #  # #  #    ####    #",
        "     #   #    #     ####   ######    #    #       #",
        "     #   #    #    ## ##   ## ###    #    #  #    #",
        "     ## ##    #    ## ##   ##  #     #    # ##    #",
        "      ###     #     ####   ##  #     #    ###     #"
    };

    int Rows = sizeof(TitleArt) / sizeof(TitleArt[0]);

    for (int i = 0; i < Rows; i++) {
        mvwprintw(win, 1 + i, 10, "%s", TitleArt[i]);
    }

    
    mvwprintw(win, 0, 24, "- Press Any Key to Continue -");
    wrefresh(win);
    wgetch(win); 
    FormattedCleanWindow(win);
}

int main()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    EnforceMinimumSize();

    // 80x24 window
    int StartY = (LINES - 24) / 2;
    int StartX = (COLS - 80) / 2;
    WINDOW *win = newwin(24, 80, StartY, StartX);
    box(win, 0, 0);
    wrefresh(win);

    TitleScreen(win);

    char Name[32] = {'\0'};
    // 0=ATK, 1=MAG, 2=FORT, 3=DEF, 4=RES, 5=SPD
    int Stats[6] = {0, 0, 5, 0, 0, 0}; 
    const char *StatNames[] = {"ATK", "MAG", "FORT", "DEF", "RES", "SPD"};

    while (Name[0] == '\0') {
        mvwprintw(win, 2, 2, "Type in unit name:");
        wrefresh(win);
        echo();
        mvwgetnstr(win, 3, 2, Name, 31);
        noecho();
        if (Name[0] == '\0') {
            mvwprintw(win, 4, 2, "Name cannot be blank...");
            wrefresh(win);
        }
    }

    // Menu for Stats
    AssignInitialStats(win, Stats, StatNames);

    // ScreenSwap
    FormattedCleanWindow(win);
    mvwprintw(win, 2, 2, "============================================================================");
    mvwprintw(win, 3, 2, "Sorry, funny messages were nuked... only for now!");
    wrefresh(win);
    
    napms(800); // Arbitrary 800 ms wait

    mvwprintw(win, 5, 2, "Mwahahahaha!");
    mvwprintw(win, 6, 2, "============================================================================");
    wrefresh(win);
    
    napms(1200); 

    // Create and Print Final Unit
    Unit Player = CreateUnit(Name, Stats[0], Stats[1], Stats[2], Stats[3], Stats[4], Stats[5], NULL, NULL);
    PrintUnitStats(win, Player, StatNames);
    
    mvwprintw(win, 14, 2, "Press any key to close.");
    wrefresh(win);
    
    wgetch(win); // Waits for keypress
    
    endwin(); // Destroys visual allocations safely back to shell context
    return 0;
}