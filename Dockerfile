FROM debian:stretch-slim

RUN apt-get update && apt-get install -y vim git make g++ zlib1g-dev libosmpbf-dev libprotobuf-dev
RUN ldconfig

RUN mkdir /data
VOLUME /data

RUN mkdir /app
VOLUME /app
#RUN git clone https://github.com/no-go/SwappyItems.git /app

WORKDIR /app

RUN echo "run me e.g.: docker run -ti -v \$(pwd)/../data:/data -v \$(pwd):/app IMAGE_OR_CONTAINER bash"
#RUN make
