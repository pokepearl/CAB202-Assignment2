# CAB202 Assignment 2
Code for my CAB202 C Assignment 2 (TeensyPewPew Platforming Game). The task was to create a variant of the platforming game made in Assignment 1 for the TeensyPewPew, a handheld games device based around the Teensy Microcontroller, developed by the [QUT Electrical Engineering Student Society](http://www.quteess.com/). The aim of the game is to safely make it to the bottom of the screen and touch the moving treasure chest without colliding with the moving platforms and obstacles which kill the player. This variant adds additional hazzards such as randomly moving zombies which kill the player on touch and food which can be used to tempoarily eliminate a zombie from play.

## Building
1. Download the source code.
2. Download and configure the appropriate libraries in the Makefile (TeensyPewPew schematics and libraries can be found [here](https://github.com/trjstewart/qut-cab202/tree/master/Teensy%20Resources))
2. Use the Makefile to compile the code.
3. Push the generated binary to the TeensyPewPew and play.

## Credits

Licence: GPLv3