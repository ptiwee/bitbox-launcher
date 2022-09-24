#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "hashmap/hashmap.h"

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 1024;

/*********************/
/* Systems constants */
/*********************/
struct system {
    char name[255];
    char color[3];
};

struct system systems[] = {
    {"arcade", {211, 84, 0}},
    {"gameboy", {127, 140, 141}},
    {"gamegear", {44, 62, 80}},
    {"gba", {142, 68, 173}},
    {"gbc", {22, 160, 133}},
    {"megadrive", {41, 105, 176}},
    {"neogeo", {52, 73, 94}},
    {"nes", {209, 72, 65}},
    {"ngpx", {241, 196, 15}},
    {"psx", {33, 33, 33}},
    {"sms", {26, 188, 156}},
    {"snes", {251, 192, 45}},
};

struct system *current_system = systems;

struct system *next_system() {
    struct system *next = current_system;

    next += 1;
    if (next >= systems + (sizeof(systems) / sizeof(systems[0])))
        next = systems;

    return next;
}

struct system *previous_system() {
    struct system *previous = current_system;

    previous -= 1;
    if (previous < systems)
        previous = systems + (sizeof(systems) / sizeof(systems[0])) - 1;

    return previous;
}

/***************/
/* Transitions */
/***************/
typedef enum {
    T_NONE,
    T_NEXT_SYSTEM,
    T_PREVIOUS_SYSTEM,
    T_SHOW_SYSTEM,
    T_FADE_IN,
    T_FADE_OUT,
    T_NEXT_GAME,
    T_PREVIOUS_GAME,
} T_TRANSITION;

int transitions_time[] = {
    300,
    300,
    300,
    300,
    300,
    300,
    150,
    150,
};

struct transition {
    T_TRANSITION type;
    int start;
    int duration;
    T_TRANSITION next;
};

struct transition current_transition = { T_NONE, 0, 0, 0 };

void next_transition(T_TRANSITION next) {
    current_transition.next = next;
}

void switch_transition(T_TRANSITION ttype, T_TRANSITION next) {
    current_transition.type = ttype;
    current_transition.start = SDL_GetTicks();
    current_transition.duration = transitions_time[ttype];

    if (next) {
        next_transition(next);
    } else {
        switch (ttype) {
            case T_NONE:
                next_transition(T_NONE);
                break;
            case T_FADE_IN:
            case T_NEXT_SYSTEM:
            case T_PREVIOUS_SYSTEM:
                next_transition(T_SHOW_SYSTEM);
                break;
            case T_SHOW_SYSTEM:
                next_transition(T_FADE_OUT);
                break;
            case T_FADE_OUT:
            case T_NEXT_GAME:
            case T_PREVIOUS_GAME:
                next_transition(T_NONE);
                break;
        }
    }
}

/*******************/
/* Cache mechanism */
/*******************/
struct hashmap *texture_map;

struct cached_texture {
    char key[255];
    SDL_Texture *texture;
};

int texture_compare(const void *a, const void *b, void *udata) {
    const struct cached_texture *ua = a;
    const struct cached_texture *ub = b;
    return strcmp(ua->key, ub->key);
}

uint64_t texture_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const char *key = item;
    return hashmap_sip(key, strlen(key), seed0, seed1);
}

/*****************/
/* SDL functions */
/*****************/
int init(SDL_Window **window, SDL_Renderer **renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Error SDL_Init : %s\n", SDL_GetError());
        return -1;
    }

    *window = SDL_CreateWindow(
            "BitLauncher",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN);

    if (!*window) {
        printf("Error SDL_CreateWindow : %s\n", SDL_GetError());
        return -1;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(*renderer, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    if (!*renderer) {
        printf("Error SDL_CreateRenderer : %s\n", SDL_GetError());
        return -1;
    }

    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    return 0;
}

void quit(SDL_Window *window, SDL_Renderer *renderer) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

SDL_Texture *loadImage(SDL_Renderer *renderer, char *path) {
    struct cached_texture *texture = hashmap_get(texture_map, path);

    if (!texture) {
        SDL_Surface *surface = IMG_Load(path);

        texture = malloc(sizeof(struct cached_texture));
        strcpy(texture->key, path);
        texture->texture = SDL_CreateTextureFromSurface(renderer, surface);
        hashmap_set(texture_map, texture);


        SDL_FreeSurface(surface);
    }

    return texture->texture;
}

SDL_Texture *loadText(SDL_Renderer *renderer, char *text, TTF_Font *font, SDL_Color color) {
    struct cached_texture *texture = hashmap_get(texture_map, text);

    if (!texture) {
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);

        texture = malloc(sizeof(struct cached_texture));
        strcpy(texture->key, text);
        texture->texture = SDL_CreateTextureFromSurface(renderer, surface);
        hashmap_set(texture_map, texture);

        SDL_FreeSurface(surface);
    }

    return texture->texture;
}

