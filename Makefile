ifneq ($(notdir $(CXX)), em++)
$(error You need to install/source emscripten and run with "emmake make")
endif

CXXFLAGS = -std=c++1z \
	--bind \
	--memory-init-file 0 \
	--pre-js ccv_pre.js \
	-s EXPORT_NAME=\"'CCVLib'\" \
	-s MODULARIZE=1 \
	-s NO_EXIT_RUNTIME=1 \
	-s TOTAL_MEMORY=$$((2 << 29))

CPPFLAGS = -I"./external/ccv/lib"

LDFLAGS = -L"./external/ccv/lib"

LDLIBS = -lccv


.PHONY: all release debug clean

all: release

release: CXXFLAGS += -O3 --llvm-lto 1 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s OUTLINING_LIMIT=10000 # TODO --closure 1
release: build/ccv.js build/ccv_without_filesystem.js

# TODO this target isn't tested and probably doesn't work
# Also you probably need to do `emmake make clean` before building debug if you've already built release
debug: CXXFLAGS += -v -g4 -s ASSERTIONS=1 -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=1
debug: CXXFLAGS += -Weverything -Wall -Wextra
debug: build/ccv.js build/ccv_without_filesystem.js

build/ccv.js: CXXFLAGS += -s NO_FILESYSTEM=0 -s FORCE_FILESYSTEM=1
build/ccv.js: CXXFLAGS += --embed-file external/ccv/samples/face.sqlite3@/
build/ccv.js: CXXFLAGS += --embed-file external/ccv/samples/pedestrian.m@/
build/ccv.js: CXXFLAGS += --embed-file external/ccv/samples/car.m@/
build/ccv.js: CXXFLAGS += --embed-file external/ccv/samples/pedestrian.icf@/
build/ccv.js: CPPFLAGS += -DWITH_FILESYSTEM
build/ccv.js: ccv_bindings.cpp external/ccv/lib/libccv.a ccv_pre.js
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) ccv_bindings.cpp -o $@ $(LDLIBS)
	echo "self.CCV = CCVLib(self.CCV || {});" >> build/ccv.js # Always create a global CCV module when script loads
	du -h build/ccv.js # show file size

build/ccv_without_filesystem.js: CPPFLAGS += -s NO_FILESYSTEM=1
build/ccv_without_filesystem.js: ccv_bindings.cpp external/ccv/lib/libccv.a ccv_pre.js
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) ccv_bindings.cpp -o $@ $(LDLIBS)
	echo "self.CCV = CCVLib(self.CCV || {});" >> build/ccv_without_filesystem.js # Always create a global CCV module when script loads
	du -h build/ccv_without_filesystem.js # show file size

external/ccv/lib/libccv.a:
	git submodule update --init
	cd external/ccv/lib && git checkout stable && emconfigure ./configure --without-cuda && emmake make libccv.a

clean:
	rm -f build/*
	#cd external/ccv/lib && make clean
