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
echo "${LYLO}create configure...${N}" && sleep 2
# Always force-clean autotools artifacts so configure is never stale or malformed
rm -rf autom4te.cache
rm -f Makefile.in aclocal.m4 compat/Makefile.in
rm -f compile config.guess config.sub config.status configure
rm -f cpuminer-config.h.in depcomp install-sh missing config.log
if ./autogen.sh; then
        echo "                              ${GR}=> done.${N}" && sleep 3
else
        exit 1
fi
echo "${LYLO}cleaning previus build...${N}" && sleep 3
if [ -e Makefile ]; then
        echo " ${LBLU}clean${N}"
        make distclean 2>/dev/null || true
fi
echo "                              ${GR}=> done.${N}" && sleep 3
# --disable-assembly: some ASM code doesn't build on ARM
# Termux/Android uses clang — do not use gcc/g++, it will fail configure
echo "${LYLO}configuring.....${N}" && sleep 3
CC=clang CXX=clang++ ./configure --with-crypto --with-curl --disable-assembly \
        CFLAGS="-Ofast -fuse-linker-plugin -ftree-loop-if-convert-stores -march=native" \
        LDFLAGS="-march=native"
[ $? = 0 ] || exit $?
echo "                              ${GR}=> done.${N}" && sleep 3
if [ -z "$NPROC" ]; then
        NPROC=$(nproc 2>/dev/null)
        NPROC=${NPROC:-1}
fi

echo "${LYLO}building process please wait...${N}" && sleep 3

make -j $NPROC

if [ $? != 0 ]; then
        echo "                              ${LRD}ERROR...!!!"
        echo "${LRD}Compilation failed (make=$?)".
        echo "${LMA}Common causes: missing libjansson-dev libcurl4-openssl-dev libssl-dev"
        echo "${LYL}If you pulled updates into this directory, remove configure and try again.${N}"
        exit 1
fi
echo "                              ${GR}=> done.${N}" && sleep 3
echo "${LCYN} ls -l cpuminer${N}" && sleep 3
ls -l cpuminer

echo "${LYLO}stripping...${N}" && sleep 3
strip -s cpuminer

[ $? = 0 ] || exit $?
echo "                              ${GR}=> done.${N}"
