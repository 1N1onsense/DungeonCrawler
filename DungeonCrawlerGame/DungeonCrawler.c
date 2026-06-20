// TODO: Clothing,
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

#define MAX_INVENTORY_SLOTS 5

/* Because Weapon needs unit and unit needs weapon, 
this ensures the compiler is quiet;*/
typedef struct Unit Unit; 

// Structures:

typedef enum {
    OFFHAND_NONE = 0,
    OFFHAND_WEAPON,
    OFFHAND_SHIELD
} OffhandType;

typedef struct Weapon {
    char Name[18]; 
    unsigned short int Type : 3; //0 = Unused for Now, 1 = Light, 2 = Versatile, 3 = Heavy, 4 = Breaker, 5 = Projectile, 6 = Firearm, 7 = Loaded;]
    unsigned int DamageType : 2; //0 = Physical, 1 = Magical. 2 or 3 means the weapon is mixed and can choose wheter to do physical or magical;
    char AmmoType[18]; //If the weapon type is 5 or above (consumes ammo), this is the name of the type of ammo it consumes, which will be used for comparisons;
    short int StatBonus; //As long as it is not offhand, grants a bonus to Atk (if damagetype is 0) or Mag (if Damagetype is 1);
    //If mixed, tries to assign equally, but gives priority to granting the bigger part to Atk if 2 or Mag if 3;
    short int AmmoCapacity; //Determines how many shots the weapon can give before reloading if ranged
    short int CurrentAmmo;//Determines how much ammo the weapon currently has loaded
    void (*SpecialEffect) (Unit*, Unit*);
} Weapon;
//Dummy Weapon Prototype
Weapon Dummy = {
    .Name = "S.S Strong Dummy!",
    .Type = 1,
    .DamageType = 0,
    .AmmoType = "\0",
    .StatBonus = 5,
    .AmmoCapacity = 8,
    .CurrentAmmo = 6,
    .SpecialEffect = NULL
};

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

typedef struct Item {
    char Name[20];
    short int Potency;
    short int Weight;
    short int Amount;
    void (*Effect) (Unit*, Unit*);
} Item;

typedef struct Clothing {
    char Name[18];
    unsigned int Type : 3; //0 = No Clothing, 1 = Light, 2 = Medium, 3 = Heavy, 4 = Ceremonial;
    short int StatBonus; //Like the weapon bonus
    unsigned int DefenseType : 2; //Determines if the statbonus will go to 0 = Def, 1 = Res, 2 = Fort.
    void (*SpecialEffect) (Unit*, Unit*);
} Clothing;

//Dummy Clothing Prototype. ERASE LATER
Clothing DummyPlate = {
    .Name = "Ultra Dummy",
    .Type = 1,
    .StatBonus = 5,
    .DefenseType = 0,
    .SpecialEffect = NULL
};

