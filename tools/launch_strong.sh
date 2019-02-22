#!/bin/bash

# NUMBER OF PARTICLES SQUARED
START_NPART=500

# OMP THREADS PER MPI PROCESS
OMP_THREADS=18
CORES=18

# max 6=32 7=64 8=128 9=256 10=512
num=1
max=10

MAX_NODES=32
MAX_PROC=64

NITER=50
NOUTPUT=1000

dir="./strong_$START_NPART"
rm -rf $dir
mkdir $dir
cd $dir

NPARTS=$(bc <<< "scale=0; $START_NPART")

cat > task.sh <<EOL
#!/bin/bash" >> task.sh
#SBATCH --time=10:00:00
#SBATCH -N $MAX_NODES
#SBATCH -n $MAX_PROC
#SBATCH -c $CORES
#SBATCH --output=out
#SBATCH --error=err

export CLOG_ENABLE_STDLOG=1
export OMP_NUM_THREADS=$OMP_THREADS

time mpirun -n 1 ../../../bin/id_generators/sodtube_3d_generator ./sodtube_n$NPARTS.par
EOL

# Writing the data file
config="sodtube_n$NPARTS.par"
cat > $config <<EOL
  #
  # Sodtube test
  #
  # initial data
  initial_data_prefix = "ev_st_n$NPARTS"
  lattice_nx = $NPARTS        \# particle lattice linear dimension
  poly_gamma = 1.4        \# polytropic index
  equal_mass = yes        \# determines whether equal mass particles are used or equal separation
  sph_eta = 1.2
  lattice_type = 1        \# 0:rectangular, 1:hcp, 2:fcc"
  # evolution
  sph_kernel = "Wendland C6"
  initial_dt = 1.0
  sph_variable_h = yes
  adaptive_timestep = yes
  timestep_cfl_factor = 0.25
  initial_iteration = 0
  final_iteration = $NITER
  out_screen_every = 1
  out_scalar_every = 1
  out_h5data_every = $NOUTPUT
  out_diagnostic_every = 1
  output_h5data_prefix = "ev_st_n$NPARTS"
  external_force_type="walls:xyz"
  zero_potential_poison_value = 0.0
  extforce_wall_steepness = 1e12
  extforce_wall_powerindex = 5
EOL

for i in `seq 1 $max`; do
  cat >> task.sh <<EOL

echo "Computation: $num"
time mpirun -n $num --bind-by socket --map-by socket ../../../bin/drivers/hydro_3d ./sodtube_n$NPARTS.par
echo "Done $num"

EOL
  sbatch task.sh
  num=$(($((2*$num))))
done

ls