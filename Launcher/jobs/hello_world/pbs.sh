#!/bin/bash
#LAU generated_on = 2015-11-4:15:29:48
#LAU user =  
#LAU cluster = Hopper
#LAU nodeset = Hopper-thin-nodes (58 Gb/node; 20 cores/node)
#LAU n_cores = 20
#LAU Gb_per_core = 2.9
#LAU Gb_total = 58
#LAU local_folder = unknown
#LAU remote_folder = unknown
#PBS -l nodes=1:ppn=20
#PBS -l walltime=1:00:00
#PBS -M engelbert.tijskens@uantwerpen.be
#PBS -m a
#PBS -W x=nmatchpolicy:exactnode
#PBS -N hello_world
#
#--- shell commands below ------------------------------------------------------
# HELLO WORLD example
# Submitting this job will create a 'hello_world' directory in the remote file
# location (typically your scratch space $VSC_SCRATCH). If all goes well during
# execution, you will there find an 'output' directory with a file 'hello_world.txt'.
# When you retrieve the job, you will find also finds these directories and files
# in the local file location.
#
#make the directory where the job was submitted the current working directory
cd $PBS_O_WORKDIR
#remove the 'output' directory and all its contents. Do not complain if it does not exist.
rm -rf output
#make an empty 'output' directory
mkdir output
#create an output file 'hello_world.txt' in the 'output' directory and write some data
echo "this is the output file of job" > output/hello_world.txt
echo $PBS_JOBID                       >>output/hello_world.txt
touch $PBS_O_WORKDIR/finished.$PBS_JOBID #Launcher needs this to be the LAST line. Do NOT move or delete!
