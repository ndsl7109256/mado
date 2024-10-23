# Update and install dependencies
sudo apt update
sudo apt install -y meson ninja-build libpixman-1-dev  zlib1g-dev libdrm-dev pkgconf

# Clone and build aml
git clone https://github.com/any1/aml.git
cd aml
meson build
ninja -C build
sudo ninja -C build install

# Clone and build NeatVNC
cd ..
git clone https://github.com/any1/neatvnc.git
cd neatvnc
git checkout v0.8
meson build
ninja -C build
sudo ninja -C build install
