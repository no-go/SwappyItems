FROM docker.pkg.github.com/no-go/swappyitems/swappy_items_diy:0.78
RUN apt-get update && apt-get install -y libstxxl-dev libstxxl1v5
#RUN git clone https://github.com/no-go/SwappyItems.git /app
WORKDIR /app
RUN echo "run me e.g.: docker run -ti -v \$(pwd)/../data:/data -v \$(pwd):/app 0ba9a25c6d70 bash"
#RUN make