typedef struct Unit {
    char Name[32];

    int HP;
    int Armor;
    int Ward;

    //Current values for those ints above
    int CurrentHP;
    int CurrentArmor;
    int CurrentWard;

    //Unit's Inventory
    Item *Inventory[MAX_INVENTORY_SLOTS];
    short int InventoryMaxCapacity;
    short int InventoryWeight; //Raises when item is added, loses when consumed or tossed

    short int Attack;
    short int Magic;
    short int Fortitude;
    short int Defense;
    short int Resistance;
    short int Speed;
    Weapon *EquippedWeapon;
    Clothing *EquippedClothing;
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

int OverflowControlFlexible (int V, int Max, int Min) {
    if (V > Max) {
        V = Max;
    }
    else if (V < Min) {
        V = Min;
    }
    return  V;
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

void RefreshUnitDefensiveStats(Unit *U) {
    U->HP = 3*(U->Fortitude); if (U->CurrentHP > U->HP) U->CurrentHP = U->HP;
    U->Armor = 2*(U->Defense); if (U->CurrentArmor > U->Armor) U->CurrentArmor = U->Armor;
    U->Ward = 2*(U->Resistance); if (U->CurrentWard > U->Ward) U->CurrentWard = U->Ward;
}

void EquipUnequipClothing(Unit *U, Clothing* Cloth, int Command) {
    switch (Command) {
        //Unequipping Clothing, always run before equipping each new clothing
        case 0:
            RefreshUnitDefensiveStats(U);
            U->EquippedClothing = NULL;
            break;
        //Equipping Clothing
        case 1:
            U->EquippedClothing = Cloth;
            //I should use an ENUM here
            //0 = No Clothing, 1 = Light, 2 = Medium, 3 = Full Plate, 4 = Ceremonial;
            //0 = Def, 1 = Res, 2 = Fort.
            switch (Cloth->Type) {
                //If Full Plate or Ceremonial
                case 3: U->Armor = 3*(U->Defense + Cloth->StatBonus); break;
                case 4: U->Ward = 3* (U->Resistance + Cloth->StatBonus); break;
                default:
                //if Anything else
                    switch (Cloth->DefenseType) {
                        case 0: U->Armor = (Cloth->StatBonus + U->Defense)*2; break;
                        case 1: U->Ward = (Cloth->StatBonus + U->Resistance)*2; break;
                        case 2: U->HP = (Cloth->StatBonus + U->Fortitude)*3; break;
                    }
                    break;
            }
            break;
    }
    U->CurrentArmor = U->Armor;
    U->CurrentWard = U->Ward;
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
    
    NewUnit.CurrentHP = NewUnit.HP;
    NewUnit.CurrentArmor = NewUnit.Armor;
    NewUnit.CurrentWard = NewUnit.Ward;

    // Equipment Bindings
    NewUnit.EquippedWeapon = InitWeapon;
    NewUnit.EquippedOffhand.SlotType = OFFHAND_NONE;
    NewUnit.EquippedOffhand.Weapon = NULL;
    NewUnit.EquippedClothing = NULL;
    
    NewUnit.InventoryMaxCapacity = 50; //Default weight of things should be around 10, Ammo much lighter
    for (int i = 0; i < MAX_INVENTORY_SLOTS; i++) {
        NewUnit.Inventory[i] = NULL;
    };

    //Passive
    NewUnit.Passive = Passive;
    return NewUnit;
}

int StatBuffs(Unit *U, int Control) {
    switch (Control) {
        // Checks if weapon is physical
        case 0:
            if (U->EquippedWeapon != NULL && (U->EquippedWeapon->DamageType == 0 || U->EquippedWeapon->DamageType == 2)) {
                return U->Attack + U->EquippedWeapon->StatBonus;
            }
            return U->Attack;
            
        // Checks if weapon is magical
        case 1:
            if (U->EquippedWeapon != NULL && (U->EquippedWeapon->DamageType == 1 || U->EquippedWeapon->DamageType == 3)) {
                return U->Magic + U->EquippedWeapon->StatBonus;
            }
            return U->Magic;
            
        // Checks if Armor is Physical
        case 2:
            if (U->EquippedClothing != NULL && U->EquippedClothing->DefenseType == 0) {
                return U->Defense + U->EquippedClothing->StatBonus;
            }
            return U->Defense;
            
        // Checks if Clothing is Magical
        case 3:
            if (U->EquippedClothing != NULL && U->EquippedClothing->DefenseType == 1) {
                return U->Resistance + U->EquippedClothing->StatBonus;
            }
            return U->Resistance;
            
        // Checks if Clothing is Fort
        case 4:
            if (U->EquippedClothing != NULL && U->EquippedClothing->DefenseType == 2) {
                return U->Fortitude + U->EquippedClothing->StatBonus;
            }
            return U->Fortitude;
    }
    return 0;
}

// Debug and Formatting Functions

void PrintUnitStats(WINDOW *win, Unit *U, const char **StatNames) {
    wclear(win); 
    box(win, 0, 0); 
    short int L = 0;
    short int M = -1;
    
    mvwprintw(win, 2+M, 2, "%s's Stats:", U->Name);
    mvwprintw(win, 5+M, 2, "HP    : %d/%d", U->CurrentHP, U->HP); M++;
    mvwprintw(win, 5+M, 2, "Armor : %d/%d", U->CurrentArmor, U->Armor); M++;
    mvwprintw(win, 5+M, 2, "Ward  : %d/%d", U->CurrentWard, U->Ward); M++;
    mvwprintw(win, 5+M, 2, "             "); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d (%d)", StatNames[L++], U->Attack, StatBuffs(U, 0)); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d (%d)", StatNames[L++], U->Magic, StatBuffs(U, 1)); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d (%d)", StatNames[L++], U->Fortitude, StatBuffs(U, 4)); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d (%d)", StatNames[L++], U->Defense, StatBuffs(U, 2)); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d (%d)", StatNames[L++], U->Resistance, StatBuffs(U, 3)); M++;
    mvwprintw(win, 5+M, 2, "[%-5s] %d (%d)", StatNames[L++], U->Speed, U->Speed); M++; //Placeholder
    //Printing on right side now
    L = M;
    M = -2;
    //Printing Weapon
    if (U->EquippedWeapon == NULL) {
        mvwprintw(win, 5+M, 50, "Weapon    : No Weapon"); M = M + 9;
    } else {
        mvwprintw(win, 5+M, 50, "Weapon    : %s", (U->EquippedWeapon)->Name); M++;
        switch ((U->EquippedWeapon)->Type) {
            case 0: mvwprintw(win, 5+M, 50, "If you're seeing this, I fucked up."); M++; break;
            case 1: mvwprintw(win, 5+M, 50, "Type      : Light"); M++; break;
            case 2: mvwprintw(win, 5+M, 50, "Type      : Versatile"); M++; break;
            case 3: mvwprintw(win, 5+M, 50, "Type      : Heavy"); M++; break;
            case 4: mvwprintw(win, 5+M, 50, "Type      : Breaker"); M++; break;
            case 5: mvwprintw(win, 5+M, 50, "Type      : Projectile"); M++; break;
            case 6: mvwprintw(win, 5+M, 50, "Type      : Firearm"); M++; break;
            case 7: mvwprintw(win, 5+M, 50, "Type      : Loaded"); M++; break;
        }
        //0 = Physical, 1 = Magical. 2 or 3 means the weapon is mixed and can choose wheter to do physical or magical;
        switch (U->EquippedWeapon->DamageType) {
            case 0: mvwprintw(win, 5+M, 50, "Bonus     : %+d Atk", (U->EquippedWeapon)->StatBonus); M++; break;
            case 1: mvwprintw(win, 5+M, 50, "Bonus     : %+d Mag", (U->EquippedWeapon)->StatBonus); M++; break;
            case 2: mvwprintw(win, 5+M, 50, "Bonus     : %+d Atk (Mixed)", (U->EquippedWeapon)->StatBonus); M++; break;
            case 3: mvwprintw(win, 5+M, 50, "Bonus     : %+d Mag (Mixed)", (U->EquippedWeapon)->StatBonus); M++; break;
        }
        //Only cares about ammo if type is the ranged thresold
        if ((U->EquippedWeapon)->AmmoType[0] != '\0') {
            mvwprintw(win, 5+M, 50, "Ranged Weapon"); M++;
            mvwprintw(win, 5+M, 50, "Ammo Type : %s", U->EquippedWeapon->AmmoType); M++;
            for (int i = 0; i < MAX_INVENTORY_SLOTS; i++) {
                if (U->Inventory[i] != NULL) {
                    if (strcmp((U->Inventory[i])->Name, U->EquippedWeapon->AmmoType) == 0) {
                        mvwprintw(win, 5+M, 50, "Ammo Left : %d/%d;", U->EquippedWeapon->CurrentAmmo, U->Inventory[i]->Amount); M++;
                        break;
                    } 
                    else if ((i == MAX_INVENTORY_SLOTS-1)) {
                    mvwprintw(win, 5+M, 50, "Ammo Left : %d/0", U->EquippedWeapon->CurrentAmmo); M++;
                    }
                }
                else if ((i == MAX_INVENTORY_SLOTS-1)) {
                    mvwprintw(win, 5+M, 50, "Ammo Left : %d/0", U->EquippedWeapon->CurrentAmmo); M++;
                }
            }
        } else {
            mvwprintw(win, 5+M, 50, "Melee Weapon"); M = M + 3;
        }
    }
    M++; //Line buffer
    //Printing Clothing
    if (U->EquippedClothing == NULL) {
        mvwprintw(win, 5+M, 50, "Clothing  : No Clothing"); M = M + 9;
    } else {
        mvwprintw(win, 5+M, 50, "Clothing  : %s", (U->EquippedClothing)->Name); M++;
        switch ((U->EquippedClothing)->Type) {
            //0 = No Clothing, 1 = Light, 2 = Medium, 3 = Full Plate, 4 = Ceremonial;
            case 0: mvwprintw(win, 5+M, 50, "                        "); M++; break;
            case 1: mvwprintw(win, 5+M, 50, "Type      : Light Clothing"); M++; break;
            case 2: mvwprintw(win, 5+M, 50, "Type      : Medium Clothing"); M++; break;
            case 3: mvwprintw(win, 5+M, 50, "Type      : Full Plate"); M++; break;
            case 4: mvwprintw(win, 5+M, 50, "Type      : Ceremonial"); M++; break;
            default: break;
        }
        switch (U->EquippedClothing->DefenseType) {
            case 0: mvwprintw(win, 5+M, 50, "Bonus     : %+d Def", (U->EquippedClothing)->StatBonus); M++; break;
            case 1: mvwprintw(win, 5+M, 50, "Bonus     : %+d Res", (U->EquippedClothing)->StatBonus); M++; break;
            case 2: mvwprintw(win, 5+M, 50, "Bonus     : %+d Fort", (U->EquippedClothing)->StatBonus); M++; break;
            default: break;
        }
    }
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
        
        mvwprintw(win, 18, 26, "- Press Any Key to Continue -");
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
    //Debug for weapon
    Player.EquippedWeapon = &Dummy;
    EquipUnequipClothing(&Player, &DummyPlate, 0);
    EquipUnequipClothing(&Player, &DummyPlate, 1);
    PrintUnitStats(win, &Player, StatNames);
    
    mvwprintw(win, 0, 24, "- Press Any Key to Continue -");
    wrefresh(win);
    
    wgetch(win);
    
    endwin();
    return 0;
}