# sub-cropper

A command line utility to locate subtitles in a screenshot and isolate them.

## Installation

`sub-cropper` depends on libpng to read and write png files.
Build and install the executable with `make install`.

## Usage

Running `sub-cropper foo.png` while (try) to find the subtitles in foo.png,
and write them to cropped_1.png, cropped_2.png, and so on.
