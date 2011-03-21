
echo "Looking for Lua5.1"
v=0
llua=""
for l in `whereis liblua51`; do v=`expr $v + 1`; done
echo $v
if [ $v -gt 1 ]; then
	echo "Found lua5.1!"
	llua="5.1"
	cp /usr/include/lua5.1/*.h src/
else
	echo "Not found, assuming it's just called Lua then"
	cp -p /usr/include/lua/*.h src/
fi
mkdir -p build
mkdir -p build/modules
l=""
for f in src/*.c
do
	f=${f/.c/}
	f=${f/src\//}
	l="$l build/$f.o"
	echo   "gcc -c -O2 -Wall -MMD -MP -MF build/$f.o.d -o build/$f.o src/$f.c"
	gcc    -c -O2 -Wall -MMD -MP -MF build/$f.o.d -o build/$f.o src/$f.c  -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua$llua -lresolv 
done




echo "gcc -o build/rumble.exe $l -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua"
gcc -r -o build/rumble.exe $l -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua$llua

ar -rv build/librumble.a $l 
ranlib build/librumble.a

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
		gcc  -shared -o build/modules/$d.so -s $l build/librumble.a  -lsqlite3 -lgnutls -lgcrypt -lssl -lpthread -lcrypto -llua$llua -lresolv
		rm -rf build/modules/$d
	fi
done

rm -r build/*.o*
