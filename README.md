# Smart-Dry

## Installation

### Build essentials

```
sudo apt update
sudo apt install build-essential
```

### Pistache

```
sudo add-apt-repository ppa:pistache+team/unstable
sudo apt update
sudo apt install libpistache-dev
```

### Nlohmann

```
sudo apt-get install nlohmann-json3-dev
```

### Compiling

```
g++ -o prog smart-dry.cpp -std=c++2a -lpistache -pthread
```

### Running

```
./prog
```

### mqtt
```
$ git clone https://github.com/eclipse/paho.mqtt.c.git
$ cd paho.mqtt.c
$ git checkout v1.3.8

$ cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON \
    -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON
$ sudo cmake --build build/ --target install
$ sudo ldconfig

$ git clone https://github.com/eclipse/paho.mqtt.cpp
$ cd paho.mqtt.cpp

$ cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON \
    -DPAHO_BUILD_DOCUMENTATION=FALSE -DPAHO_BUILD_SAMPLES=TRUE
$ sudo cmake --build build/ --target install
$ sudo ldconfig
```

### mosquitto
https://mosquitto.org/download/
```sudo apt-add-repository ppa:mosquitto-dev/mosquitto-ppa
sudo apt-get update
sudo apt install mosquitto
```

### compile mqtt version
```
g++ -std=c++17 smart-dry-mqtt.cpp -lpaho-mqttpp3 -lpaho-mqtt3as -lpthread
```

