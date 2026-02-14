The Configs.scala file is found in chipyard/generators/gemmini/src/main/scala/gemmini/Configs.scala . It contains the default configs alongside with the other configs that came with the original git repo.

I added the uclaConfig at line 251 and line 400. But I ended up just using the modified defaultConfig.

To build this config you just have to run make CONFIG=GemminiRocketConfig at the chipyard/sims/verilator directory.

Next create a directory at chipyad/gemmini/software/gemmini-rocc-tests/ucla_parallel_sim .

The test that I am trying to run is called parallel_grid.c located here chipyard/gemmini/software/gemmini-rocc-tests/ucla_parallel_sim/parallel_grid.c . This test is supposed to do a software simulation of a 7x7 SA each with 64 x 64 PE. 

To build the binaries for this test file we have to create a Makefile at # chipyard/generators/gemmini/software/gemmini-rocc-tests/ucla_parallel_sim/Makefile. (In the folder) Then run make parallel_grid-baremetal abs_top_srcdir=.. at /chipyard/generators/gemmini/software/gemmini-rocc-tests/ucla_parallel_sim

We also have to modify the Makefile at chipyard/generators/gemmini/software/gemmini-rocc-tests/Makefile.in (This is in the folder)

To run the test type: make CONFIG=GemminiRocketConfig run-binary     BINARY=../../generators/gemmini/software/gemmini-rocc-tests/ucla_parallel_sim/parallel_grid-baremetal at chipyard/sims/verilator

The last file is just a modified version of the template.c that is used to run make CONFIG=GemminiRocketConfig run-binary BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal at chipyards/sim/verilator (This is the command in the Run Simulators section of the GitHub) 
