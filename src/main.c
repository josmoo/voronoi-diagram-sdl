#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <SDL2/SDL.h>

#define WIDTH_AND_HEIGHT 600

#define SEEDS_COUNT 256
#define UNDEFINED_COLOR 0x00BABABA

#define EMPTY_ORIGIN (Point){-1,-1}

#define OUTPUT_FILE_PATH "output.ppm"

typedef struct {
    int x, y;
} Point;

typedef struct {
    uint32_t color;
    Point origin;
} Pixel;

static Pixel image[WIDTH_AND_HEIGHT][WIDTH_AND_HEIGHT];
static Point seeds[SEEDS_COUNT];

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

void initialize_window(void){
    if(SDL_Init(SDL_INIT_EVERYTHING) != 0){
        fprintf(stderr, "Error init SDL\n");
    }

    window = SDL_CreateWindow(
        "voronoi",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH_AND_HEIGHT,
        WIDTH_AND_HEIGHT,
        0
    );

    if(!window){
        fprintf(stderr, "Error creating window\n");
    }

    renderer = SDL_CreateRenderer(
        window,
        -1,
        0
    );

    if(!renderer){
        fprintf(stderr, "Error creating renderer\n");
    }
}

void dissect_color(uint8_t* bytes, uint32_t color){
    bytes[0] = (color & 0x000000FF);     //R
    bytes[1] = (color & 0x0000FF00)>>8;  //G
    bytes[2] = (color & 0x00FF0000)>>16; //B
}

void render_window(void){
    SDL_SetRenderDrawColor(renderer, 0,0,0,0);
    SDL_RenderClear(renderer);

    for(int y = 0; y < WIDTH_AND_HEIGHT; ++y){
        for (int x = 0; x < WIDTH_AND_HEIGHT; ++x){
            uint8_t bytes[3];
            dissect_color(bytes, image[y][x].color);
            SDL_SetRenderDrawColor(renderer, (bytes[0]), (int)bytes[1], (int)bytes[2], 255);
            SDL_RenderDrawPoint(renderer, y, x);
        }
    }
    SDL_RenderPresent(renderer);
}

void cleanup_window(void){
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
 
void generate_random_seeds(void){
    srand((unsigned int)time(NULL));
    for (size_t i = 0; i < SEEDS_COUNT; ++i){
        seeds[i].x = rand() % WIDTH_AND_HEIGHT;
        seeds[i].y = rand() % WIDTH_AND_HEIGHT;
    }
}
int sqr_dist(int x1, int y1, int x2, int y2){
    int dx = x1 - x2;
    int dy = y1 - y2;
    return dx*dx + dy*dy;
}

void fill_seed_marker(int x, int y, uint32_t * color){
    if(color){
        image[y][x] = (Pixel){*color, (Point){x, y}};
        return;
    }
    
    image[y][x] = (Pixel){(rand() % 0x1000000) + 0xFF000000, (Point){x, y}} ;
}

void render_seed_markers(uint32_t * color){
    for(size_t i = 0; i < SEEDS_COUNT; ++i){
        fill_seed_marker(seeds[i].x, seeds[i].y, color);
    }
}

//https://en.wikipedia.org/wiki/Jump_flooding_algorithm#Implementation
void fill_voronoi(void){
    for(int k = WIDTH_AND_HEIGHT/2; k > 0; k/=2){
        for (int y = 0; y < WIDTH_AND_HEIGHT; ++y){
            for (int x = 0; x < WIDTH_AND_HEIGHT; ++x){
                for(int i = y - k; i <= k + y; i += k){
                    if(i < 0 || i >= WIDTH_AND_HEIGHT){
                        continue;
                    }
                    for(int j = x - k; j <= k + x; j += k){
                        if(j < 0 || j >= WIDTH_AND_HEIGHT){
                            continue;
                        }
                        if(i == y && j == x){
                            continue;
                        }
                        uint32_t neighbors_color = image[i][j].color;
                        if(neighbors_color == UNDEFINED_COLOR){
                            continue;
                        }
                        Point neighbors_origin = image[i][j].origin;
                        if(image[y][x].color == UNDEFINED_COLOR){
                            image[y][x].color = neighbors_color;
                            image[y][x].origin = neighbors_origin;
                            continue;
                        }

                        int sqr_dist_to_pixel_origin = sqr_dist(x, y, image[y][x].origin.x, image[y][x].origin.y);
                        int sqr_dist_to_neighbor_origin = sqr_dist(x, y, neighbors_origin.x, neighbors_origin.y);
                        if(sqr_dist_to_neighbor_origin < sqr_dist_to_pixel_origin){
                            image[y][x].color = neighbors_color;
                            image[y][x].origin = neighbors_origin;
                            continue;
                        }
                    }
                }
            }
        }
    }
}

void fill_image(uint32_t color){
    for(size_t y = 0; y < WIDTH_AND_HEIGHT; ++y){
        for(size_t x = 0; x < WIDTH_AND_HEIGHT; ++x){
            image[y][x] = (Pixel){color, EMPTY_ORIGIN};
        }
    }
}

void refresh_voronoi(void){
    generate_random_seeds();
    fill_image(UNDEFINED_COLOR);
    render_seed_markers(NULL);
    fill_voronoi();
    uint32_t point = 0x00000000;
    render_seed_markers(&point);
}

void handle_key_press(SDL_Keycode sym){
    switch(sym){
    case SDLK_r:
        refresh_voronoi();
        render_window();
        break;
    default:
        break;
    }
}

int main(void){
    initialize_window();
    refresh_voronoi();
    render_window();

    SDL_Event event;
    int loop = 1;
    while(loop){
        while(SDL_PollEvent(&event)){
            switch(event.type){
            case SDL_QUIT:
                loop = 0;
                break;

            case SDL_KEYDOWN:
                handle_key_press(event.key.keysym.sym);
                break;

            default:
                break;
            }
        }
    }
    cleanup_window();
    return 0;
}