#!/bin/bash
sudo modprobe fbtft_device custom name=fb_ili9341 gpios=dc:0,reset:1 speed=33000000 busnum=1 rotate=90 fps=30
sudo FRAMEBUFFER=/dev/fb0 xinit ./gdkGBA hywz.gba