SDL_Texture *createTexture(SDL_Renderer *renderer, char *name, int width, int height) {
    struct cached_texture *texture = hashmap_get(texture_map, name);

    if (!texture) {
        texture = malloc(sizeof(struct cached_texture));
        strcpy(texture->key, name);
        texture->texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_TARGET,
                width, height);
        SDL_SetTextureBlendMode(texture->texture, SDL_BLENDMODE_BLEND);
        hashmap_set(texture_map, texture);
    }

    return texture->texture;
}

void drawOutline(SDL_Renderer *renderer, SDL_Rect *rect, char shadow) {
    SDL_Rect outline;

    /* Draw shadow */
    if (shadow) {
        for (int i=0; i<6; i++) {
            outline.x = rect->x - (i + 2);
            outline.y = rect->y - (i + 2);
            outline.w = rect->w + 2*(i + 2);
            outline.h = rect->h + 2*(i + 2);

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 96 - 16 * i);
            SDL_RenderDrawRect(renderer, &outline);
        }
    }

    /* Draw borders */
    outline.x = rect->x - 1;
    outline.y = rect->y - 1;
    outline.w = rect->w + 2;
    outline.h = rect->h + 2;
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &outline);
}

#define ALIGN_TOP_LEFT      0b0001
#define ALIGN_MIDDLE        0b0010
#define ALIGN_MIDDLE_LEFT   0b0100

#define SIZE_CONTAIN        0b010000

void SDL_Copy(
        SDL_Renderer *renderer,
        SDL_Texture *texture,
        int x, int y, int width, int height,
        int rotate, int flags) {
    SDL_Rect srcrect, dstrect;

    srcrect.x = 0;
    srcrect.y = 0;

    SDL_QueryTexture(texture, NULL, NULL, &srcrect.w, &srcrect.h);

    dstrect.w = srcrect.w;
    dstrect.h = srcrect.h;

    float scale;
    if (flags & SIZE_CONTAIN) {
        scale = (float) width / height;

        if (scale < 0) {
            srcrect.h = srcrect.w * scale;
        } else {
            srcrect.w = srcrect.h * scale;
        }

        dstrect.w = width;
        dstrect.h = height;
    } else {
        /* Apply ratio */
        scale = (float) width / dstrect.w;

        if (dstrect.h * scale > height)
            scale = (float) height / dstrect.h;

        if (width > 0) {
            dstrect.w = dstrect.w * scale;
        }

        if (height > 0) {
            dstrect.h = dstrect.h * scale;
        }
    }

    if (flags & ALIGN_TOP_LEFT) {
        dstrect.x = x;
        dstrect.y = y;
    }

    if (flags & ALIGN_MIDDLE) {
        dstrect.x = x - dstrect.w * 0.5;
        dstrect.y = y - dstrect.h * 0.5;
    }
    
    if (flags & ALIGN_MIDDLE_LEFT) {
        dstrect.x = x;
        dstrect.y = y - dstrect.h * 0.5;
    }

    SDL_RenderCopyEx(renderer, texture, &srcrect, &dstrect, rotate, NULL, SDL_FLIP_NONE);
}

