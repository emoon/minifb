#!/bin/bash
set -e

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
pushd $dir

rm -rf gdb
rm -rf djgpp
rm -rf dosbox-x

base_url="https://marioslab.io/dump/dos"
gdb_url=""
djgpp_url=""
dosbox_url=""
os=$OSTYPE

#--------------------------------------
if [[ "$os" == "cygwin" ]] || [[ "$os" == "msys" ]]; then

    gdb_url="https://github.com/badlogic/gdb-7.1a-djgpp/releases/download/gdb-7.1a-djgpp/gdb-7.1a-djgpp-windows.zip";
    djgpp_url="https://github.com/andrewwutw/build-djgpp/releases/download/v3.3/djgpp-mingw-gcc1210-standalone.zip";
    dosbox_url="https://github.com/badlogic/dosbox-x/releases/download/dosbox-x-gdb-v0.84.5/dosbox-x-mingw-win64-20221223232734.zip";

elif [[ "$os" == "linux-gnu"* ]]; then

    gdb_url="https://github.com/badlogic/gdb-7.1a-djgpp/releases/download/gdb-7.1a-djgpp/gdb-7.1a-djgpp-linux.zip";
    djgpp_url="https://github.com/andrewwutw/build-djgpp/releases/download/v3.3/djgpp-linux64-gcc1210.tar.bz2";
    dosbox_url="https://github.com/badlogic/dosbox-x/releases/download/dosbox-x-gdb-v0.84.5/dosbox-x-0.84.5-linux.zip";

elif [[ "$os" == "darwin"* ]]; then

    gdb_url="https://github.com/badlogic/gdb-7.1a-djgpp/releases/download/gdb-7.1a-djgpp/gdb-7.1a-djgpp-macos-x86_64.zip";
    djgpp_url="https://github.com/andrewwutw/build-djgpp/releases/download/v3.3/djgpp-osx-gcc1210.tar.bz2";
    dosbox_url="https://github.com/badlogic/dosbox-x/releases/download/dosbox-x-gdb-v0.84.5/dosbox-x-macosx-x86_64-20221223232510.zip";

else

    echo "Sorry, this template doesn't support $os"
    exit

fi

#--------------------------------------
echo "Installing GDB"
echo " $gdb_url"
mkdir -p gdb
pushd gdb &> /dev/null
curl -L $gdb_url --output gdb.zip &> /dev/null
unzip -o gdb.zip > /dev/null
rm gdb.zip > /dev/null
popd &> /dev/null
echo " [] Installed GDB"

#--------------------------------------
echo "Installing DJGPP"
echo " $djgpp_url"
echo " OS: $os"
echo " uname: $(uname -r) "

if [[ "$djgpp_url" == *.zip ]]; then

    curl -L $djgpp_url --output djgpp.zip &> /dev/null
    if [[ ! -f "djgpp.zip" ]]; then
        echo "Error: Failed to download DJGPP (zip file not found)"
        exit 1
    fi
    unzip djgpp.zip &> /dev/null
    rm djgpp.zip

elif [[ "$djgpp_url" == *.tar.bz2 ]]; then

    curl -L $djgpp_url --output djgpp.tar.bz2 &> /dev/null
    if [[ ! -f "djgpp.tar.bz2" ]]; then
        echo "Error: Failed to download DJGPP (tar.bz2 file not found)"
        exit 1
    fi
    tar xf djgpp.tar.bz2
    rm djgpp.tar.bz2

else

    echo "Error: Unknown file extension for DJGPP URL: $djgpp_url"
    exit 1

fi
echo " [] Installed DJGPP"

#--------------------------------------
echo "Installing DOSBox-x"
echo " $dosbox_url"
curl -L $dosbox_url --output dosbox.zip &> /dev/null
unzip -o dosbox.zip &> /dev/null
rm dosbox.zip
echo " [] Installed DOSBox-x"

if [[ $1 = "--with-vs-code" ]]; then
    echo "Installing VS Code extensions"
    if [[ $(code --version) ]]; then
        code --install-extension llvm-vs-code-extensions.vscode-clangd --install-extension ms-vscode.cmake-tools --install-extension ms-vscode.cpptools --install-extension webfreak.debug
        cp -r .vscode ../../../.vscode
    else
        echo "WARNING: could not find 'code' on path. Could not install VS Code extensions!"
        echo "         Ensure 'code' is on your PATH and re-run 'download-tools.sh' to install"
        echo "         the VS Code extensions."
    fi
fi

#--------------------------------------
if [[ "$os" == "linux-gnu"* ]]; then

    chmod a+x gdb/gdb > /dev/null
    chmod a+x djgpp/bin/*
    chmod a+x djgpp/i586-pc-msdosdjgpp/bin/*
    chmod a+x dosbox-x/dosbox-x-sdl1
    ln -s $(pwd)/dosbox-x/dosbox-x-sdl1 dosbox-x/dosbox-x
    echo
    echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo
    echo " Please install the following packages using your Linux distribution's "
    echo " package manager:"
    echo

    # Detect Linux distribution and show appropriate install command
    if command -v apt &> /dev/null; then
        echo " sudo apt install libncurses5 libfl-dev libslirp-dev libfluidsynth-dev"
    elif command -v dnf &> /dev/null; then
        echo " sudo dnf install ncurses-libs flex-devel libslirp-devel fluidsynth-devel"
    elif command -v pacman &> /dev/null; then
        echo " sudo pacman -S ncurses flex libslirp fluidsynth"
    elif command -v zypper &> /dev/null; then
        echo " sudo zypper install ncurses-devel flex libslirp-devel fluidsynth-devel"
    elif command -v apk &> /dev/null; then
        echo " sudo apk add ncurses-dev flex libslirp-dev fluidsynth-dev"
    else
        echo " libncurses5 libfl-dev libslirp-dev libfluidsynth-dev"
    fi

    echo
    echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"

elif [[ "$os" == "darwin"* ]]; then

    chmod a+x gdb/gdb > /dev/null
    chmod a+x djgpp/bin/*
    chmod a+x djgpp/i586-pc-msdosdjgpp/bin/*
    chmod a+x dosbox-x/dosbox-x.app/Contents/MacOS/dosbox-x
    ln -s $(pwd)/dosbox-x/dosbox-x.app/Contents/MacOS/dosbox-x dosbox-x/dosbox-x

elif [[ "$os" == "cygwin" ]] || [[ "$os" == "msys" ]] || [[ $(uname -r) =~ WSL ]]; then

    rm -rf "COPYING"
    mv mingw-build/mingw dosbox-x
    rm -rf mingw-build

fi

popd &> /dev/null