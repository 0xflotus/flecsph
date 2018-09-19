![logo](doc/flecsph_logo_bg.png)

[![Build Status](https://travis-ci.org/laristra/flecsph.svg?branch=master)](https://travis-ci.org/laristra/flecsph)
[![codecov.io](https://codecov.io/github/laristra/flecsph/coverage.svg?branch=master)](https://codecov.io/github/laristra/flecsph?branch=master)
<!---
[![Quality Gate](https://sonarqube.com/api/badges/gate?key=flecsph%3A%2Fmaster)](https://sonarqube.com/dashboard?id=flecsph%3A%2Fmaster)
--->

# SPH on FleCSI

This project is an implementation of SPH problem using FleCSI framework.
This code intent to provide distributed and parallel implementation of the octree data structure provide by FleCSI.
The Binary, Quad and Oct tree respectively for 1, 2 and 3 dimensions is developed here for Smoothed Particle Hydrodynamics problems.

For the first version of the code we intent to provide several basic physics problems:

- Sod Shock Tube Tests in 1D
- Sedov Blast Wave 2D
- Binary Compact Object Merger 3D

You can find detail ingredients of FleCSPH such as formulations and algorithm under `doc/notes.pdf`. The document is constantly updated to contain latest development of FleCSPH

# Getting the Code

FleCSPH can be installed anywhere in your system; to be particular, below we
assume that all repositories are downloaded in FLECSPH root directory `${HOME}/FLECSPH`.
The code requires:

- FleCSI third party library
- FleCSI
- shared local directory

### Suggested directory structure

```{engine=sh}
   mkdir -p $HOME/FLECSPH/local
   cd $HOME/FLECSPH
   git clone --recursive git@github.com:laristra/flecsph.git
```    

```{engine=sh}
  ${HOME}/FLECSPH
  ├── flecsi
  │   └── build
  ├── flecsi-third-party
  │   └── build
  ├── flecsph
  │   ├── build
  │   └── third-party-libraries
  │       ├── install_h5hut.sh
  │       └── install_hdf5_parallel.sh
  └── local
      ├── bin
      ├── include
      ├── lib
      ├── lib64
      └── share
```

In this configuration, all dependencies are installed in `${HOME}/FLECSPH/local`.

## Install the dependencies

You will need the following tools:

- gcc version > 6.2;
- MPI library: openmpi or mpich, compiled with the gcc compiler above; and with `--enable-mpi-thread-multiple`;
- cmake version > 3.7
- boost library


On DARWIN supercomputer load the modules:

    % module load gcc/6.2.0 openmpi/2.0.1-gcc_6.2.0 git/2.8.0 cmake/3.7.1 boost/1.59.0_gcc-6.2.0

### FleCSI third part libraries

Clone the FleCSI repo with third party libraries and check out FleCSPH-compatible branch `flecsph`:

```{engine=sh}    
   cd $HOME/FLECSPH
   git clone --recursive git@github.com:laristra/flecsi-third-party.git
   cd flecsi-third-party
   git checkout FleCSPH
   git submodule update
   mkdir build ; cd build
   ccmake ..
```    

Let all the flags ON, make sure the conduit of GASNET is MPI.
Set `CMAKE_INSTALL_PREFIX -> ~/FLECSPH/local`.
Build the libraries using several cores (note that no install step is required):

    % make -j8

### FleCSI

Clone FleCSI repo and change to FlecSPH branch:

```{engine=sh}    
   cd $HOME/FLECSPH
   git clone --recursive git@github.com:laristra/flecsi.git
   cd flecsi
   git checkout stable/flecsph-compatible
   git submodule update
   mkdir build ; cd build
   ccmake ..
```    

Press `c` to do initial configuring.
- Set `CMAKE_INSTALL_PREFIX -> ~/FLECSPH/local`.

Press `c` to reconfigure. Press `t` for advanced options; scroll down to select:
Here add:
- `ENABLE_MPI`: ON
- `ENABLE_MPI_CXX_BINDINGS`: ON
- `ENABLE_OPENMP`: ON
- `ENABLE_LEGION`: ON
- `FLECSI_RUNTIME_MODEL`: legion

Press `c` to reconfigure and `g` to generate configurations scripts.

You can also supply CMakeCache.txt to avoid multiple reconfigures:

```{engine=sh}
cat > CMakeCache.txt << EOF
  CMAKE_INSTALL_PREFIX:PATH=$HOME/FLECSPH/local
  ENABLE_MPI:BOOL=ON
  ENABLE_MPI_CXX_BINDINGS:BOOL=ON
  ENABLE_OPENMP:BOOL=ON
  ENABLE_LEGION:BOOL=ON
  FLECSI_RUNTIME_MODEL:STRING=legion
EOF
ccmake ..
```

If no errors appeared, build and install:

    % make -j8 install

In case of errors: if you are rebuilding everything from scratch, 
make sure that your installation directory (`$HOME/FLECSPH/local` 
in our example) is empty.

# Build

## Dependencies

In order to build flecsph some other dependencies can be found in the third-party-libraries/ directory.
- Use the scripts to install HDF5 and H5Hut from within build/ directory:

```{engine=sh}
   cd ~/FLECSPH/flecsph
   mkdir build; cd build
   ../third-party-libraries/install_hdf5_parallel.sh
   ../third-party-libraries/install_h5hut.sh
```    

- ScalingFramework is available in LANL property right now, soon open-source

## Build FleCSPH

Continue with the build:

    % ccmake ..

Set the following options:
- `CMAKE_INSTALL_PREFIX`: ~/FLECSPH/local
- `ENABLE_MPI`: ON
- `ENABLE_OPENMPI`: ON
- `ENABLE_LEGION`: ON
- `ENABLE_UNIT_TESTS`: ON
- `HDF5_C_LIBRARY_hdf5`: `~/FLECSPH/local/lib/libhdf5.so`

You can also use the following command to setup cmake cache:

```{engine=sh}
cat > CMakeCache.txt << EOF
  CMAKE_INSTALL_PREFIX:PATH=$HOME/FLECSPH/local
  ENABLE_LEGION:BOOL=ON
  ENABLE_MPI:BOOL=ON
  ENABLE_MPI_CXX_BINDINGS:BOOL=ON
  ENABLE_OPENMP:BOOL=ON
  ENABLE_UNIT_TESTS:BOOL=ON
  HDF5_C_LIBRARY_hdf5:FILEPATH=$HOME/FLECSPH/local/lib/libhdf5.so
  VERSION_CREATION:STRING=
EOF
ccmake ..
```

Configure, build and install:

    % make -j8 install

### Building FleCSPH on various architectures
Architecture-/machine-specific notes for building FleCSPH are collected in
[doc/machines](https://github.com/laristra/flecsph/tree/master/doc/machines)
We appreciate user contributions.

# Runnig existing applications
Current FleCSPH contains several initial data generators and two evolution
drivers: `hydro` and `newtonian`. Initial data generators are located in
`app/id_generators/`:

- `sodtube`: 1D/2D/3D sodtube shock test;
- `sedov`: 2D and 3D Sedov blast wave;
- `noh`: 2D and 3D Noh implosion test.

Evolution drivers are located in `app/drivers`:

- `hydro`: 1D/2D/3D hydro evolution without gravity;
- `newtonian`: 3D hydro evolution with self-gravity.

To run a test, you also need a parameter file, specifying parameters of the
problem. Parameter files are located in `data/` subdirectory. Running an 
application consists of two steps:

- generating intitial data;
- running evolution code.

For instance, to run a `sodtube` 1D shock test, do the following (assuming
you are in your build directory after having successfully built FleCSPH):
```{engine=sh}
  cp ../data/sodtube_t1_n1000.par sodtube.par
  # edit the file sodtube.par to adjust the number of particles etc.
  app/id_generators/sodtube_1d_generator  sodtube.par
  app/driver/hydro_1d sodtube.par
```

# Creating your own initial data or drivers
You can add your own initial data generator or a new evolution module under
`app/id_generators` or `app/drivers` directories. Create a directory with 
unique name for your project and modify CMakeLists.txt to inform the cmake
system that your project needs to be built. 

A new initial data generator usually has a single `main.cc` file and an optional
include file. You can use existing interfaces for lattice generators or equations 
of state in the `include/` directory. 
The file `app/drivers/include/user.h` defines the dimensions of your problem, both
for initial data generators and for the evolution drivers. 
This is done via a compile-time macro `EXT_GDIMENSION`, which allows users to have
the same source code for different problem dimensions. Actual dimension is set at 
compile time via the `target_compile_definitions` directive of cmake, e.g.:
```
   target_compile_definitions(sodtube_1d_generator PUBLIC -DEXT_GDIMENSION=1)
   target_compile_definitions(sodtube_2d_generator PUBLIC -DEXT_GDIMENSION=2)
```

A new evolution driver must have a `main.cc` and `main_driver.cc` files. Do not edit 
`main.cc`, because FleCSI expects certain format of this file. It is easier to start 
by copying existing files to your folder under `app/drivers`. Include cmake 
targets with different dimensions using examples in `app/drivers/CMakeLists.txt`.

Make sure to document your subproject in a corresponding `README.md` file 
that describes the problem you want to run. In order to get all files easily and 
correctly, you can copy them from other subprojects such as `sodtube` or `hydro`.

# Style guide

FleCSPH follows the FleCSI coding style, which in turn follows (in general) the Google coding conventions.
FleCSI coding style is documented here:
https://github.com/laristra/flecsi/blob/master/flecsi/style.md

# Logs 

Cinch Log is the logging tool for this project. 
In order to display log set the environement variable as: 
```bash 
export CLOG_ENABLE_STDLOG=1
```

In the code, you can set the level of output from trace(0) and info(1) to warn(2), error(3) and fatal(4).
You can then control the level of output at compile time by setting the flag `CLOG_STRIP_LEVEL`:
by default it is set to 0 (trace), but for simulations it is perhaps preferrable to set it to 1 (info).
In the code, if you only want a single rank (e.g. rank 0) to output, you need to use `clog_one` command,
and indicate the output rank using the `clog_set_output_rank(rank)` macro right after MPI initialization:
```cpp
clog_set_output_rank(0);
clog_one(info) << "This is essential output (level 1)" << std::endl;
clog_one(trace) << "This is verbose output  (level 0)" << std::endl;
clog_one(fatal) << "Farewell!" << std::endl; 
```

For further details, refer to the documentation at: 
https://github.com/laristra/cinch/blob/master/logging/README.md

 # Contacts

 If you have any questions or find any worries about FleCSPH, please contact Julien Loiseau (julien.loiseau@univ-reims.fr) and/or Hyun Lim (hylim1988@gmail.com)

