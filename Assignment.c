#include <stdint.h>
#include <stdio.h>
#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <stdlib.h>
#include <graphics.h>
#include <macros.h>
#include "lcd_model.h"
#include <lcd.h>
#include <sprite.h>
#include <string.h>
#include <math.h>
#include "cab202_adc.h"
#include "usb_serial.h"

/*
    Global variable Declarations
*/
int statcounterINT[] = {10,00,00,0000,0,30}; //[0] - Lives, [1] - Minutes, [2] - Seconds, [3] - Time Counter, [4] - Score, [5] - Total number of blocks
int mvTreasure = 1;
int treasureDir = 1;
int jumpregister = 0;
int death = 0;
int placefood = 0;
int usedfood = 0;
int remainfood = 5;
int toSpawnZombie = 1;
int zombieRemain = 0;
int runStdKey = 0;
Sprite zombieArr[5];
int zombieArrMove[5];
volatile int overflow_counter = 0;
volatile int overflow_counter2 = 0;
#define FREQ     (8000000.0)
#define PRESCALE (1024.0)
#define BIT(x) (1 << (x))
#define OVERFLOW_TOP (1023)
#define ADC_MAX (1023)
char txtbuffer[40];
Sprite blocks[30];
Sprite food[5];
unsigned char blockBlankSpr[] = {0b00000000};
unsigned char blockSafeSpr[] = {0b11111111, 0b11000000,
                                0b11111111, 0b11000000,
                                0b11111111, 0b11000000};
unsigned char blockUnsafeSpr[] = {0b11111111, 0b11000000,
                                0b01000000, 0b00000000,
                                0b11111111, 0b11000000};
unsigned char bm[] = {
        0b11110000, 0b11110000, 0b01100000, 0b10010000};
