#!/bin/bash
set -e

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
pushd $dir

rm -rf tools/gdb
rm -rf tools/djgpp
rm -rf tools/dosbox-x

base_url="https://marioslab.io/dump/dos"
gdb_url=""
djgpp_url=""
dosbox_url=""
os=$OSTYPE

if [[ "$os" == "linux-gnu"* ]]; then
    gdb_url="$base_url/gdb/gdb-7.1a-djgpp-linux.zip"
    djgpp_url="$base_url/djgpp/djgpp-linux64-gcc1210.tar.bz2"
    dosbox_url="$base_url/dosbox-x/dosbox-x-0.84.4-linux.zip"
elif [[ "$os" == "darwin"* ]]; then
    gdb_url="$base_url/gdb/gdb-7.1a-djgpp-macos-x86_64.zip"
    djgpp_url="$base_url/djgpp/djgpp-osx-gcc1210.tar.bz2"
    dosbox_url="$base_url/dosbox-x/dosbox-x-0.84.5-macos.zip"
elif [[ "$os" == "cygwin" ]] || [[ "$os" == "msys" ]] || [[ $(uname -r) =~ WSL ]]; then
    gdb_url="$base_url/gdb/gdb-7.1a-djgpp-windows.zip"
    djgpp_url="$base_url/djgpp/djgpp-mingw-gcc1210-standalone.zip"
    dosbox_url="$base_url/dosbox-x/dosbox-x-0.84.5-windows.zip"
else
    echo "Sorry, this template doesn't support $os"
    exit
fi

echo "Installing GDB"
echo "$gdb_url"
mkdir -p gdb
pushd gdb &> /dev/null
curl $gdb_url --output gdb.zip &> /dev/null
unzip -o gdb.zip > /dev/null
rm gdb.zip > /dev/null
popd &> /dev/null

echo "Installing DJGPP"
echo "$djgpp_url"
if [[ "$os" == "cygwin" ]] || [[ "$os" == "msys" ]] || [[ $(uname -r) =~ WSL ]]; then
    curl $djgpp_url --output djgpp.zip &> /dev/null
    unzip djgpp.zip &> /dev/null
    rm djgpp.zip
else
    curl $djgpp_url --output djgpp.tar.bz2 &> /dev/null
    tar xf djgpp.tar.bz2
    rm djgpp.tar.bz2
fi

echo "Installing DOSBox-x"
echo "$dosbox_url"
curl $dosbox_url --output dosbox.zip &> /dev/null
unzip -o dosbox.zip &> /dev/null
rm dosbox.zip

echo "Installing VS Code extensions"
if [[ $(code --version) ]]; then
    code --install-extension llvm-vs-code-extensions.vscode-clangd --install-extension ms-vscode.cmake-tools --install-extension ms-vscode.cpptools --install-extension webfreak.debug
    cp -r .vscode ../../../.vscode
else
    echo "WARNING: could not find 'code' on path. Could not install VS Code extensions!"
    echo "         Ensure 'code' is on your PATH and re-run 'download-tools.sh' to install"
    echo "         the VS Code extensions."
fi

if [[ "$os" == "linux-gnu"* ]]; then
    chmod a+x gdb/gdb > /dev/null
elif [[ "$os" == "darwin"* ]]; then
    chmod a+x gdb/gdb > /dev/null
    chmod a+x dosbox-x/dosbox-x.app/Contents/MacOS/dosbox-x
    ln -s $(pwd)/dosbox-x/dosbox-x.app/Contents/MacOS/dosbox-x dosbox-x/dosbox-x
elif [[ "$os" == "cygwin" ]] || [[ "$os" == "msys" ]] || [[ $(uname -r) =~ WSL ]]; then
    echo "" > /dev/null
fi

popd &> /dev/null