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

printf "%-32s" "Checking for gcc..."
gccver=`gcc -dumpversion`
if [ $? -ne 0 ]; then
	echo "gcc wasn't found, please install it!"
	exit
else
	echo "[33m$gccver[0m"
fi

# SQL database support 
cmysql=0
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
	for l in `whereis lib$lib`; do  files=`expr $files + 1`; done
	if [ $files -lt 2 ]; then
		for l in `locate -l 2 lib$lib`; do  files=`expr $files + 1`; done
	fi
	if [ $files -lt 2 ]; then
		echo "[31m[BAD][0m"
		echo "Couldn't find lib$lib, please install it before compiling.";
		exit
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
	for l in `whereis /$lib.h | grep "\.h"`; do  files=`expr $files + 1`; done
	if [ $files -lt 2 ]; then
		files=0
		for l in `locate -l 2 /$lib.h`; do  files=`expr $files + 1`; done
	fi
	if [ $files -lt 1 ]; then
		echo "[31m[BAD][0m"
		echo "Couldn't find lib$lib-dev, please install it before compiling.";
		exit
	fi
	echo "[32m[OK][0m"
done

echo
echo "----------------------------------------"
echo "Setting up build environment"
echo "----------------------------------------"

echo "Copying required header files..."
heads=0
for luah in lua.h lualib.h lauxlib.h; do
	original=`locate -l 1 /$luah`
	if [ "$original" ]; then
		cp "$original" src/
echo cp "$original" src/
	else
		echo "Couldn't find lua.h, please install the liblua5.1-dev package."
		exit
	fi
done
iscyg=`expr "$os" = "Cygwin"`
if [ $iscyg -ne 0 ]; then
	echo "Downloading Cygwin-specific components..."
	wget -O src/ns_parse.c http://rumbleserver.sourceforge.net/ns_parse.c
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
	gcc    $add -c -O2 -Wall -MMD -MP -MF build/$f.o.d -o build/$f.o src/$f.c  -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua$llua -lresolv 
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
