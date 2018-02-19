#!/bin/sh

## TODO: install packages including 

sudo apt-get update
sudo apt-get upgrage

sudo apt-get install vim tmux git autojump #sublime-text

sudo update-alternatives --install /usr/bin/editor editor /usr/bin/subl 100
sudo update-alternatives --config editor


# Configure autojump to start automatically
source /usr/share/autojump/autojump.sh on startup
echo '. /usr/share/autojump/autojump.sh' >> ~/.bashrc
