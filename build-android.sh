#!/bin/bash
N=$(echo -en '\033[0m')
RD=$(echo -en '\033[07;31m') # tulisan diberi blok
RED=$(echo -en '\033[00;31m')
GR=$(echo -en '\033[07;32m') # tulisan diberi blok
GRN=$(echo -en '\033[00;32m')
YLW=$(echo -en '\033[00;33m')
BLUE=$(echo -en '\033[00;34m')
MTA=$(echo -en '\033[00;35m')
LMA=$(echo -en '\033[20;36m')
PURP=$(echo -en '\033[00;35m')
CYAN=$(echo -en '\033[00;36m')
LGRAY=$(echo -en '\033[00;38m')
LRD=$(echo -en '\033[07;31m')
LRED=$(echo -en '\033[01;31m')
LGR=$(echo -en '\033[01;32m')
LYL=$(echo -en '\033[01;33m')
LYLO=$(echo -en '\033[07;33m') # tulisan diberi blok
LBL=$(echo -en '\033[01;34m')
LBLU=$(echo -en '\033[07;34m')
LMA=$(echo -en '\033[01;35m')
LPPLE=$(echo -en '\033[01;35m')
LCY=$(echo -en '\033[01;36m')
LCYN=$(echo -en '\033[07;36m')
WHT=$(echo -en '\033[01;37m')
clear
sleep 2
echo "               ${GR}** cpuminer builder for android device **${N}"
sleep 2
echo ""
echo ""
echo ""
echo "${LYLO}cleaning autotools artifacts...${N}" && sleep 2
# Force-clean every generated autotools file so configure is never stale or malformed
rm -rf autom4te.cache
rm -f Makefile Makefile.in aclocal.m4 compat/Makefile.in
rm -f compile config.guess config.sub config.status configure
rm -f cpuminer-config.h.in depcomp install-sh missing config.log
echo "                              ${GR}=> done.${N}" && sleep 2

echo "${LYLO}running autogen...${N}" && sleep 2
if ./autogen.sh; then
        echo "                              ${GR}=> done.${N}" && sleep 2
else
        echo "${LRD}autogen.sh failed — install autoconf automake libtool pkg-config${N}"
        exit 1
fi

# Sanity-check the generated configure script
if ! head -n 3 configure | grep -q '#!/'; then
        echo "${LRD}configure looks malformed — re-run: pkg install autoconf automake libtool${N}"
        exit 1
fi

echo "${LYLO}configuring.....${N}" && sleep 2
# Termux/Android uses clang — do not use gcc/g++, it will fail configure
CC=clang CXX=clang++ ./configure --with-crypto --with-curl --disable-assembly \
        CFLAGS="-Ofast -fuse-linker-plugin -ftree-loop-if-convert-stores -march=native" \
        LDFLAGS="-march=native"
[ $? = 0 ] || exit $?
echo "                              ${GR}=> done.${N}" && sleep 2

if [ -z "$NPROC" ]; then
        NPROC=$(nproc 2>/dev/null)
        NPROC=${NPROC:-1}
fi

echo "${LYLO}building on ${NPROC} processes...${N}" && sleep 2
make -j $NPROC

if [ $? != 0 ]; then
        echo "                              ${LRD}ERROR...!!!"
        echo "${LRD}Compilation failed (make=$?)."
        echo "${LMA}Common causes: missing autoconf automake libtool clang pkg-config libcurl openssl"
        echo "${LYL}Run: pkg install clang make autoconf automake libtool pkg-config git${N}"
        exit 1
fi
echo "                              ${GR}=> done.${N}" && sleep 2
echo "${LCYN}ls -l cpuminer${N}" && sleep 2
ls -l cpuminer

echo "${LYLO}stripping...${N}" && sleep 2
strip -s cpuminer
[ $? = 0 ] || exit $?
echo "                              ${GR}=> done.${N}"
