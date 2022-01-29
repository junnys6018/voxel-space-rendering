flags = -O3 -s EXPORT_ES6 -s MODULARIZE -s ENVIRONMENT='web' -s WASM=1

all:
	emcc voxel_space.c ${flags} -o voxel_space.js --preload-file maps \
	-s EXPORTED_FUNCTIONS='["_create_voxel_space_context", "_destroy_voxel_space_context", "_render_voxel_space"]' \
	-s EXPORTED_RUNTIME_METHODS='["cwrap"]'