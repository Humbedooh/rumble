add=""
echo "----------------------------------------"
echo "Initializing build process"
echo "----------------------------------------"
echo -n "Checking architecture..."
if [ `arch` == "x86_64" ]; then
	echo "64 bits"
	add="-fPIC"
else
	echo "32 bits"
fi
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
		for l in `locate --limit=2 lib$lib`; do  files=`expr $files + 1`; done
	fi
	if [ $files -lt 2 ]; then
		echo "Couldn't find lib$lib!";
		exit
	fi
	echo "[OK]"
done

echo -n "Checking lua library name..."
vone=0
vtwo=0
llua=""
for l in `locate liblua5.1`; do vone=`expr $vone + 1`; done
for l in `locate liblua5.2`; do vtwo=`expr $vtwo + 1`; done
if [ $vtwo -gt 1 ]; then
	echo "found liblua5.2.a"
	llua="5.2"
elif [ $vone -gt 1 ]; then
	echo "found liblua5.1.a"
	llua="5.1"
else
	echo "found liblua.a"
fi

echo
echo "----------------------------------------"
echo "Setting up build environment"
echo "----------------------------------------"

echo "Copying some headers I found..."
for luah in lua.h lualib.h lauxlib.h; do
	original=`locate --limit=1 /$luah`
	cp "$original" src/
done

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
	echo   "$f.c"
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
		echo "Building: $d...";
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

echo "All done!!"
