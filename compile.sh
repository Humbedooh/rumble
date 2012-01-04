#!/bin/bash
add=""
echo "----------------------------------------"
echo "Initializing build process"
echo "----------------------------------------"
printf "%-32s"  "Checking architecture..."
if [ `arch` == "x86_64" ]; then
	echo "[33m64 bit[0m"
	add="-fPIC"
else
	echo "[33m32 bit[0m"
fi


printf "%-32s"  "Checking platform..."
os=`uname -o`
echo "[33m$os[0m"


#Check for where
if where; then
    where="where /Q"
else
	where="whereis"
fi

#Check for 'whereis'
if $where whereis; then
	haveWhereis=1
else
	haveWhereis=0
fi


#Check for 'wget'
if $where wget; then
	haveWget=1
else
	haveWget=0
fi

#Check for 'locate'
if $where locate; then
	haveLocate=1
else
	haveLocate=0
fi



printf "%-32s" "Checking for gcc..."
gccver=`gcc -dumpversion`
if [ $? -ne 0 ]; then
	echo "gcc wasn't found, please install it!"
	exit
else
	echo "[33m$gccver[0m"
fi

printf "%-32s" "Checking for g++..."
gppver=`g++ -dumpversion`
if [ $? -ne 0 ]; then
	echo "g++ wasn't found, rumblectrl will not be compiled."
	havegpp=0
else
	echo "[33m$gccver[0m"
	havegpp=1
fi


#apt and yum checks
printf "%-32s" "Checking for apt..."
haveApt=`apt-get -v`
if [ $? -ne 0 ]; then
	echo "[31mNO[0m"
	haveApt=0
else
	echo "[32mYES[0m"
	haveApt=1
fi

printf "%-32s" "Checking for yum..."
haveYum=`yum -v`
if [ $? -ne 0 ]; then
	echo "[31mNO[0m"
	haveYum=0
else
	echo "[32mYES[0m"
	haveYum=1
fi


# SQL database support 
cmysql=0
libmysql=""
while true
do
	WISH=0
	read -r -n1 -p "Do you wish to compile with mysql support? [[32mY[0m]es/[N]o:" WISH
	echo ""
	
	case $WISH in
		y|Y|"") cmysql=1; break ;;
		n|N) break ;;
		*) 
	esac
done

echo "----------------------------------------"
echo "Checking library availability"
echo "----------------------------------------"
for lib in sqlite3 gnutls gcrypt ssl pthread crypto lua resolv; do
	files=0
	msg="Checking for lib$lib..."
	#echo -n $msg
	printf "%-32s" "$msg"
	if [ $haveWhereis -eq 1 ]; then
		for l in `whereis lib$lib`; do  files=`expr $files + 1`; done
	fi
	if [ $files -lt 2 ]; then
		if [ $haveLocate -eq 1 ]; then
			for l in `locate -l 2 lib$lib`; do  files=`expr $files + 1`; done
		fi
	fi
	if [ $files -lt 2 ]; then
		echo "[31m[BAD][0m"
		echo "Couldn't find lib$lib, attempting to install it.";
		if [ $haveApt -gt 0 ]; then
			apt-get install libgnutls-dev libsqlite3-dev liblua5.1 libssl0.9.8
		else
			if [ $haveYum -gt 0 ]; then
				yum install libgnutls-dev libsqlite3-dev liblua5.1 libssl0.9.8
			fi
		fi
	fi
	echo "[32m[OK][0m"
done

printf "%-32s" "Checking lua library version..."
vone=0
vtwo=0
llua=""

for l in `locate liblua5.1`; do vone=`expr $vone + 1`; done
for l in `locate liblua5.2`; do vtwo=`expr $vtwo + 1`; done
if [ $vtwo -gt 1 ]; then
	echo "[33m5.2[0m"
	llua="5.2"
elif [ $vone -gt 1 ]; then
	echo "[33m5.1[0m"
	llua="5.1"
else
	echo "[33mgeneric[0m"
fi

echo "----------------------------------------"
echo "Checking library headers"
echo "----------------------------------------"
for lib in sqlite3 gnutls openssl pthread crypto lua; do
	files=0
	msg="Checking for lib$lib-dev..."
	#echo -n $msg
	printf "%-32s" "$msg"
	if [ $haveWhereis -eq 1 ]; then
		for l in `whereis /$lib.h | grep "\.h"`; do  files=`expr $files + 1`; done
	fi
	if [ $files -lt 2 ]; then
		files=0
		if [ $haveLocate -eq 1 ]; then
			for l in `locate -l 2 /$lib.h`; do  files=`expr $files + 1`; done
		fi
	fi
	if [ $files -lt 1 ]; then
		echo "[31m[BAD][0m"
		echo "Couldn't find lib$lib-dev, I'll try installing it for you.";
		if [ $haveApt -eq 1 ]; then
			apt-get install libgnutls-dev libsqlite3-dev liblua5.1 libssl0.9.8
		fi
		if [ $haveYum -eq 1 ]; then
			yum install libgnutls-dev libsqlite3-dev liblua5.1 libssl0.9.8
		fi
	fi
	echo "[32m[OK][0m"
done

echo
echo "----------------------------------------"
echo "Setting up build environment"
echo "----------------------------------------"

