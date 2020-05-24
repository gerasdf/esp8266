/* This file is just a trick.
 * you do need this file in the same place you put your .ino file, but also, you need to do
 * a few things in order to get GIT_HASH properly working:
 *
 * Rearranged from: https://arduino.stackexchange.com/a/51488
 *
 * Create a file named `make-git-version` and makge it executable (`chmod +x make-git-version`)
 * I placed it in my `Arduino` folder (`~/Arduino`). Its content should be:
 *
 * ----------------------------------------------------------------------
 * #!/bin/bash
 *
 * # Go to the source directory.
 * [ -n "$1" ] && cd "$1" || exit 1
 * 
 * # Build a version string with git.
 * version=$(git describe --tags --always --dirty 2> /dev/null)
 * 
 * # If this is not a git repository, fallback to the compilation date.
 * [ -n "$version" ] || version=$(date -I)
 * 
 * # Save this in git-version.h.
 * echo "#define GIT_VERSION \"$version\"" > $2/sketch/git-version.h
 * -----------------------------------------------------------------------
 *
 * And then, create a file named `platform.local.txt` in the place you already have a `platform.txt`
 * In my installation that is `~/.arduino15/packages/esp8266/hardware/esp8266/2.6.3/platform.local.txt`
 * containing the one line:
 * 
 * -----------------------------------------------------------------------
 * recipe.hooks.sketch.prebuild.1.pattern=/home/gera/Arduino/make-git-version "{build.source.path}" "{build.path}"
 * -----------------------------------------------------------------------
 */

#ifndef GIT_HASH
#define GIT_HASH "no-git-hash-support"
#endif
