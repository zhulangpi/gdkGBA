#include "sdl.h"
#include "sound.h"

static SDL_Joystick    *joy;

static void js_init(void)
{
    if (SDL_NumJoysticks() > 0){ 
        joy = SDL_JoystickOpen(0);
        if (joy != NULL){
            printf("Opened Joystick 0\n");
            printf("Name: %s\n", SDL_JoystickNameForIndex(0));
            printf("Number of Axes: %d\n", SDL_JoystickNumAxes(joy));
            printf("Number of Buttons: %d\n", SDL_JoystickNumButtons(joy));
            printf("Number of Balls: %d\n", SDL_JoystickNumBalls(joy));
            SDL_JoystickEventState(SDL_ENABLE);
        }else{
            printf("Couldn't open Joystick 0\n");
        }
    }else{
        printf("no joystick!\n");
    }
}

static void js_uninit(void)
{
    // Close if opened
    if (SDL_JoystickGetAttached(joy)) {
        SDL_JoystickClose(joy);
    }
}

void sdl_init() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

    window   = SDL_CreateWindow("gdkGBA", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 240, 160, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture  = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        240,
        160);

    tex_pitch = 240 * 4;

    SDL_AudioSpec spec = {
        .freq     = SND_FREQUENCY, //32KHz
        .format   = AUDIO_S16SYS, //Signed 16 bits System endiannes
        .channels = SND_CHANNELS, //Stereo
        .samples  = SND_SAMPLES, //16ms
        .callback = sound_mix,
        .userdata = NULL
    };

    SDL_OpenAudio(&spec, NULL);
    SDL_PauseAudio(0);

    js_init();
}

void sdl_uninit() {
    js_uninit();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_CloseAudio();
    SDL_Quit();
}