echo "Copying required header files..."
heads=0
for luah in lua.h lualib.h lauxlib.h luaconf.h; do
	original=`locate -l 1 /$luah`
	if [ "$original" ]; then
		cp "$original" src/
		cp "$original" /usr/include/
echo cp "$original" src/
	else
		echo "Couldn't find lua.h, please install the liblua5.1-dev package."
		exit
	fi
done
iscyg=`expr "$os" = "Cygwin"`
if [ $iscyg -ne 0 ]; then
	echo "Downloading Cygwin-specific components..."
	if [ $haveWget -eq 1 ]; then
		wget -O src/ns_parse.c http://rumbleserver.sourceforge.net/ns_parse.c
	else
		echo "Hmm, you don't have wget installed, and I kind of need it on Cygwin platforms"
		echo "Please install it before trying to compile again."
		exit
	fi
fi


if [ ! -e "src/radb/radb.h" ]; then
	if [ $haveWget -eq 1 ]; then
		echo "Downloading RADB package..."
		mkdir src/radb
		wget --no-check-certificate -O src/radb/radb.c https://github.com/Humbedooh/radb/raw/master/radb.c
		wget --no-check-certificate -O src/radb/radb.h https://github.com/Humbedooh/radb/raw/master/radb.h
		wget --no-check-certificate -O src/radb/radb.cpp https://github.com/Humbedooh/radb/raw/master/radb.cpp
	else
		echo "You don't have the RADB files in the src/radb folder, and I need wget to fetch them :("
		echo "Please get RADB from https://github.com/Humbedooh/radb or install wget"
		exit
	fi
fi

if [ $cmysql -ne 0 ]; then
	echo "Copying RADB for MySQL+SQLite"
	cp src/radb/* src/
	libmysql="-lmysqlclient_r"
else
	echo "Copying RADB for SQLite only"
	cat src/radb/radb.h | grep -v mysql.h > src/radb/radb.h2
	mv -f src/radb/radb.h2 src/radb/radb.h
	cp src/radb/radb.* src/
fi


echo "Making build directory..."
mkdir -p build
mkdir -p build/modules

echo
echo "----------------------------------------"
echo "Compiling individual files"
echo "----------------------------------------"
l=""
for f in src/*.c
do
	f=${f/.c/}
	f=${f/src\//}
	l="$l build/$f.o"
	echo   "[36m$f.c[0m"
	gcc    $add -c -O2 -Wall -MMD -MP -MF build/$f.o.d -o build/$f.o src/$f.c  -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua$llua -lresolv $libmysql
done

echo
echo "----------------------------------------"
echo "Building library and server"
echo "----------------------------------------"
gcc -o build/rumble $l -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua$llua -lresolv
if [[ $? -ne 0 ]]; then
	echo "An error occured, trying to compile with static linkage instead";
	gcc -static -o build/rumble $l -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua$llua -lresolv
	if [[ $? -ne 0 ]]; then
		echo "Meh, that didn't work either - giving up!"
		exit
	fi
fi

ar -rvc build/librumble.a $l 
ranlib build/librumble.a


if [ $havegpp -eq 1 ]; then
	echo
	echo "----------------------------------------"
	echo "Compiling rumblectrl"
	echo "----------------------------------------"
	l=""
	for f in src/rumblectrl/*.cpp
	do
		f=${f/.cpp/}
		f=${f/src\/rumblectrl\//}
		l="$l build/$f.opp"
		echo   "[36m$f.c[0m"
		g++    $add -c -O2 -Wall -MMD -MP -MF build/$f.o.d -o build/$f.opp src/rumblectrl/$f.cpp  -lsqlite3 $libmysql
	done

	g++ -o build/rumblectrl $l -lsqlite3
	if [[ $? -ne 0 ]]; then
		echo "An error occured, trying to compile with static linkage instead";
		g++ -static -o build/rumblectrl $l -lsqlite3
		if [[ $? -ne 0 ]]; then
			echo "Meh, that didn't work either - giving up!"
			
		fi
	fi
fi


echo
echo "----------------------------------------"
echo "Compiling standard modules"
echo "----------------------------------------"

for d in src/modules/*
do
	l=""
	d=${d/src\/modules\//}
	if [ "$d" != "rumblelua" ]; then
		echo "Building: [36m$d[0m";
		mkdir -p "build/modules/$d"
		for f in src/modules/$d/*.c
		do
			f=${f/src\/modules\/$d\//}
			f=${f/.c/}
			l="$l build/modules/$d/$f.o"
			gcc  $add -c -O3 -s  -MMD -MP -MF build/modules/$d/$f.o.d -o build/modules/$d/$f.o src/modules/$d/$f.c
		done
		gcc  $add -shared -o build/modules/$d.so -s $l build/librumble.a  -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua$llua -lresolv
		rm -rf build/modules/$d
	fi
done

echo
echo "----------------------------------------"
echo "Finalizing the build process"
echo "----------------------------------------"

echo "Creating the final folders and scripts"
cp -r src/modules/rumblelua build/modules/rumblelua
cp -r config build/config
mkdir build/db

echo "Cleaning up..."
rm -r build/*.o*

echo "[32mAll done![0m"
echo "Everything you need has been placed in the build/ folder."
