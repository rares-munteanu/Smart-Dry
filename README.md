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
g++ -o prog smart-dry.cpp -std=c++2a -lpistache
```

### Running
```
./prog
```
