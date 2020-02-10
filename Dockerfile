FROM debian:stretch-slim
COPY Makefile /app/
COPY *.h /app/
COPY *.hpp /app/
COPY *.cpp /app/

# e.g. docker run -ti -v $(pwd)/../where/the/pbf_files/are:/data 27b1f6b270a7 bash
RUN mkdir /data
VOLUME /data

RUN apt-get update && apt-get install -y vim make g++ zlib1g-dev libosmpbf-dev libprotobuf-dev
RUN ldconfig
WORKDIR /app
RUN make
