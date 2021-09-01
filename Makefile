all:
	emcc voxel_space.c -O3 -o voxel_space.js --preload-file maps \
	-s EXPORTED_FUNCTIONS='["_create_voxel_space_context", "_destroy_voxel_space_context", "_render_voxel_space"]' \
	-s EXPORTED_RUNTIME_METHODS='["cwrap"]'