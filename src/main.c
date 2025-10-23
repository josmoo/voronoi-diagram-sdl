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

#define REFRESH_BMP_PATH "./REFRESH.bmp"
#define ORIGIN_POINTS_BMP_PATH "./ORIGIN_POINTS.bmp"
#define SEEDS_BMP_PATH "./SEEDS.bmp"
#define NUMBERS_BMP_PATH "./NUMBERS.bmp"

SDL_Texture* refresh_text = NULL;
SDL_Texture* origin_points_text = NULL;
SDL_Texture* seeds_text = NULL;
SDL_Texture* numbers_text = NULL;

typedef struct {
    int x, y;
} Point;

typedef struct {
    uint32_t color;
    Point origin;
} Pixel;

static Pixel image[VORONOI_LENGTH][VORONOI_LENGTH];
static Point seeds[SEEDS_COUNT];
static int draw_origin_points = 0;

SDL_Rect button = {};
SDL_Rect checkbox = {};
SDL_Rect origin_toggle_rect = {};
SDL_Rect seeds_count_input = {};
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

void render_window(){
    SDL_SetRenderDrawColor(renderer, WINDOW_BACKGROUND_COLOR);
    SDL_RenderClear(renderer);

    unsigned int y_offset = (WINDOW_HEIGHT - VORONOI_LENGTH) >> 1;
    unsigned int x_offset = (WINDOW_WIDTH - VORONOI_LENGTH) >> 4;

    button.h = 32;
    button.x = VORONOI_LENGTH + x_offset + x_offset;
    button.y = VORONOI_LENGTH + y_offset - button.h; 
    button.w = x_offset * 9;
    SDL_SetRenderDrawColor(renderer, BUTTON_COLOR);

    SDL_Rect refresh_rect = {button.x + ((button.w - 115)>>1), button.y + 5, 115, 21};
    SDL_RenderFillRect(renderer, &button);
    SDL_RenderCopy(renderer, refresh_text, NULL, &refresh_rect);

    checkbox.x = button.x;
    checkbox.y = button.y - y_offset + 2;
    checkbox.w = 13;
    checkbox.h = 18;
    SDL_RenderDrawRect(renderer, &checkbox);

    if(draw_origin_points){
        SDL_Rect checkbox_fill = {checkbox.x + 2, checkbox.y + 2, checkbox.w - 4, checkbox.h - 4};
        SDL_RenderFillRect(renderer, &checkbox_fill);
    }

    origin_toggle_rect.x = button.x + 21;
    origin_toggle_rect.y = button.y - y_offset;
    origin_toggle_rect.w = 207;
    origin_toggle_rect.h = 21;
    SDL_RenderCopy(renderer, origin_points_text, NULL, &origin_toggle_rect);

    SDL_Rect seeds_count_label = {button.x, origin_toggle_rect.y - y_offset, 90, 21};
    SDL_RenderCopy(renderer, seeds_text, NULL, &seeds_count_label);

    seeds_count_input.x = seeds_count_label.x + seeds_count_label.w;
    seeds_count_input.y = seeds_count_label.y + 2;
    seeds_count_input.w = button.w - (seeds_count_label.w);
    seeds_count_input.h = 18;
    SDL_RenderFillRect(renderer, &seeds_count_input);
    
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

void refresh_voronoi(){
    generate_random_seeds();
    fill_image(UNDEFINED_COLOR);
    render_seed_markers(NULL);
    fill_voronoi();
    if(draw_origin_points){
        uint32_t black = 0xFF000000;
        render_seed_markers(&black);
    }
    render_window(refresh_text, origin_points_text);
}

void load_bmp(SDL_Texture** texture, const char* bmp_path){
    SDL_Surface* surface = SDL_LoadBMP(bmp_path);
    if(!surface){
        fprintf(stderr, "couldn't load surface %s\n", bmp_path);        
    } 
    *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if(!*texture){
        fprintf(stderr, "couldn't load texture %s\n", bmp_path);  
    }
    SDL_FreeSurface(surface);
}

int process_events(){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch(event.type){
        case SDL_QUIT:
            return 0;
            break;

        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_r){
                refresh_voronoi(refresh_text, origin_points_text);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if(inside_rect(event.button.x, event.button.y, &button)){
                refresh_voronoi(refresh_text, origin_points_text);
            }
            else if(inside_rect(event.button.x, event.button.y, &checkbox) 
                    || inside_rect(event.button.x, event.button.y, &origin_toggle_rect)){
                draw_origin_points = (draw_origin_points + 1) % 2;
                if(draw_origin_points){
                    uint32_t black = 0xFF000000;
                    render_seed_markers(&black);
                }
                render_window(refresh_text, origin_points_text);
            }
            break;

        default:
            break;
        }
    }
    return 1;
}

void initalize_textures(void){
    load_bmp(&refresh_text, REFRESH_BMP_PATH);
    load_bmp(&origin_points_text, ORIGIN_POINTS_BMP_PATH);
    load_bmp(&seeds_text, SEEDS_BMP_PATH);
    load_bmp(&numbers_text, NUMBERS_BMP_PATH);    
}

void cleanup_textures(void){
    SDL_DestroyTexture(refresh_text);
    SDL_DestroyTexture(origin_points_text);
    SDL_DestroyTexture(seeds_text);
    SDL_DestroyTexture(numbers_text);
}

int main(void){
    initialize_window(); 
    initalize_textures();
    refresh_voronoi();

    while(process_events()){};

    cleanup_textures();
    cleanup_window();
    return 0;
}