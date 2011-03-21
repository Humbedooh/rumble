
mkdir -p build
mkdir -p build/modules
l=""
for f in src/*.c
do
	f=${f/.c/}
	f=${f/src\//}
	l="$l build/$f.o"
	echo   "gcc -c -O2 -Wall -MMD -MP -MF build/$f.o.d -o build/$f.o src/$f.c"
	gcc    -c -O2 -Wall -MMD -MP -MF build/$f.o.d -o build/$f.o src/$f.c  -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua -lresolv
done



ar -rv build/librumble.a $l 
ranlib build/librumble.a
gcc -o build/rumble.exe $l -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua -lresolv


for d in src/modules/*
do
	l=""
	d=${d/src\/modules\//}
	if [ "$d" != "rumblelua" ]; then
		mkdir -p "build/modules/$d"
		for f in src/modules/$d/*.c
		do
			f=${f/src\/modules\/$d\//}
			f=${f/.c/}
			echo $f;
			l="$l build/modules/$d/$f.o"
			echo   "gcc  -c -O3 -s  -MMD -MP -MF build/modules/$d/$f.o.d -o build/modules/$d/$f.o src/modules/$d/$f.c"
			gcc   -c -O3 -s  -MMD -MP -MF build/modules/$d/$f.o.d -o build/modules/$d/$f.o src/modules/$d/$f.c
		done
		gcc  -shared -o build/modules/$d.so -s $l build/librumble.a  -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua -lresolv
		rm -rf build/modules/$d
	fi
done

rm -r build/*.o*
