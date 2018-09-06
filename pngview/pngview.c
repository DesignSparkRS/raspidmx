//-------------------------------------------------------------------------
//
// The MIT License (MIT)
//
// Copyright (c) 2013 Andrew Duncan
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------

#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "backgroundLayer.h"
#include "imageLayer.h"
#include "key.h"
#include "loadpng.h"

#include "bcm_host.h"

//-------------------------------------------------------------------------

#define NDEBUG

//-------------------------------------------------------------------------

const char *program = NULL;

//-------------------------------------------------------------------------

volatile bool run = true;

//-------------------------------------------------------------------------

static void
signalHandler(
    int signalNumber)
{
    switch (signalNumber)
    {
    case SIGINT:
    case SIGTERM:

        run = false;
        break;
    };
}

//-------------------------------------------------------------------------

void usage(void)
{
    fprintf(stderr, "Usage: %s ", program);
    fprintf(stderr, "[-b <RGBA>] [-d <number>] [-l <layer>] ");
    fprintf(stderr, "[-x <offset>] [-y <offset>] <file.png>\n");
    fprintf(stderr, "    -b - set background colour 16 bit RGBA\n");
    fprintf(stderr, "         e.g. 0x000F is opaque black\n");
    fprintf(stderr, "    -d - Raspberry Pi display number\n");
    fprintf(stderr, "    -l - DispmanX layer number\n");
    fprintf(stderr, "    -x - offset (pixels from the left)\n");
    fprintf(stderr, "    -y - offset (pixels from the top)\n");

    exit(EXIT_FAILURE);
}

//-------------------------------------------------------------------------

#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2
static int getLine (char *prmpt, char *buff, size_t sz) {
    int ch, extra;
    
    // Get line with buffer overrun protection.
    if (prmpt != NULL) {
        printf ("%s", prmpt);
        fflush (stdout);
    }
    if (fgets (buff, sz, stdin) == NULL)
    return NO_INPUT;
    
    // If it was too long, there'll be no newline. In that case, we flush
    // to end of line so that excess doesn't affect the next call.
    if (buff[strlen(buff)-1] != '\n') {
        extra = 0;
        while (((ch = getchar()) != '\n') && (ch != EOF))
        extra = 1;
        return (extra == 1) ? TOO_LONG : OK;
    }
    
    // Otherwise remove newline and give string back to caller.
    buff[strlen(buff)-1] = '\0';
    return OK;
}