/************/
/* Conveyor */
/************/
void drawConveyorBackground(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_Texture *texture;
    SDL_Rect rect;
    SDL_Color color = {255, 255, 255};

    /* Top border */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawLine(renderer, 0, 639, 1280, 639);

    /* Bottom border */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawLine(renderer, 0, 992, 1280, 992);

    /* Background */
    rect.x = 0;
    rect.y = 640;
    rect.h = 352;
    rect.w = 1280;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
    SDL_RenderFillRect(renderer, &rect);

    /* Colored stripe */
    rect.y = 704;
    rect.h = 224;

    drawOutline(renderer, &rect, 1);
    SDL_SetRenderDrawColor(renderer, current_system->color[0], current_system->color[1], current_system->color[2], 255);
    SDL_RenderFillRect(renderer, &rect);

    /* Controls information */
    texture = loadImage(renderer, "controls/left_right.png");
    SDL_Copy(renderer, texture, 792, 960, 48, 48, 0, ALIGN_MIDDLE);

    texture = loadText(renderer, "GAMES", font, color);
    SDL_Copy(renderer, texture, 832, 962, -1, -1, 0, ALIGN_MIDDLE_LEFT);

    texture = loadImage(renderer, "controls/up_down.png");
    SDL_Copy(renderer, texture, 952, 960, 48, 48, 0, ALIGN_MIDDLE);

    texture = loadText(renderer, "SYSTEMS", font, color);
    SDL_Copy(renderer, texture, 992, 962, -1, -1, 0, ALIGN_MIDDLE_LEFT);

    texture = loadImage(renderer, "controls/a.png");
    SDL_Copy(renderer, texture, 1120, 960, 48, 48, 0, ALIGN_MIDDLE);

    texture = loadText(renderer, "PLAY", font, color);
    SDL_Copy(renderer, texture, 1152, 962, -1, -1, 0, ALIGN_MIDDLE_LEFT);
}

/*****************/
/* Corner stripe */
/*****************/
SDL_Texture *getStripe(SDL_Renderer *renderer, struct system *system) {
    SDL_Texture *stripe = createTexture(renderer, system->name, 1920, 172);
    SDL_Texture *texture;
    SDL_Rect rect;

    SDL_SetRenderTarget(renderer, stripe);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    rect.x = 0;
    rect.y = 6;
    rect.w = 1920;
    rect.h = 160;
    
    /* Colored stripe */
    SDL_SetRenderDrawColor(renderer, system->color[0], system->color[1], system->color[2], 255);
    SDL_RenderFillRect(renderer, &rect);

    /* Logo */
    char path[255];
    snprintf(path, sizeof(path), "systems/%s.png", system->name);
    texture = loadImage(renderer, path);
    SDL_Copy(renderer, texture, 960, 86, 256, 128, 0, ALIGN_MIDDLE);

    SDL_SetRenderTarget(renderer, NULL);

    return stripe;
}

void drawCorner(SDL_Renderer *renderer, float progress) {
    SDL_Texture *stripe = getStripe(renderer, current_system);
    SDL_Texture *texture;
    SDL_Rect rect;

    /* Opacify background */
    rect.x = 0;
    rect.y = 0;
    rect.w = 1280;
    rect.h = 1024;
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 255 * progress);
    SDL_RenderFillRect(renderer, &rect);

    /* Corner outline */
    rect.x = 0;
    rect.y = 6;
    rect.w = 1920;
    rect.h = 160;

    SDL_SetRenderTarget(renderer, stripe);
    drawOutline(renderer, &rect, 1);
    SDL_SetRenderTarget(renderer, NULL);

    SDL_Copy(
            renderer, stripe,
            1152 - 512 * progress , 128 + 384 * progress,
            1920, 172,
            45 * (1 - progress), ALIGN_MIDDLE);
}

