# The following variables need to be set prior to including library.mk

# Versioning
include ver/version.mk

# Program name
NAME = cut

# If the project is separated among multiple sub-folders
PROJECT_ROOTS =

# Additionnal libraries (ex: -pthread, -lmath, etc)
LIBRARIES = -lm

# Additionnal flags for the compiler
FLAGS =

include res/program.mk

bin/bootstrap: src/bootstrap.c bin
	gcc -Iinc $< -o $@

build-cache: .cut bin/bootstrap
	./bin/bootstrap --cache > .cut/.cache

clean-cache: clean
	rm -f .cut/.cache