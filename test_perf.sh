mkdir -p build && cd build && cmake .. && make -j$(nproc) Cytopia_Tests
./bin/Cytopia_Tests "[simulation]"
