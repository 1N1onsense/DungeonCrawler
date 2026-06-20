// TODO: Battle Logic
// More specifically, finish ALLY and ENEMY turn
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

//The engine will only support a maximum of 4 members of each side
typedef struct Group {
    Unit *Units[4]; //Holds pointers to the units in the group
    unsigned char FallenUnits[4]; //Will store the index of units that have been defeated;
    unsigned int MainUnit : 2; //Determines MC position if player group and Boss position if enemy group that has boss
    unsigned int AmountUnits : 3; //How many units this group has
    unsigned int Type : 2; //0 = Player, 1 = Enemy, 2 = Enemy with boss, 3 = Special Enemy;
    unsigned int NumFallenUnits : 2; //Dont know if I'll use this, but it makes a perfect byte with bitfields!
} Group;

typedef enum {
    OFFHAND_NONE = 0,
    OFFHAND_WEAPON,
    OFFHAND_SHIELD
} OffhandType;

typedef struct Weapon {
    char Name[18]; 
    unsigned short int Type : 3; //0 = Unused for Now, 1 = Light, 2 = Versatile, 3 = Heavy, 4 = Breaker, 5 = Projectile, 6 = Firearm, 7 = Loaded;
    unsigned int DamageType : 2; //0 = Physical, 1 = Magical. 2 or 3 means the weapon is mixed and can choose wheter to do physical or magical;
    char AmmoType[18]; //If the weapon type is 5 or above (consumes ammo), this is the name of the type of ammo it consumes, which will be used for comparisons;
    short int StatBonus; //As long as it is not offhand, grants a bonus to Atk (if damagetype is 0) or Mag (if Damagetype is 1);
    //If mixed, tries to assign equally, but gives priority to granting the bigger part to Atk if 2 or Mag if 3;
    short int AmmoCapacity; //Determines how many shots the weapon can give before reloading if ranged
    short int CurrentAmmo;//Determines how much ammo the weapon currently has loaded
    void (*SpecialEffect) (Group*, Group*, Unit*, Unit*, int*);
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
    void (*SpecialEffect) (Group*, Group*, Unit*, Unit*, int*);
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
    void (*Effect) (Group*, Group*, Unit*, Unit*, int*);
} Item;

typedef struct Clothing {
    char Name[18];
    unsigned int Type : 3; //0 = No Clothing, 1 = Light, 2 = Medium, 3 = Heavy, 4 = Ceremonial;
    short int StatBonus; //Like the weapon bonus
    unsigned int DefenseType : 2; //Determines if the statbonus will go to 0 = Def, 1 = Res, 2 = Fort.
    void (*SpecialEffect) (Group*, Group*, Unit*, Unit*, int*);
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
    void (*Passive) (Group*, Group*, Unit*, Unit*, int*);
    unsigned char IsAlly;
    short int StatusEffect;
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
    if (U->CurrentArmor > U->Armor) U->CurrentArmor = U->Armor;
    if (U->CurrentWard > U->Ward) U->CurrentWard = U->Ward;
}

