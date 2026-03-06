# SPEC MPI 2007 V 2.0.1

## 104.milc

Compile like lulesh for the TypeART instrumented and linked version with

```{.bash}
make TYPEART=yes
```


### Running the application

The application can be run with the reference data set (takes with 8 MPI processes ~12 minutes per run).

```{.bash}
mpirun -np X ./104.milc < data/mref_su3imp.in
```

In the data directory there is also mtest_se3imp.in and su3imp.in (which is the train set), which consume considerably less runtime.
