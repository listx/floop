# FLOOP

## What is it?

Floop is a pseudorandom number generator that outputs a stream of bytes.
Its main purpose is to be used as a program to do (hopfully) secure wipes of storage devices such as hard drives and flash media.

## Usage

Floop outputs *only to STDOUT*; if you want to write to a file, pipe it to `dd` for controlling the filename and size.

## Technical Notes

Floop is multithreaded and uses child threads to generate large chunks of bytes.
The size of these chunks is up for you to decide; trial and error is necessary to figure out the optimum speeds for your platform.