Unit CreateUnit(char *Name,
short int Attack,short int Magic,short int Fortitude,short int Defense,
short int Resistance,short int Speed, Weapon *InitWeapon, unsigned char IsAlly, void (*Passive)(Group*, Group*, Unit*, Unit*, int*)) {
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
    NewUnit.IsAlly = IsAlly;
    NewUnit.StatusEffect = 0;
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

    //IMPORTANT: Main Battle Function!

void InitiativeSort(Group* Allies, Group *Enemies, Unit **Initiative) {
    unsigned char InitCount = 0;
    for (int i = 0; i < Allies->AmountUnits; i++) {
        //CurrentHP > 0 is alive condition. 
        if (Allies->Units[i] != NULL && Allies->Units[i]->CurrentHP > 0) { 
            Initiative[InitCount++] = Allies->Units[i];
        }
    }
    for (int i = 0; i < Enemies->AmountUnits; i++) {
        if (Enemies->Units[i] != NULL && Enemies->Units[i]->CurrentHP > 0) {
            Initiative[InitCount++] = Enemies->Units[i];
        }
    }

    //Sort by Speed (Descending) using Insertion Sort
    for (int i = 1; i < InitCount; i++) {
        Unit *Key = Initiative[i];
        int j = i - 1;

        //Shift units with lower speed down the array to make room for Key
        while (j >= 0 && Initiative[j]->Speed < Key->Speed) {
            Initiative[j + 1] = Initiative[j];
            j = j - 1;
        }
        Initiative[j + 1] = Key;
    }
}

unsigned char ApplyDamage(Unit* User, short int IdxTargets[4], Group *UserGroup, Group *TargetGroup, unsigned char BlowType, WINDOW *win) {
    int BaseDamage = 0;

    unsigned char IsBreaker = 0;
    if (User->EquippedWeapon != NULL && User->EquippedWeapon->Type == 4) {
        IsBreaker = 1;
    }

    // (0 = Physical, 1 = Magical)
    if (BlowType == 0) {
        BaseDamage = StatBuffs(User, 0); 
    } else {
        BaseDamage = StatBuffs(User, 1); 
    }

    int ValidTargetCount = 0;
    Unit *OnlyTarget = NULL;
    for (int i = 0; i < 4; i++) {
        if (IdxTargets[i] >= 0 && IdxTargets[i] < TargetGroup->AmountUnits) {
            Unit *T = TargetGroup->Units[IdxTargets[i]];
            if (T != NULL && T->CurrentHP > 0) {
                ValidTargetCount++;
                OnlyTarget = T; // Will hold the target if it ends up being just one
            }
        }
    }

    //If not AoE and if faster. Will add more conditions later when we can debug fights
    unsigned char IsDoubleHit = 0;
    if (ValidTargetCount == 1 && OnlyTarget != NULL) {
        if (User->Speed > OnlyTarget->Speed) {
            IsDoubleHit = 1;
        }
    }

    // Start printing messages at Row 4
    int PrintRow = 4; 

    //Cleaning message area
    for(int r = 4; r < 8; r++) {
        mvwprintw(win, r, 23, "                                ");
    }

    //DOUBLE STRIKE!
    if (IsDoubleHit) {
        mvwprintw(win, PrintRow++, 24, "%s is fast! Double strike!", User->Name);
    }

    for (int i = 0; i < 4; i++) {
        if (IdxTargets[i] < 0 || IdxTargets[i] >= TargetGroup->AmountUnits) continue;
        
        Unit *Target = TargetGroup->Units[IdxTargets[i]];
        if (Target == NULL || Target->CurrentHP <= 0) continue; 

        // Set the amount of times the damage loop will run for this target
        int TargetHits = IsDoubleHit ? 2 : 1;

        for (int hit = 0; hit < TargetHits; hit++) {
            //Because this piece of shit was hitting a dead unit on the second strike
            if (Target->CurrentHP <= 0) break;
            int Spillover = 0;
            unsigned char BrokeArmor = 0;
            unsigned char BrokeWard = 0;

            // Apply Physical Damage
            if (BlowType == 0) { 
                if (Target->CurrentArmor > 0) {
                    Target->CurrentArmor -= BaseDamage;
                    if (Target->CurrentArmor <= 0) {
                        Spillover = -(Target->CurrentArmor);
                        Target->CurrentArmor = 0;
                        BrokeArmor = 1;
                        
                        if (IsBreaker && Spillover > 0) {
                            Target->CurrentHP -= Spillover;
                        }
                    }
                } else {
                    Target->CurrentHP -= BaseDamage;
                }
            } 
            // Apply Magical Damage
            else { 
                if (Target->CurrentWard > 0) {
                    Target->CurrentWard -= BaseDamage;
                    if (Target->CurrentWard <= 0) {
                        Spillover = -(Target->CurrentWard);
                        Target->CurrentWard = 0;
                        BrokeWard = 1;
                        
                        if (IsBreaker && Spillover > 0) {
                            Target->CurrentHP -= Spillover;
                        }
                    }
                } else {
                    Target->CurrentHP -= BaseDamage;
                }
            }

            if (Target->CurrentHP < 0) Target->CurrentHP = 0;

            mvwprintw(win, PrintRow, 23, "                                "); 

            if (BrokeArmor) {
                mvwprintw(win, PrintRow, 24, "%s hit %s! ARMOR BREAK!", User->Name, Target->Name);
            } else if (BrokeWard) {
                mvwprintw(win, PrintRow, 24, "%s hit %s! WARD BREAK!", User->Name, Target->Name);
            } else {
                mvwprintw(win, PrintRow++, 24, "%s hit %s", User->Name, Target->Name);
                mvwprintw(win, PrintRow, 24, "%d Damage!", BaseDamage);
            }
            
            PrintRow++;
        }
    }

    wrefresh(win);
    napms(1500); //Arbitrary 1.5 seconds wait
    for(int r = 4; r < 8; r++) {
        mvwprintw(win, r, 23, "                                ");
    }
    wrefresh(win);

    return 0;
}

 //Functions to draw battle UI
void DrawBattlePanels(WINDOW *win, Group *Allies, Group *Enemies, Unit *ActiveUnit) {
    //Redraw the outer boundary box (ensures it stays intact)
    box(win, 0, 0);
    
    //Draws vertical dividers to separate the 3 columns
    //Left panel ends at X=22, Center panel ends at X=55
    for (int y = 1; y < 23; y++) {
        mvwaddch(win, y, 22, '|');
        mvwaddch(win, y, 55, '|');
    }

    //Loop through Allies (Left Panel - Text starts at X = 2)
    int YOffset = 2; // Matches row 3 on your blueprint layout
    for (int i = 0; i < 4; i++) {
        //Only draws if unit slot exists in group
        if (i < Allies->AmountUnits && Allies->Units[i] != NULL) {
            Unit *U = Allies->Units[i];
            //Highlights if it's the unit's turn
            if (U == ActiveUnit) wattron(win, COLOR_PAIR(1) | A_BOLD);
            
            if (U->CurrentHP <= 0) {
                mvwprintw(win, YOffset,     2, "%-20s", U->Name);
                mvwprintw(win, YOffset + 1, 2, "%-20s", "*FALLEN*");
                mvwprintw(win, YOffset + 2, 2, "%-20s", ""); 
                mvwprintw(win, YOffset + 3, 2, "%-20s", "");
            } else {
                mvwprintw(win, YOffset,     2, "%-20s", U->Name);
                mvwprintw(win, YOffset + 1, 2, "HP    : %3d/%-3d", U->CurrentHP, U->HP);
                mvwprintw(win, YOffset + 2, 2, "Armor : %3d/%-3d", U->CurrentArmor, U->Armor);
                mvwprintw(win, YOffset + 3, 2, "Ward  : %3d/%-3d", U->CurrentWard, U->Ward);
            }
            
            if (U == ActiveUnit) wattroff(win, COLOR_PAIR(1) | A_BOLD);
        } else {
            //Blank out unused party slots
            for (int r = 0; r < 4; r++) mvwprintw(win, YOffset + r, 2, "%-20s", "");
        }
        YOffset += 5; //move down 4 lines of stats + 1 line of padding space
    }

    //Loop through Enemies (Right Panel - Text starts at X = 57)
    YOffset = 2; 
    for (int i = 0; i < 4; i++) {
        if (i < Enemies->AmountUnits && Enemies->Units[i] != NULL) {
            Unit *E = Enemies->Units[i];
            if (E == ActiveUnit) wattron(win, COLOR_PAIR(1) | A_BOLD);
            
            if (E->CurrentHP <= 0) {
                mvwprintw(win, YOffset,     57, "%-20s", E->Name);
                mvwprintw(win, YOffset + 1, 57, "%-20s", "*DEFEATED*");
                mvwprintw(win, YOffset + 2, 57, "%-20s", "");
                mvwprintw(win, YOffset + 3, 57, "%-20s", "");
            } else {
                mvwprintw(win, YOffset,     57, "%-20s", E->Name);
                mvwprintw(win, YOffset + 1, 57, "HP    : %3d/%-3d", E->CurrentHP, E->HP);
                mvwprintw(win, YOffset + 2, 57, "Armor : %3d/%-3d", E->CurrentArmor, E->Armor);
                mvwprintw(win, YOffset + 3, 57, "Ward  : %3d/%-3d", E->CurrentWard, E->Ward);
            }

            if (E == ActiveUnit) wattroff(win, COLOR_PAIR(1) | A_BOLD);
        } else {
            for (int r = 0; r < 4; r++) mvwprintw(win, YOffset + r, 57, "%-20s", "");
        }
        YOffset += 5;
    }
    wrefresh(win);
}

#define NUM_OPTIONS 6
void DrawCenterMenu(WINDOW *win, unsigned char Cursor) {
    const char *Options[] = {"STRIKE", "BRACE", "SKILL", "ITEM", "STATS", "SPECIAL"};
    int StartRow = 8;
    
    for (int i = 0; i < NUM_OPTIONS; i++) {
        if (i == Cursor) {
            mvwprintw(win, StartRow + i, 24, "---> [%-12s]", Options[i]);
        } else {
            mvwprintw(win, StartRow + i, 24, "     [%-12s]", Options[i]);
        }
    }
    wrefresh(win);
}

void SelectTargetUI(WINDOW *win, Group *TargetGroup, int StrikeType, short int ResultTargets[4]) {
    int Cursor = 0;
    unsigned char ActionChosen = 0;
    
    // Determine maximum possible targets (Primary + Extra AoE targets)
    int MaxTargets = 1 + StrikeType; 
    if (MaxTargets > 4) MaxTargets = 4;

    // 1. Initialize all output results to -1 (Empty)
    for (int i = 0; i < 4; i++) {
        ResultTargets[i] = -1;
    }

    // 2. Find the first valid (alive) unit to place the cursor initially
    while (Cursor < 4 && (TargetGroup->Units[Cursor] == NULL || TargetGroup->Units[Cursor]->CurrentHP <= 0)) {
        Cursor++;
    }
    
    // Safety check: If no targets are alive, exit immediately
    if (Cursor == 4) return; 

    while (!ActionChosen) {
        // Temporary array to hold the indices of units that will be hit this frame
        int HitIndices[4] = {-1, -1, -1, -1};
        int HitCount = 0;
        
        // Assign the primary target the user is hovering over
        HitIndices[HitCount++] = Cursor;

        // If it's an AoE (StrikeType > 0), grab the next available alive units in the group
        if (StrikeType > 0) {
            for (int i = 0; i < 4 && HitCount < MaxTargets; i++) {
                // Skip the primary target, empty slots, and fallen units
                if (i != Cursor && TargetGroup->Units[i] != NULL && TargetGroup->Units[i]->CurrentHP > 0) {
                    HitIndices[HitCount++] = i;
                }
            }
        }

        for (int r = 8; r < 16; r++) {
            mvwprintw(win, r, 23, "                                ");
        }

        mvwprintw(win, 8, 24, "SELECT TARGET:");
        
        // Print the units that will be hit by this attack
        for (int i = 0; i < HitCount; i++) {
            if (i == 0) {
                // Primary target gets the cursor arrow
                mvwprintw(win, 10 + i, 24, "---> [%-12s]", TargetGroup->Units[HitIndices[i]]->Name);
            } else {
                // Splash targets get a plus indicator
                mvwprintw(win, 10 + i, 24, "  +  [%-12s]", TargetGroup->Units[HitIndices[i]]->Name);
            }
        }
        
        mvwprintw(win, 15, 23, "ENTER: Hit | BACKSPACE: Back");
        wrefresh(win);

        // Inputs
        int ch = wgetch(win);
        switch (ch) {
            case 's': case 'S': case KEY_DOWN:
                // Move cursor to the NEXT alive unit
                do {
                    Cursor++;
                    if (Cursor > 3) Cursor = 0; // Wrap around to the top
                } while (TargetGroup->Units[Cursor] == NULL || TargetGroup->Units[Cursor]->CurrentHP <= 0);
                break;

            case 'w': case 'W': case KEY_UP:
                // Move cursor to the PREVIOUS alive unit
                do {
                    Cursor--;
                    if (Cursor < 0) Cursor = 3; // Wrap around to the bottom
                } while (TargetGroup->Units[Cursor] == NULL || TargetGroup->Units[Cursor]->CurrentHP <= 0);
                break;

            case 'd': case 'D': case KEY_ENTER: case '\n': 
            case 13:
                // Confirm selection: Lock in the HitIndices into the ResultTargets array
                for (int i = 0; i < 4; i++) {
                    ResultTargets[i] = HitIndices[i];
                }
                ActionChosen = 1;
                break;

            case 'a': case 'A': case KEY_LEFT: case KEY_BACKSPACE: case 8: case 127:
                // Cancel selection: ActionChosen triggers the exit, ResultTargets remains -1
                ActionChosen = 1; 
                break;
        }
    }
    
    for (int r = 8; r < 16; r++) {
        mvwprintw(win, r, 23, "                                ");
    }
    wrefresh(win);
}

void AllyTurn(Group *Allies, Group *Enemies, Unit *CurrentUnit, WINDOW *win) {
    unsigned char TurnComplete = 0;

    while (!TurnComplete) {
        DrawBattlePanels(win, Allies, Enemies, CurrentUnit);
        unsigned char Cursor = 0;
        unsigned char ActionChosen = 0;

        while (!ActionChosen) {
            DrawCenterMenu(win, Cursor);
            int ch = wgetch(win);

            switch (ch) {
                case 's':
                case 'S':
                case KEY_DOWN:
                    Cursor++; if (Cursor >= NUM_OPTIONS) Cursor = 0; break;

                case 'w':
                case 'W':
                case KEY_UP:
                    if (Cursor == 0) Cursor = NUM_OPTIONS - 1; 
                    else Cursor--; break;

                case 'a':
                case 'A':
                case KEY_LEFT:
                case KEY_BACKSPACE:
                    /*Returns to previous menu if there is one*/ break;
                
                case 'd':
                case 'D':
                case KEY_ENTER:
                    ActionChosen = 1; break;
            }
        }

        short int ResultTargets[4] = {-1, -1, -1, -1};
            
        switch (Cursor) {
            case 0: // Strike Action
                short int BlowType = CurrentUnit->EquippedWeapon->DamageType;
                
                // TODO: Implement the Mixed Damage UI selection here
                if (BlowType == 2 || BlowType == 3) {
                    BlowType = 0; // Temporary default to prevent passing 2/3 to ApplyDamage
                }

                SelectTargetUI(win, Enemies, 0, ResultTargets);
                
                // Check if the user cancelled
                if (ResultTargets[0] == -1) {
                    ActionChosen = 0; // Resets the menu choice, goes back to start of TurnComplete loop
                } else {
                    ApplyDamage(CurrentUnit, ResultTargets, Allies, Enemies, BlowType, win);
                    TurnComplete = 1; // Ends the turn safely
                }
                break;

            case 1: break; //Brace Action, will not be made for now as focus is on testing battle UI.
            case 2: break; //Skills... This in particular will take so much work, I'm almost starting to regret doing this
            case 3: break; //Item Action
            case 4: break; //Print Stats Action, this will basically just PrintUnitStats, then return to selection menu once a key is pressed
            case 5: break; //Special Actions
        }
    }

};

void EnemyTurn(Group *Allies, Group *Enemies,  Unit *CurrentUnit, WINDOW *win){
    // Clean row 4
    mvwprintw(win, 4, 23, "                                ");
    // Print debug message
    mvwprintw(win, 4, 24, "%s skips turn...", CurrentUnit->Name);
    wrefresh(win);
    // Arbitrary 2 second delay
    napms(2000); 
    // Clean up
    mvwprintw(win, 4, 23, "                                ");
    wrefresh(win);
}

unsigned char Battle(Group* Allies, Group *Enemies, WINDOW *win) {
    int CurrentRound = 0;
    Unit *Initiative[8];
    unsigned char AliveAllies = Allies->AmountUnits - Allies->NumFallenUnits;
    unsigned char AliveEnemies = Enemies->AmountUnits - Enemies->NumFallenUnits; 
    unsigned char TotalUnits = AliveAllies + AliveEnemies;
    InitiativeSort(Allies, Enemies, Initiative);

    // Main Battle Loop
    while (AliveAllies > 0 && AliveEnemies > 0) {
        CurrentRound++;
        // Turn Start
        for (unsigned char i = 0; i < TotalUnits; i++) {
            Unit *CurrentUnit = Initiative[i];
            //Only acts if it's alive
            if (CurrentUnit->CurrentHP > 0) {
                //Processes unit passive first and foremost
                if (CurrentUnit->Passive != NULL) CurrentUnit->Passive(Allies, Enemies, CurrentUnit, NULL, &CurrentRound);
                if (CurrentUnit->IsAlly) {
                    AllyTurn(Allies, Enemies, CurrentUnit, win);
                } else {
                    EnemyTurn(Allies, Enemies, CurrentUnit, win);
                }
            }
        }
        //Refreshing allocation of units alive
        AliveAllies = 0;
        AliveEnemies = 0;

        for (int i = 0; i < Allies->AmountUnits; i++) {
            if (Allies->Units[i] != NULL && Allies->Units[i]->CurrentHP > 0) AliveAllies++;
        }
        for (int i = 0; i < Enemies->AmountUnits; i++) {
            if (Enemies->Units[i] != NULL && Enemies->Units[i]->CurrentHP > 0) AliveEnemies++;
        }

        TotalUnits = AliveAllies + AliveEnemies;
        InitiativeSort(Allies, Enemies, Initiative);
    }
    
    if (AliveAllies > 0) return 0;
    else return 1;
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
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK); 
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
        mvwprintw(win, 4, 2, "                               "); // Cleans the erorr message
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
    Unit Player = CreateUnit(Name, Stats[0], Stats[1], Stats[2], Stats[3], Stats[4], Stats[5], NULL, 1, NULL);
    //Debug for weapon
    Player.EquippedWeapon = &Dummy;
    EquipUnequipClothing(&Player, &DummyPlate, 0);
    EquipUnequipClothing(&Player, &DummyPlate, 1);
    PrintUnitStats(win, &Player, StatNames);
    
    mvwprintw(win, 0, 24, "- Press Any Key to Start Battle -");
    wrefresh(win);
    wgetch(win);

//TODO: BELOW HERE IS ALL DEBUG, DELETE LATER!!!

    Unit BadGuy = CreateUnit("Shali", 10, 0, 5, 5, 5, 2, NULL, 0, NULL);
    BadGuy.EquippedWeapon = &Dummy;

    Group PlayerParty;
    PlayerParty.AmountUnits = 1;
    PlayerParty.NumFallenUnits = 0;
    PlayerParty.Type = 0;
    PlayerParty.Units[0] = &Player; // Pass the memory address of the player

    Group EnemyParty;
    EnemyParty.AmountUnits = 1;
    EnemyParty.NumFallenUnits = 0;
    EnemyParty.Type = 1;
    EnemyParty.Units[0] = &BadGuy; // Pass the memory address of the enemy

    FormattedCleanWindow(win);
    unsigned char BattleResult = Battle(&PlayerParty, &EnemyParty, win);

    FormattedCleanWindow(win);
    if (BattleResult == 0) {
        mvwprintw(win, 11, 35, "YOU WIN!");
    } else {
        mvwprintw(win, 11, 35, "YOU SCRUB...");
    }
    wrefresh(win);
    wgetch(win);

    endwin();
    return 0;
}