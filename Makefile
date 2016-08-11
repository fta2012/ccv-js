all: build/ccv.js

build/ccv.js: external/ccv/lib/libccv.a ccv_bindings.cpp ccv_lib.js
	emcc -I"./external/ccv/lib" -L"./external/ccv/lib" ccv_bindings.cpp -lccv -o build/ccv.js \
		-O2 \
		--bind \
		-s EXPORTED_RUNTIME_METHODS=[] \
		-s NO_EXIT_RUNTIME=1 \
		-s TOTAL_MEMORY=536870912 \
		--pre-js ccv_lib.js \
		--memory-init-file 0
		
external/ccv/lib/libccv.a:
	#source ~/Desktop/external/emsdk_portable/emsdk_env.sh
	git submodule update --init
	cd external/ccv/lib && emconfigure ./configure && emmake make libccv.a

clean:
	rm build/* || true
	cd external/ccv/lib && make clean

