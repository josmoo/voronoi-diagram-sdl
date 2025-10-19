#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <SDL2/SDL.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 680
#define VORONOI_LENGTH 600
#define WINDOW_BACKGROUND_COLOR 15, 15, 15, 255
#define BUTTON_COLOR 246,246,246,255

#define SEEDS_COUNT 256
#define UNDEFINED_COLOR 0x00BABABA

#define EMPTY_ORIGIN (Point){-1,-1}

#define OUTPUT_FILE_PATH "output.ppm"

//todo: ui button to refresh
//          |REFRESH VORONOI| 
//      checkbox to enable / disable origin points (optional stretch goal, let user choose color)
//          [✓] DISPLAY ORIGIN POINTS

//text needed: _ADEFGHILNOPRSTVY [ ] [✓]

typedef struct {
    int x, y;
} Point;

typedef struct {
    uint32_t color;
    Point origin;
} Pixel;

static Pixel image[VORONOI_LENGTH][VORONOI_LENGTH];
static Point seeds[SEEDS_COUNT];
static SDL_Rect button = {};
static SDL_Rect checkbox = {};

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
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
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

int inside_rect(int x, int y, SDL_Rect * rect){
    if(x >= rect->x && x <= rect->x + rect->w && y >= rect->y && y <= rect->y + rect->h){
        return 1;
    }
    return 0;
}

void dissect_color(uint8_t* bytes, uint32_t color){
    bytes[0] = (color & 0x000000FF);     //R
    bytes[1] = (color & 0x0000FF00)>>8;  //G
    bytes[2] = (color & 0x00FF0000)>>16; //B
}

void render_window(void){
    SDL_SetRenderDrawColor(renderer, WINDOW_BACKGROUND_COLOR);
    SDL_RenderClear(renderer);

    unsigned int y_offset = (WINDOW_HEIGHT - VORONOI_LENGTH) / 2;
    unsigned int x_offset = (WINDOW_WIDTH - VORONOI_LENGTH) / 16;

    //draw UI
    button.w = x_offset * 8;
    button.h = 32;
    button.x = VORONOI_LENGTH + x_offset + x_offset;
    button.y = VORONOI_LENGTH + y_offset - button.h; 
    SDL_SetRenderDrawColor(renderer, BUTTON_COLOR);
    SDL_RenderFillRect(renderer, &button);

    //draw voronoi
    for(int y = 0; y < VORONOI_LENGTH; ++y){
        for (int x = 0; x < VORONOI_LENGTH; ++x){
            uint8_t bytes[3];
            dissect_color(bytes, image[y][x].color);
            SDL_SetRenderDrawColor(renderer, (bytes[0]), (int)bytes[1], (int)bytes[2], 255);
            SDL_RenderDrawPoint(renderer, x + x_offset, y + y_offset);
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
        seeds[i].x = rand() % VORONOI_LENGTH;
        seeds[i].y = rand() % VORONOI_LENGTH;
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
    for(int k = VORONOI_LENGTH/2; k > 0; k/=2){
        for (int y = 0; y < VORONOI_LENGTH; ++y){
            for (int x = 0; x < VORONOI_LENGTH; ++x){
                for(int i = y - k; i <= k + y; i += k){
                    if(i < 0 || i >= VORONOI_LENGTH){
                        continue;
                    }
                    for(int j = x - k; j <= k + x; j += k){
                        if(j < 0 || j >= VORONOI_LENGTH){
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
    for(size_t y = 0; y < VORONOI_LENGTH; ++y){
        for(size_t x = 0; x < VORONOI_LENGTH; ++x){
            image[y][x] = (Pixel){color, EMPTY_ORIGIN};
        }
    }
}

void refresh_voronoi(uint32_t * marker_color){
    generate_random_seeds();
    fill_image(UNDEFINED_COLOR);
    render_seed_markers(NULL);
    fill_voronoi();
    if(marker_color){
        render_seed_markers(marker_color);
    }
}

int main(void){
    initialize_window();
    refresh_voronoi(NULL);
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
                if(event.key.keysym.sym == SDLK_r){
                    refresh_voronoi(NULL);
                    render_window();
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if(inside_rect(event.button.x, event.button.y, &button)){
                    refresh_voronoi(NULL);
                    render_window();
                }
                break;

            default:
                break;
            }
        }
    }
    cleanup_window();
    return 0;
}