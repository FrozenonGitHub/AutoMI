project(GraphAutoMI)

# ================ SSSP Variants ================
add_graphlab_executable(power_sssp powerlyra_sssp.cpp)
add_graphlab_executable(multi_sssp multilyra_sssp.cpp)
add_graphlab_executable(automi_sssp automi_sssp.cpp)
add_graphlab_executable(track_sssp track_sssp.cpp)

# ================ bBFS Variants ================
add_graphlab_executable(power_rch powerlyra_bbfs.cpp)
add_graphlab_executable(multi_rch multilyra_bbfs.cpp)
add_graphlab_executable(automi_rch automi_bbfs.cpp)
add_graphlab_executable(track_rch track_bbfs.cpp)

# ================ iBFS Variants ================
add_graphlab_executable(power_int powerlyra_ibfs.cpp)
add_graphlab_executable(multi_int multilyra_ibfs.cpp)
add_graphlab_executable(automi_int automi_ibfs.cpp)

# ================ Greedy Graph Coloring ================
add_graphlab_executable(power_gc single_graph_coloring.cpp)
add_graphlab_executable(track_gc track_graph_coloring.cpp)
add_graphlab_executable(automi_gc automi_graph_coloring.cpp)

# ================ PPR Variants ================
add_graphlab_executable(power_ppr single_ppr.cpp)
add_graphlab_executable(automi_ppr automi_ppr.cpp)
add_graphlab_executable(track_ppr track_ppr.cpp)

# ================ SpMV Variants ================
add_graphlab_executable(power_spmv single_spmv.cpp)
add_graphlab_executable(track_spmv track_spmv.cpp)
add_graphlab_executable(automi_spmv automi_spmv.cpp)

# ================ CF (SGD) Variants ================
add_graphlab_executable(power_cf powerlyra_sgd.cpp)
requires_eigen(power_cf)
add_graphlab_executable(automi_cf automi_sgd.cpp)
requires_eigen(automi_cf)