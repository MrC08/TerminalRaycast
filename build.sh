cmake . -DCMAKE_BUILD_TYPE=Debug

if make
then
	echo ""
	echo ""
	echo ""
	./terminalraycast $*
fi