//-----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    uint16_t background = 0x000F;
    int32_t layer = 1;
    uint32_t displayNumber = 0;
    int32_t xOffset = 0;
    int32_t yOffset = 0;
    bool xOffsetSet = false;
    bool yOffsetSet = false;

    program = basename(argv[0]);

    //---------------------------------------------------------------------

    int opt = 0;

    while ((opt = getopt(argc, argv, "b:d:l:x:y:")) != -1)
    {
        switch(opt)
        {
        case 'b':

            background = strtol(optarg, NULL, 16);
            break;

        case 'd':

            displayNumber = strtol(optarg, NULL, 10);
            break;

        case 'l':

            layer = strtol(optarg, NULL, 10);
            break;

        case 'x':

            xOffset = strtol(optarg, NULL, 10);
            xOffsetSet = true;
            break;

        case 'y':

            yOffset = strtol(optarg, NULL, 10);
            yOffsetSet = true;
            break;

        default:

            usage();
            break;
        }
    }

    //---------------------------------------------------------------------

    if (optind >= argc)
    {
        usage();
    }

    //---------------------------------------------------------------------

    if (signal(SIGINT, signalHandler) == SIG_ERR)
    {
        perror("installing SIGINT signal handler");
        exit(EXIT_FAILURE);
    }

    //---------------------------------------------------------------------

    if (signal(SIGTERM, signalHandler) == SIG_ERR)
    {
        perror("installing SIGTERM signal handler");
        exit(EXIT_FAILURE);
    }

    //---------------------------------------------------------------------

    bcm_host_init();

    //---------------------------------------------------------------------

    DISPMANX_DISPLAY_HANDLE_T display
        = vc_dispmanx_display_open(displayNumber);
    assert(display != 0);

    //---------------------------------------------------------------------

    DISPMANX_MODEINFO_T info;
    int result = vc_dispmanx_display_get_info(display, &info);
    assert(result == 0);

    //---------------------------------------------------------------------

    BACKGROUND_LAYER_T backgroundLayer;

    if (background > 0)
    {
        initBackgroundLayer(&backgroundLayer, background, 0);
    }

    IMAGE_LAYER_T imageLayer;
    if (loadPng(&(imageLayer.image), argv[optind]) == false)
    {
        fprintf(stderr, "unable to load %s\n", argv[optind]);
    }
    createResourceImageLayer(&imageLayer, layer);

    //---------------------------------------------------------------------

    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
    assert(update != 0);

    if (background > 0)
    {
        addElementBackgroundLayer(&backgroundLayer, display, update);
    }

    if (xOffsetSet == false)
    {
        xOffset = (info.width - imageLayer.image.width) / 2;
    }

    if (yOffsetSet == false)
    {
        yOffset = (info.height - imageLayer.image.height) / 2;
    }

    addElementImageLayerOffset(&imageLayer,
                               xOffset,
                               yOffset,
                               display,
                               update);

    result = vc_dispmanx_update_submit_sync(update);
    assert(result == 0);

    //---------------------------------------------------------------------

    int32_t step = 1;

    while (run)
    {
        
        bool moveLayer = false;
        
//        int ch = 0;
//        
//        ch = getc( stdin );
//        
//        switch (ch){
//            case 27:
//                run = false;
//                break;
//                
//            case '1':
//                xOffset = 100;
//                moveLayer = true;
//                break;
//               
//            case '2':
//                xOffset = 200;
//                moveLayer = true;
//                break;
//                
//            case '3':
//                xOffset = 300;
//                moveLayer = true;
//                break;
//                
//            case '4':
//                xOffset = 400;
//                moveLayer = true;
//                break;
//                
//            case '5':
//                xOffset = 500;
//                moveLayer = true;
//                break;
//                
//            case '6':
//                xOffset = 600;
//                moveLayer = true;
//                break;
//                
//        }
        
        
        
//while (getline(cin,stdin)) {
        
        //there is no find in c
        // use strstr
        //there is no std::string also just char arrays
            
            
//            if (lineInput.find("x:")){
//                std::string sx;
//                sx = lineInput.substr((lineInput.find("x:") + 2), string::npos);
//                xOffset = strtol(sx, NULL, 10);
//                moveLayer = true;
//            }
//            
//            if (lineInput.find("y:")){
//                std::string sy;
//                sy = lineInput.substr((lineInput.find"y:") + 2), string::npos);
//                yOffset = strtol(sy, NULL, 10);
//                moveLayer = true;
//            }
//}
 
        
        int rc;
        char buff[20];
        
        rc = getLine (NULL, buff, sizeof(buff));
        if (rc == OK) {
            
            char * pch;
            pch = strtok (buff," :;,");
            xOffset = strtol(pch, NULL, 10);
            
            pch = strtok (NULL, " :;,");
            yOffset = strtol(pch, NULL, 10);
            
            moveLayer = true;
        }
        
        
        
        
        if (moveLayer)
        {
            update = vc_dispmanx_update_start(0);
            assert(update != 0);
            
            moveImageLayer(&imageLayer, xOffset -imageLayer.image.width/2, yOffset-imageLayer.image.height/2, update);
            
            result = vc_dispmanx_update_submit_sync(update);
            assert(result == 0);
        }

        
///////////////////////////////////////////////////////////////////////////////////
//        int c = 0;
//        if (keyPressed(&c))
//        {
//            c = tolower(c);
//
//            
//
//            switch (c)
//            {
//            case 27:
//
//                run = false;
//                break;
//
//            case 'a':
//
//                xOffset -= step;
//                moveLayer = true;
//                break;
//
//            case 'd':
//
//                xOffset += step;
//                moveLayer = true;
//                break;
//
//            case 'w':
//
//                yOffset -= step;
//                moveLayer = true;
//                break;
//
//            case 's':
//
//                yOffset += step;
//                moveLayer = true;
//                break;
//
//            case '+':
//
//                if (step == 1)
//                {
//                    step = 5;
//                }
//                else if (step == 5)
//                {
//                    step = 10;
//                }
//                else if (step == 10)
//                {
//                    step = 20;
//                }
//                break;
//
//            case '-':
//
//                if (step == 20)
//                {
//                    step = 10;
//                }
//                else if (step == 10)
//                {
//                    step = 5;
//                }
//                else if (step == 5)
//                {
//                    step = 1;
//                }
//                break;
//            }
//
//            if (moveLayer)
//            {
//                update = vc_dispmanx_update_start(0);
//                assert(update != 0);
//
//                moveImageLayer(&imageLayer, xOffset, yOffset, update);
//
//                result = vc_dispmanx_update_submit_sync(update);
//                assert(result == 0);
//            }
//        }
//        else
//        {
//            usleep(100000);
//        }
    }

    //---------------------------------------------------------------------

    keyboardReset();

    //---------------------------------------------------------------------

    if (background > 0)
    {
        destroyBackgroundLayer(&backgroundLayer);
    }

    destroyImageLayer(&imageLayer);

    //---------------------------------------------------------------------

    result = vc_dispmanx_display_close(display);
    assert(result == 0);

    //---------------------------------------------------------------------

    return 0;
}

