cd @

sudo apt update

# Build tools
sudo apt-get -y install git
sudo apt-get -y install ninja-build
sudo apt-get -y install g++
sudo apt-get -y install cmake

# Dependencies
sudo apt-get -y install libssl-dev
sudo apt-get -y install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-qt5 gstreamer1.0-pulseaudio
sudo apt-get -y install libboost-all-dev
sudo apt-get -y install libspdlog-dev

cd ..
git clone https://github.com/cesanta/mongoose
cd ./mongoose
