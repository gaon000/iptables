BUILD_DIR := ./build
HEADER_DIR := ./header
SOURCE_DIR := ./source

all : iptables

iptables: main.o
		g++ -g -o ${BUILD_DIR}/iptables ${BUILD_DIR}/main.o -lnetfilter_queue
main.o: makeBuildFolder
		g++ -g -c -o ${BUILD_DIR}/main.o ${SOURCE_DIR}/main.cpp
makeBuildFolder:
				mkdir -p ${BUILD_DIR}
clean:
		rm -f ${BUILD_DIR}/*

