all: helloworld.wasm

helloworld.wasm: helloworld.wat
	wat2wasm helloworld.wat -o helloworld.wasm

clean:
	rm -f helloworld.wasm