void drawSlide(SDL_Renderer *renderer, float progress, bool previous) {
    SDL_Texture *stripe[2];
    SDL_Rect rect;
    int offset = progress * 160;
    int position = 672;

    stripe[0] = getStripe(renderer, current_system);

    stripe[1] = getStripe(renderer, next_system());
    if (previous) {
        stripe[1] = getStripe(renderer, previous_system());

        offset = - offset;
        position = 352;
    }

    SDL_Copy(
            renderer, stripe[0],
            640, 512 - offset,
            1920, 172,
            0, ALIGN_MIDDLE);

    SDL_Copy(
            renderer, stripe[1],
            640, position - offset,
            1920, 172,
            0, ALIGN_MIDDLE);

    /* Opacify background */
    rect.x = 0;
    rect.y = 0;
    rect.w = 1280;
    rect.h = 432;
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 255);
    SDL_RenderFillRect(renderer, &rect);

    /* Opacify background */
    rect.x = 0;
    rect.y = 592;
    rect.w = 1280;
    rect.h = 432;
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 255);
    SDL_RenderFillRect(renderer, &rect);
}

/******************/
/* Game thumbnail */
/******************/
void grownSize(SDL_Renderer *renderer, char *image, int *width, int *height) {
    SDL_Texture *texture = loadImage(renderer, image);
    SDL_QueryTexture(texture, NULL, NULL, width, height);

    float ratio = (float) *width / *height;

    if (ratio < 1) {
        if (ratio < 0.75) {
            *width = 480 * ratio;
            *height = 480;
        } else {
            *width = 360;
            *height = 360 / ratio;
        }
    } else {
        if (ratio > 1.33) {
            *width = 480;
            *height = 480 / ratio;
        } else {
            *width = 360 * ratio;
            *height = 360;
        }
    }
}

void drawThumb(SDL_Renderer *renderer, int pos, float progress) {
    SDL_Texture *thumb = createTexture(renderer, "default", 492, 492);
    SDL_Texture *texture;
    SDL_Rect rect;
    int width = 192;
    int height = 192;
    int xpos = 320;

    /* Compute size */
    if (progress > 0) {
        if (pos == -1) {
            width = 192 + (480 - 192) * (progress);
            height = 192 + (352 - 192) * (progress);
        }
        if (pos == 0) {
            width = 192 + (480 - 192) * (1 - progress);
            height = 192 + (352 - 192) * (1 - progress);
        }
    } else {
        if (pos == 0) {
            width = 192 + (480 - 192) * (1 + progress);
            height = 192 + (352 - 192) * (1 + progress);
        }
        if (pos == 1) {
            width = 192 + (480 - 192) * (-progress);
            height = 192 + (352 - 192) * (-progress);
        }
    }

    /* Compute position */
    /* Place the tile center */
    xpos = 320 + (pos + progress) * 212;

    SDL_SetRenderTarget(renderer, thumb);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    /* Cover */
    texture = loadImage(renderer, "flyer.png");
    SDL_Copy(renderer, texture, 246, 246, width, height, 0, SIZE_CONTAIN | ALIGN_MIDDLE);

    /* Outline */
    rect.x = (492 - width) / 2;
    rect.y = (492 - height) / 2;
    rect.w = width;
    rect.h = height;

    drawOutline(renderer, &rect, false);

    /* Copy thumb to renderer */
    SDL_SetRenderTarget(renderer, NULL);
    SDL_Copy(renderer, thumb, xpos, 816, -1, -1, 0, ALIGN_MIDDLE);
}

void drawConveyor(SDL_Renderer *renderer, TTF_Font *font, float progress) {
    drawConveyorBackground(renderer, font);

    drawThumb(renderer, -2, progress);
    drawThumb(renderer, 2, progress);
    drawThumb(renderer, 3, progress);
    drawThumb(renderer, 4, progress);
    drawThumb(renderer, 5, progress);
    drawThumb(renderer, -1, progress);
    drawThumb(renderer, 1, progress);
    drawThumb(renderer, 0, progress);
}

