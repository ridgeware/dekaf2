
BUILD_DIR    :=~/dekaf2-samples-cmake
CMAKE        :=${BUILD_DIR}/Makefile
CORES        :=4

help:
	@echo ""
	@echo " make - handy wrapper around cmake"
	@echo ""
	@echo " usage: make <options>"
	@echo ""
	@echo " where <options> are:"
	@echo "   all      :: normal build"
	@echo "   scratch  :: scratch build"
	@echo "   help     :: this message"
	@echo ""

all: ${CMAKE}
	@echo ":: BUILD_DIR = ${BUILD_DIR}"
	@${MAKE} -j${CORES} -C ${BUILD_DIR}
	@cp -pf ${BUILD_DIR}/statinfo ${CURDIR}/.

scratch:
	@rm -rf ${BUILD_DIR}
	@${MAKE} all

${CMAKE}: CMakeLists.txt
	@mkdir -p ${BUILD_DIR}
	@cd ${BUILD_DIR} && cmake ${CURDIR}

