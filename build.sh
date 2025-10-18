#!/bin/sh

set -xe

gcc -Wall -std=c99 ./src/main.c -lSDL2 -o voronoi 