/*************/
/* Main loop */
/*************/
int main(int argc, char *argv) {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *picture;
    TTF_Font *font;
    SDL_Event e;
    char exit = 0;
    float progress;

    if (init(&window, &renderer) == 0) {
        /* Initialize maps */
        texture_map = hashmap_new(sizeof(struct cached_texture), 0, 0, 0,
                texture_hash, texture_compare, NULL, NULL);

        picture = loadImage(renderer, "snap.png");
        font = TTF_OpenFont("BebasNeue-Regular.ttf", 32);

        while (!exit) {
            progress = ((float) SDL_GetTicks() - current_transition.start) / current_transition.duration;

            if (progress > 1)
                progress = 1;

            SDL_RenderClear(renderer);
            
            SDL_RenderCopy(renderer, picture, NULL, NULL);

            /* Draw conveyor */
            switch (current_transition.type) {
                case T_NEXT_GAME:
                    drawConveyor(renderer, font, -progress);
                    break;
                case T_PREVIOUS_GAME:
                    drawConveyor(renderer, font, progress);
                    break;
                default:
                    drawConveyor(renderer, font, 0);
                    break;
            }

            /* Draw system informations */
            switch (current_transition.type) {
                case T_NEXT_SYSTEM:
                    drawSlide(renderer, progress, false);
                    break;
                case T_PREVIOUS_SYSTEM:
                    drawSlide(renderer, progress, true);
                    break;
                case T_SHOW_SYSTEM:
                    drawSlide(renderer, 0, false);
                    break;
                case T_FADE_IN:
                    drawCorner(renderer, progress);
                    break;
                case T_FADE_OUT:
                    drawCorner(renderer, 1 - progress);
                    break;
                default:
                    drawCorner(renderer, 0);
            }

            SDL_RenderPresent(renderer);

            /* Next transition */
            if (progress == 1) {
                switch (current_transition.type) {
                    case T_NEXT_SYSTEM:
                        current_system = next_system();
                        break;
                    case T_PREVIOUS_SYSTEM:
                        current_system = previous_system();
                        break;
                }

                switch_transition(current_transition.next, 0);
            }

            /* Handle key input */
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_KEYDOWN) {
                    switch (e.key.keysym.sym) {
                        case SDLK_DOWN:
                            switch (current_transition.type) {
                                case T_NONE:
                                    switch_transition(T_FADE_IN, T_NEXT_SYSTEM);
                                    break;
                                case T_SHOW_SYSTEM:
                                    switch_transition(T_NEXT_SYSTEM, 0);
                                    break;
                                case T_PREVIOUS_SYSTEM:
                                case T_NEXT_SYSTEM:
                                    next_transition(T_NEXT_SYSTEM);
                                    break;
                            }
                            break;
                        case SDLK_UP:
                            switch (current_transition.type) {
                                case T_NONE:
                                    switch_transition(T_FADE_IN, T_PREVIOUS_SYSTEM);
                                    break;
                                case T_SHOW_SYSTEM:
                                    switch_transition(T_PREVIOUS_SYSTEM, 0);
                                    break;
                                case T_PREVIOUS_SYSTEM:
                                case T_NEXT_SYSTEM:
                                    next_transition(T_PREVIOUS_SYSTEM);
                                    break;
                            }
                            break;
                        case SDLK_LEFT:
                            switch (current_transition.type) {
                                case T_NONE:
                                    switch_transition(T_PREVIOUS_GAME, 0);
                                    break;
                                case T_NEXT_GAME:
                                case T_PREVIOUS_GAME:
                                    next_transition(T_PREVIOUS_GAME);
                                    break;
                            }
                            break;
                        case SDLK_RIGHT:
                            switch (current_transition.type) {
                                case T_NONE:
                                    switch_transition(T_NEXT_GAME, 0);
                                    break;
                                case T_NEXT_GAME:
                                case T_PREVIOUS_GAME:
                                    next_transition(T_NEXT_GAME);
                                    break;
                            }
                            break;
                        case SDLK_ESCAPE:
                            exit = 1;
                            break;
                    }
                } else if (e.type == SDL_KEYUP) {
                    switch (current_transition.type) {
                        case T_NEXT_SYSTEM:
                        case T_PREVIOUS_SYSTEM:
                            next_transition(T_SHOW_SYSTEM);
                            break;
                        case T_NEXT_GAME:
                        case T_PREVIOUS_GAME:
                            next_transition(T_NONE);
                            break;
                    }
                } else if (e.type == SDL_QUIT) {
                    exit = 1;
                }
            }
        }
    }

    quit(window, renderer);

    return 0;
}
