![logo](doc/flecsph_logo_bg.png)

# SPH on FleCSI 

This project is an implementation of SPH problem using FleCSI framework.

# Getting the Code 

    % git clone --recursive git@gitlab.lanl.gov/laristra/FleCSI_SPH.git

# Requirements

Before trying to compile you need to install on your system: 

- FleCSI thrird party
- FleCSI = compile legion

The installation steps are the followings: 

On DARWIN load the modules: 

    % module load gcc/6.2.0
    % module load openmpi/2.0.1-gcc_6.2.0
    % module load git/2.8.0
    % module load cinch/cinch-utils
    % module load cmake/3.7.1

Then install the flecsi third libraries

    % git clone --recursive git@github.com:laristra/flecsi/flecsi-third-party.git
    % mkdir build ; cd build
    % ccmake ../

Here have all ON except: 
- ENABLE_EXODUS OFF 
- ENABLE_SCOTCH OFF
- METIS_INT64 OFF
- USE_SYSTEM_LIBS OFF
- GASnet_CONDUIT mpi
and set a path for the CMAKE_INSTALL_PREFIX like /home/XXX/local/

And

    % make ; make install 

Then you will have to install FleCSI. 

    % git clone --recursive git@github.com:laristra/flecsi/flecsi.git

here we need to change to the refractor branch 

    % git checkout refractor 
    % mkdir build ; cd build 
    % ccmake ../

Here add ENABLE_MPI and ENABLE_OPENMP 
Set FLECSI_RUNTIME_MODEL legion
and set a path for CMAKE_INSTALL_PREFIX like /home/XXX/local/

    % make ; make install 

Then download and build FleCSPH.


# Build 

    % mkdir build
    % cd build 
    % ccmake ../ 
    % make 


# Todo 
- Handle the file name from the specialization driver to the mpi task 
- Add non power of two handler in the branch sharing
- See the code structuration 
