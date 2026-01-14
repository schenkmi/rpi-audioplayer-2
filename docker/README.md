# Build instructions using Docker

## Prepare docker
The first step is to build the Docker image and create a container. You can specify a directory in your hosts system which will be mounted inside the container. This allows you to copy the built system easily. The current directory must contain the provided Dockerfile which will be compiled into an image. The directory called `build` will contain the sources and the images which will be built later.
```
cd rpi-audioplayer-2
docker build -t br-docker -f docker/Dockerfile .
docker create -it --name br-docker --mount type=bind,source="$(pwd)",destination=/home/br-user/br-docker br-docker
```

## Start docker
You can start the container if it was created successfully. This command gives you an interactive shell inside the container.
```
docker start -ia br-docker
```

## RPI3
```
cd ~/br-docker/buildroot
make PLATFORM=RPI3_64
```

## RPI4
```
cd ~/br-docker/buildroot
make PLATFORM=RPI4_64
```

## Prune all docker images
```
docker system prune -a
```


# Podman
```
sudo apt-get update
sudo apt-get -y install podman
```
