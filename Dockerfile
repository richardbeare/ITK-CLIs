################################################################################
# base system
################################################################################
FROM ubuntu:16.04 as system


################################################################################
# builder
################################################################################
FROM system as builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    git \
    ca-certificates `# essential for git over https` \
    build-essential

### cmake independent of distro version
RUN curl -s https://cmake.org/files/v3.11/cmake-3.11.4-Linux-x86_64.sh -o cmake.sh
RUN sh cmake.sh --prefix=/usr --exclude-subdir --skip-license

### ITK
RUN git clone -b v4.13.1 --depth 1 https://itk.org/ITK.git 

RUN mkdir -p ITK_build && \
    cd ITK_build && \
    cmake \
    	  -DCMAKE_INSTALL_PREFIX=/opt/itk/ \
	  -DCMAKE_MODULE_PATH=/opt/vtk/lib/cmake \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DBUILD_SHARED_LIBS=ON \
	  -DBUILD_TESTING=OFF \
	  -DModule_ITKVtkGlue=OFF \
	  -DModule_ITKReview=ON \
	  -DModule_LabelErodeDilate=ON \
	  -DModule_MinimalPathExtraction=ON \
	  -DModule_ParabolicMorphology=ON \
	  -DModule_StreamingSinc=ON \
	  -DModule_LesionSizingToolkit=OFF \
	  ../ITK && \
    make -j"$(nproc)" && \
    make -j"$(nproc)" install


### ITK-CLIs
RUN apt-get update && apt-get install -y --no-install-recommends \
    libprocps-dev

COPY . /code/

RUN mkdir -p /build/ && \
    cd /build/ && \
    cmake \
    	  -DCMAKE_INSTALL_PREFIX=/opt/ITK-CLIs/ \
	  -DCMAKE_PREFIX_PATH=/opt/itk/lib/cmake/ \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_CXX_STANDARD=11 \
	  -DCMAKE_CXX_FLAGS="-Wno-format ${CMAKE_CXX_FLAGS}" \
	  /code/ && \
    make -j"$(nproc)" && \
    make -j"$(nproc)" install


################################################################################
# install
################################################################################
FROM system as install

COPY --from=builder /opt/itk/ /opt/itk/
COPY --from=builder /opt/ITK-CLIs/ /opt/ITK-CLIs/

ENV PATH "/opt/ITK-CLIs/bin/:${PATH}"