unsigned char chest[] = {0b11000000,0b11000000};
unsigned char foodSpr[] =  {0b10100000,0b01000000,0b10000000};
unsigned char zombie[] =   {0b11100000, 0b01000000, 0b11100000};
/*
    Initialise blank sprites used for comparing checking which block the player hit.
*/
Sprite safeblockdummy;
Sprite unsafeblockdummy;
Sprite noblockdummy;
void dummyblock() {
    
    sprite_init(&safeblockdummy,0,0,10,3,blockSafeSpr);
    sprite_init(&unsafeblockdummy,0,0,10,3,blockUnsafeSpr);
    sprite_init(&noblockdummy,0,0,10,3,blockBlankSpr);
}
/*
    Used to send information over Serial.
    Taken from the Week 8 AMS questions.
*/
void serialSend(char * buffer, int buffer_size, const char * format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, buffer_size, format, args);
	usb_serial_write((uint8_t *) buffer, strlen(buffer));
}
/*
    Initialise the joystick and button controls for use in the game. Also enables the LEDs.
*/
void initController(void) {
    DDRB = DDRB & ~(1<<0); //CENTRE
    DDRB = DDRB & ~(1<<1); //LEFT
    DDRB = DDRB & ~(1<<7); //DOWN
    DDRD = DDRD & ~(1<<0); //RIGHT
    DDRD = DDRD & ~(1<<1); //UP
    DDRF = DDRF & ~(1<<6); //SW2
    DDRF = DDRF & ~(1<<5); //SW3
    DDRB |= (1<<2); //Left LED
    DDRB |= (1<<3); //Right LED
}
/*
    Displays the introduction message with my name and student #. Waits for SW0 or the 's' key to continue.
*/
void introMessage(void) {
    TIMSK1 = 0;
    clear_screen();
    draw_string(0,0,"REDACTED",FG_COLOUR);
    draw_string(0,20,"REDACTED",FG_COLOUR);
    show_screen();
    while (1) {
        int key  = usb_serial_getchar();
        if ((PINF >> 6) & 1 || key == 's') {
            clear_screen();
            show_screen();
            TIMSK1 = 1;
            break;
        }
    }
}
/*
    Uses a random number to determine which type of block should be placed, creates a sprite for it and writes it to the array.
    Input: Block Row, Block Column
*/
Sprite setup_block(int col, int row) {
    int bx = (col * 10)-10+(col * 3);
    int by = (row * 4)+(5* row);
    if (col == 1) {
        bx = (col * 10)-10;
    }
    
    
    int random = rand() % 3;
    if (bx == 0) {
        random = 0;
    }
    if (col == 1 && row == 1) {
        Sprite newblock;
        bx = (col * 10)-10;
        sprite_init(&newblock,bx,by,10,3,blockSafeSpr);
        return newblock;
    }
    else {
        if (random == 2) {
            Sprite newblock;
            sprite_init(&newblock,bx+3,by,1,1,blockBlankSpr);
            return newblock;
        }
        else if (random == 1) {
            Sprite newblock;
            sprite_init(&newblock,bx+3,by,10,3,blockUnsafeSpr);
            return newblock;
        }
        else {
            Sprite newblock;
            sprite_init(&newblock,bx+3,by,10,3,blockSafeSpr);
            return newblock;
        }
    }
    
}
/*
    Loops through the maximum number of blocks and calls the function responsible for creating the sprite array with a given row and column.
*/
void init_blocks() {
    int colC = 1;
    int rowC = 1;
    for (int i=0; i < statcounterINT[5]; i++) {
        blocks[i] = setup_block(colC, rowC);
        if (colC >= 6) {
            colC = 1;
            rowC++;
        } else {
            colC++;
        }

    }
}
/*
    Iterate over the array of block sprites and tell the display to draw them, used to redraw the screen on every loop.
*/
void blockdraw() {
    for (int c = 0; c < statcounterINT[5]; c++) {
        sprite_draw(&blocks[c]); //Loop through maximum elements in Block array and call draw function
    }
}
/*
    Initialises the sprite for the treasure.
*/
Sprite treasure;
void init_treasure() {
    sprite_init(&treasure,10,40,2,2,chest);
}
/*
    Moves the treasure sprite from one side of the screen to the other and turns it around if it is about to leave the screen.
*/
void movetreasure() {
    if (*&treasure.x < 1) {
        *&treasureDir = 1;
    } else if (*&treasure.x > 83) {
        *&treasureDir = 0;
    }
    if (mvTreasure) {
        if (*&treasureDir == 1) {
            *&treasure.x = *&treasure.x + 1;
        } else if (*&treasureDir == 0) {
            *&treasure.x = *&treasure.x - 1;
        }
    }
    
}
/*
    Displays the pause menu with information about the current game.
    Pauses the overflow on TIMER1 (responsible for increasing the clock) so the clock doesn't increase while in the menu.
*/
void pauseMenu(void) {
    TIMSK1 = 0;
    serialSend(txtbuffer, sizeof(txtbuffer), "PAUSE:SC=%d,L=%d,T=%d:%d,Z=%d,F=%d",statcounterINT[4],statcounterINT[0],statcounterINT[1],statcounterINT[2],zombieRemain,remainfood);
    clear_screen();
    draw_string(0,0,"Lives: ", FG_COLOUR);
    draw_string(0,10,"Score: ",FG_COLOUR);
    draw_string(0,20,"Time:   :",FG_COLOUR);
    draw_string(0,30,"Zombies:",FG_COLOUR);
    draw_string(0,40,"Food:",FG_COLOUR);
    char strLiveCache[3];
    sprintf(strLiveCache, "%d", statcounterINT[0]);
    draw_string(30,0,strLiveCache,FG_COLOUR);
    char strScoreCache[5];
    sprintf(strScoreCache, "%d", statcounterINT[4]);
    draw_string(30,10,strScoreCache,FG_COLOUR);
    char strMinCache[2];
    sprintf(strMinCache, "%d", statcounterINT[1]);
    draw_string(28,20,strMinCache,FG_COLOUR);
    char strSecCache[2];
    sprintf(strSecCache, "%d", statcounterINT[2]);
    draw_string(45,20,strSecCache,FG_COLOUR);

    char strZombieCache[1];
    sprintf(strZombieCache, "%d", zombieRemain);
    draw_string(40,30,strZombieCache,FG_COLOUR);

    char strFoodCache[1];
    sprintf(strFoodCache, "%d", remainfood);
    draw_string(30,40,strFoodCache,FG_COLOUR);
    show_screen();
    while (1) {
        int key  = usb_serial_getchar();
        if ((PINB >> 0) & 1 || key == 'p') {
            TIMSK1 = 1;
            break;
        }
    }

}
/*
    Checks if two given sprites have matching X or Y values which could indicate a collision and returns 1 if there is one or 0 otherwise.
    Input: Sprite 1, Sprite 2
*/
int spritecollide(Sprite sp1, Sprite sp2) {
    int x1 = round(sp1.x);
    int x2 = round(sp2.x);
    int y1 = round(sp1.y);
    int y2 = round(sp2.y);
    int w1 = x1 + sp1.width - 1;
    int w2 = x2 + sp2.width - 1;
    int h1 = y1 + sp1.height - 1;
    int h2 = y2 + sp2.height - 1;  
    if (x1 > w2) {return 0;}
    else if (x2 > w1) {return 0;}
    else if (y1 > h2) {return 0;}
    else if (y2 > h1) {return 0;}
    return 1;

}
/*
    Initialises the player character and draws them at a given position.
*/
Sprite player;
void spawnPlayerChar() {
    sprite_init(&player, 4, 4, 4, 4, bm);
    sprite_draw(&player);
    serialSend(txtbuffer, sizeof(txtbuffer), "START:X=%d,Y=%d",4,4);
}
/*
    Checks if the appropriate key has been pressed or the required character was sent over serial and performs the appropriate command to manipulate the player.
*/
void keypress(void) {
    int key  = usb_serial_getchar();
    if ((PIND >> 1) & 1 || key == 'w') { //UP
    *&jumpregister = 1;

    } else if ((PINB >> 7) & 1 || key == 's') { //DOWN
    *&placefood = 1;

    } else if ((PINB >> 1) & 1 || key == 'a') { //LEFT
        *&player.x = *&player.x -1;
    } else if ((PIND >> 0) & 1 || key == 'd') { //RIGHT
        *&player.x = *&player.x +1;
    } else if ((PINB >> 0) & 1 || key == 'p') { //CENTRE
        pauseMenu();   
    } else if (((PINF >>5) & 1 || key == 't')) {
        if (*&mvTreasure == 1) {
            *&mvTreasure = 0;
        } else {
            *&mvTreasure = 1;
        }
    }
}
/*
    Sets the screen backlight to a given value using the appropriate registers.
    Input: Desired backlight level.
*/
void set_backlight(int val) {
    TC4H = val >> 8;
    OCR4A = val & 0xff;
}
/*
    Either increases or decreases the player's lives by a given amount.
    Input: "i" or "d" depending on an increase or decrease, amount to increase/decrease by.
*/
void lifechange(char* method, int amount) {
    if (strcmp(method,"i") == 0) {
        *&statcounterINT[0] = *&statcounterINT[0] + amount;
    }
    else if (strcmp(method,"d") == 0) {
        *&statcounterINT[0] = *&statcounterINT[0] - amount;
    }
}
/*
    Sets the player's X and Y position back to the safe spawn point.
*/
void respawnChar() {
    *&player.x = 4;
    *&player.y = 4;
    serialSend(txtbuffer, sizeof(txtbuffer), "RESPAWN:X=%d,Y=%d",4,4);
}
/*
    Sets the LCD contrast to a desired level. Ignores the input if it would take the screen below the minimum contrast.
    Input: Desired contrast level.
*/
void setContrast(int num) {
    if (num >= LCD_LOW_CONTRAST) {
        
    }
    LCD_CMD( lcd_set_function, lcd_instr_extended);
	    LCD_CMD( lcd_set_contrast, num);
	    LCD_CMD( lcd_set_function, lcd_instr_basic);
    
}
/*
    Decreases both the LCD backlight and contrast over time. Used in the standard death animation for the player.
*/
void dropScreen() {
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 10);
    set_backlight(ADC_MAX - (long)170);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 20);
    set_backlight(ADC_MAX - (long)340);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 30);
    set_backlight(ADC_MAX - (long)510);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 40);
    set_backlight(ADC_MAX - (long)680);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 50);
    set_backlight(ADC_MAX - (long)850);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 60);
    set_backlight(ADC_MAX - (long)1023);
}
/*
    Increases the LCD backlight and contrast over time back to their default values. Used in the standard death animation for the player.
*/
void raiseScreen() {
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 60);
    set_backlight(ADC_MAX - (long)1023);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 50);
    set_backlight(ADC_MAX - (long)850);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 40);
    set_backlight(ADC_MAX - (long)680);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 30);
    set_backlight(ADC_MAX - (long)510);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 20);
    set_backlight(ADC_MAX - (long)340);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 10);
    set_backlight(ADC_MAX - (long)170);
    _delay_ms(200);
    setContrast(LCD_DEFAULT_CONTRAST - 0);
    set_backlight(ADC_MAX);
}
/*
    Decrements the players lives by 1 and determines whether the default death animation should be player or not. Checks if the lives are above 0 and respawns the player.
    Input: Integer representing the reason for the death.
*/
void killplayer(int reason) {
    lifechange("d",1);
    if (reason == 1) {
        serialSend(txtbuffer, sizeof(txtbuffer), "DEATH:R=Block,L=%d,SC=%d,T=%d:%d", statcounterINT[0],statcounterINT[4],statcounterINT[1],statcounterINT[2]);
    } else if (reason == 2) {
        serialSend(txtbuffer, sizeof(txtbuffer), "DEATH:R=Edge,L=%d,SC=%d,T=%d:%d", statcounterINT[0],statcounterINT[4],statcounterINT[1],statcounterINT[2]);
    } else if (reason == 3) {
        serialSend(txtbuffer, sizeof(txtbuffer), "DEATH:R=Zombie,L=%d,SC=%d,T=%d:%d", statcounterINT[0],statcounterINT[4],statcounterINT[1],statcounterINT[2]);
    }
    
    if (statcounterINT[0] > 0) {dropScreen();}
    if (statcounterINT[0]> 0) {
        respawnChar();
    }
    
    _delay_ms(600);
    if (statcounterINT[0] > 0) {raiseScreen();}
}
/*
    Checks if the player left the boundaries of the display and kills them if they have.
*/
void playerEdgeCheck() {
    if (player.x < -4 || player.x > 88  || player.y > 49 || player.y < -4) {
        killplayer(2);
    }
}
/*
    Checks if the player ran out of lives, displays the final death animation and shows the gameover score results.
    Pressing SW0 displays my student ID and does nothing else and SW1 resets the game.
*/
void gameOver() {
    if (death == 1) {
        TIMSK1 = 0;
        TIMSK3 = 0;
        PORTB = PORTB & ~(1<<2);
        PORTB = PORTB & ~(1<<3);
        serialSend(txtbuffer, sizeof(txtbuffer), "GAMEOVER:SC=%d,L=%d,T=%d:%d,Z=N",statcounterINT[4],statcounterINT[0],statcounterINT[1],statcounterINT[2]);
        LCD_CMD(lcd_set_display_mode,lcd_display_inverse);
        uint8_t x = player.x;
        uint8_t y = player.y;
        *&player.y = *&player.y - 2;
        LCD_CMD(lcd_set_x_addr, x);
        LCD_CMD(lcd_set_y_addr, y / 8);
        lcd_write(1,0xFF);
        _delay_ms(200);
        LCD_CMD(lcd_set_x_addr, (x+1));
        lcd_write(1,0xFF);
        _delay_ms(200);
        LCD_CMD(lcd_set_x_addr, (x+2));
        lcd_write(1,0xFF);
        _delay_ms(200);
        LCD_CMD(lcd_set_x_addr, (x+3));
        lcd_write(1,0xFF);
        _delay_ms(200);
        _delay_ms(1000);
        LCD_CMD(lcd_set_display_mode,lcd_display_normal);
    }
    while (death == 1) {
        clear_screen();
        draw_string(0,10,"Game Over!", FG_COLOUR);
        draw_string(0,20,"Score: ",FG_COLOUR);
        draw_string(0,30,"Time:   :",FG_COLOUR);
        char strScoreCache[5];
        sprintf(strScoreCache, "%d", statcounterINT[4]);
        char strMinCache[2];
        sprintf(strMinCache, "%d", statcounterINT[1]);
        char strSecCache[2];
        sprintf(strSecCache, "%d", statcounterINT[2]);
        draw_string(30,20,strScoreCache,FG_COLOUR);
        draw_string(28,30,strMinCache,FG_COLOUR);
        draw_string(45,30,strSecCache,FG_COLOUR);
        show_screen();
        int key  = usb_serial_getchar();
        if ((PINF >>5) & 1 || key == 'r') {
            *&statcounterINT[0] = 10;
            *&statcounterINT[1] = 0;
            *&statcounterINT[2] = 0;
            *&statcounterINT[3] = 0;
            *&statcounterINT[4] = 0;
            //*&treasure.is_visible = 1;
            respawnChar();
            *&death = 0;
            TIMSK1 = 1;
            TIMSK3 = 1;
        } else if ((PINF >>6) & 1 || key == 'q') {
            int loop = 1;
            while (loop == 1) {
                clear_screen();
                draw_string(0,0,"REDACTED",FG_COLOUR);
                show_screen();
            }
        }
    }

}
/*
    Moves the blocks along the screen in a consistent manner. Teleports them to the other side of the screen if they leave the boundaries.
*/
void moveblocks() {
    int pot0 = adc_read(0)/350;
    for (int i=0; i< statcounterINT[5];i++) {
        if (blocks[i].y == 27 || blocks[i].y == 45) {
            *&blocks[i].x = *&blocks[i].x + pot0;
        } else if (blocks[i].y == 18 || blocks[i].y == 36) {
            *&blocks[i].x = *&blocks[i].x - pot0;
        } else if (blocks[i].y == 9) {
            if (blocks[i].x != 0) {
                *&blocks[i].x = *&blocks[i].x + pot0;
            }
        }
        if (blocks[i].x > 85 && blocks[i].y !=9) {
            *&blocks[i].x = 0;
        } else if (blocks[i].x < -1) {
            *&blocks[i].x = 84;
        } else if (blocks[i].x > 85 && blocks[i].y == 9 && blocks[i].x > 9) {
            *&blocks[i].x = 13;
        }

    }
}
/*
    Creates a food sprite and returns it for use in the array.
*/
Sprite placeFood() {
    Sprite foodObj;
    int playerx = player.x + 1;
    int playery = player.y + 1;
    sprite_init(&foodObj, playerx, playery, 3, 3, foodSpr);
    return foodObj;
}
/*
    Checks if the player has any food left and places one if they do.
*/
void generateNewFood() {
    
    if (remainfood > 0) {
        food[usedfood] = placeFood();
        remainfood = remainfood - 1;
        usedfood = usedfood + 1;
        *&placefood = 0;

    }


}
/*
    Iterate over the array of food sprites and instruct the display to draw them. Also resets their sprite to default in case it was overwritten.
*/
void drawFood() {
    for (int c = 0; c < usedfood; c++) {
        *&food[c].bitmap = foodSpr;
        sprite_draw(&food[c]);
    }
}
/*
    Creates zombies and adds them to the array.
    Input: Position in the array
*/
Sprite arrayZombie( int x) {
            Sprite zombieInd;
            int zombiex = 15;
            if (x > 0) {
                zombiex = 5*x + (15 * x);
            }
            int zombiey = 3;
            sprite_init(&zombieInd,zombiex,zombiey,3,3,zombie);
            return zombieInd;
}
/*
    Creates an array of zombies and sets the variables relating to the amount on screen. Waits for the TIMER0 overflow to reach 1980 (roughly 4.5 seconds) before spawning.
*/
void initZombie() {
    if (overflow_counter > 1980 && toSpawnZombie == 1) {
        serialSend(txtbuffer, sizeof(txtbuffer), "ZSPAWN:C=%d,T=%d:%d,L=%d,SC=%d",5,statcounterINT[1],statcounterINT[2],statcounterINT[0],statcounterINT[4]);
        
        for (int i = 0; i < 5; i++) {
            
            zombieArr[i] = arrayZombie(i);
            

        }
        *&zombieRemain = 5;
        *&toSpawnZombie = 0;
    }
    
}
/*
    Iterates over the zombie array and draws them.
*/
void drawZombie() {
    for (int i = 0; i < 5; i++) {
        sprite_draw(&zombieArr[i]);
    }
}
/*
    Moves zombies further down the screen on each loop.
*/
void dropZombie() {
    for (int i = 0; i < 5; i++) {
        zombieArr[i].y = zombieArr[i].y +1;
    }
}
/*
    Primary loop, reloads everything every 100ms and is responsible for checking block collisions.
*/
void process(void) {
    clear_screen();
    if (*&statcounterINT[0] < 1) {
        *&death = 1;
        gameOver();
    }
    keypress();
    playerEdgeCheck();
    initZombie();
    dropZombie();
    blockdraw();
    *&player.y = *&player.y +0.75;
    *&player.bitmap = bm;
    
    for (int i=0;i<statcounterINT[5];i++) {
        if (spritecollide(blocks[i], player) == 1) {
            if (blocks[i].bitmap == safeblockdummy.bitmap) {
                *&player.y = *&player.y -1;
                *&statcounterINT[4] = *&statcounterINT[4] + 1;
                *&jumpregister = 0;
                if (placefood == 1) {
                    generateNewFood();
                }
            } else if (blocks[i].bitmap == unsafeblockdummy.bitmap) {
                killplayer(1);
            }
            
        }
        for (int c=0;c<5;c++) {
            if (zombieRemain == 0 && toSpawnZombie == 0) {
                *&toSpawnZombie = 1;
                *&overflow_counter = 0;
            }
            if (zombieArr[c].y > 50) {
                if (zombieArr[c].is_visible == 1) {
                    *&zombieArr[c].is_visible = 0;
                    *&zombieRemain = zombieRemain -1;   
                }
                
            }
            if (spritecollide(blocks[i], zombieArr[c]) == 1) {
                if (blocks[i].bitmap == safeblockdummy.bitmap || blocks[i].bitmap == unsafeblockdummy.bitmap) {
                    *&zombieArr[c].y = *&zombieArr[c].y - 1;
                    if (zombieArr[c].x == blocks[i].x +6) {
                        *&zombieArrMove[c] = 1;
                    }
                    else if (zombieArr[c].x == blocks[i].x -1) {
                        *&zombieArrMove[c] = 0;
                    }
                    if (zombieArrMove[c] == 0) {
                        *&zombieArr[c].x = zombieArr[c].x + 1;
                    } else if (zombieArrMove[c] == 1) {
                        *&zombieArr[c].x = zombieArr[c].x - 1;
                    }
                }
                for (int f=0;f<5;f++) {
                    if (spritecollide(food[f],zombieArr[c])) {
                        if (zombieArr[c].is_visible == 1) {
                            *&zombieArr[c].is_visible = 0;
                            *&zombieRemain = zombieRemain -1;
                            *&food[f].is_visible = 0;
                            *&remainfood = remainfood + 1;
                            *&statcounterINT[4] = *&statcounterINT[4] + 10;
                            serialSend(txtbuffer, sizeof(txtbuffer), "ZFOOD:Z=%d,F=%d,T=%d:%d",zombieRemain,remainfood,statcounterINT[1],statcounterINT[2]);
                        }
                    }
                }
                if (spritecollide(zombieArr[c],player)) {
                    killplayer(3);
                }
            }
            
            *&zombieArr[c].bitmap = zombie;
        }
        
    }
    if (jumpregister == 1) {
        player.y = player.y - 3;
        *&jumpregister = 0;
    }
    if (spritecollide(treasure, player) == 1) {
        lifechange("i",2);
        serialSend(txtbuffer, sizeof(txtbuffer), "TREASURE:SC=%d,L=%d,T=%d:%d,X=4,Y=4",statcounterINT[4],statcounterINT[0],statcounterINT[1],statcounterINT[2]);
        respawnChar();
        *&treasure.is_visible = 0;
    }
    moveblocks();
    movetreasure();
    drawZombie();
    drawFood();
    sprite_draw(&treasure);
    sprite_draw(&player);
    
    show_screen();

}
/*
    Enables the LCD backlight and sets it to the maximum level
*/
void init_backlight() {
    TC4H = OVERFLOW_TOP >> 8;
	OCR4C = OVERFLOW_TOP & 0xff;
    TCCR4A = BIT(COM4A1) | BIT(PWM4A);
	SET_BIT(DDRC, 7);
    TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
    TCCR4D = 0;
}
/*
    Configures the overflow operations for TIMER1. TIMER1 is used for incrementing the clock and determining whether to increment the minute or second variable.
*/
int timeCount = 0;
ISR(TIMER1_OVF_vect) {
    //Used for Timer
    overflow_counter2++;
    timeCount++;
    if (timeCount == 15) {
        *&timeCount = 0;
        *&statcounterINT[2] = statcounterINT[2] + 1;
        if (statcounterINT[2] == 60) {
            *&statcounterINT[2] = 0;
            *&statcounterINT[1] = statcounterINT[1] + 1;
        }
    }
}
/*
    Configures the overflow operations for TIMER0. Used for determining when to spawn zombies.
*/
ISR(TIMER0_OVF_vect) {
    //Used for Controlling Zombie Spawn
	overflow_counter++;
}
/*
    Configures the overflow operations for TIMER3. Used to alternating the LEDs when zombies are on screen.
*/
ISR(TIMER3_OVF_vect) {
    //Used for LED
    if (zombieRemain > 0) {
        if ((PINB >>2) & 1) {
            PORTB = PORTB & ~(1<<2);
            PORTB |= (1<<3);
        }
        else {
            PORTB |= (1<<2);
            PORTB = PORTB & ~(1<<3);
        }

    }

}
/*
    Main function, responsible for initialising the program as well as the timers.
*/
int main(void) {
    set_clock_speed(CPU_8MHz);
    lcd_init(LCD_DEFAULT_CONTRAST);
    TCCR0A = 0;
    TCCR0B = 3;
    TIMSK0 = 1;
    TCCR1A = 0;
    TCCR1B = 2;
    TIMSK1 = 1;
    TCCR3A = 0;
    TCCR3B = 3;
    TIMSK3 = 1;
    sei();
    adc_init();
    usb_init();
    init_backlight();
    set_backlight(ADC_MAX);
	initController();
    introMessage();
    srand(overflow_counter);
    *&overflow_counter = 0;
    init_blocks();
    init_treasure();
    dummyblock();
    *&overflow_counter = 0;
    clear_screen();
    spawnPlayerChar();
    show_screen();
	for ( ;; ) {
		process();
        _delay_ms(100);
	}
}
