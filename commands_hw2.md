## Building with scons
```
scons build/X86/gem5.opt -j8 --linker=mold --ignore-style
```

## Running Workloads
Note: these should all be run from the gem5 config folder!

### Hello World
```
../gem5/build/X86/gem5.opt ./se_hello_world.py
```
### Specific Benchmarks:
`bfs_small`:
```
../gem5/build/X86/gem5.opt --outdir ${GEM5_OUT}/bfs_small_out -re \
    -- se_custom_binary.py --input-bin="/scratch/cluster/speedway/cs395t/hw2/part1/bin/bfs" \
    --bpred "tage-sc-l" \
    --input-args="-n 1 -r 1 -f /scratch/cluster/speedway/cs395t/hw2/part1/inputs/bfs_small.sg"
```

`matmul_small`
```
../gem5/build/X86/gem5.opt --outdir ${GEM5_OUT}/matmul_small -re \
    -- se_custom_binary.py --input-bin=/scratch/cluster/speedway/cs395t/hw2/part1/bin/matmul_small \
    --bpred "tage-sc-l" 
```
*Note:* I set the --bpred flag here, but in the actual config files, I may have disabled it so that the CPU always runs with TAGE SC L

### SPEC
```
./scripts/run-spec06-se-periodic.py <BENCHMARK> \
    --outdir ${GEM5_OUT}/baseline_tage_sc_l_64/<BENCHMARK> \
    --ff-interval 10000 \
    --warmup-interval 25 \
    --roi-interval 50 \
    --init-ff-interval 10000 \
    --num-rois 3 \
    --redirect
```

### GAP
#### bfs
```
../gem5/build/X86/gem5.opt --outdir ${GEM5_OUT}/baseline_tage_sc_l_64/bfs -re \
    -- se_custom_binary_periodic.py \
    --input-bin /scratch/cluster/speedway/cs395t/hw2/part2/gap/bfs \
    --input-args "-r 1 -f /scratch/cluster/speedway/cs395t/hw2/part2/gap/g17.el" \
    --ff-interval 10000 \
    --warmup-interval 25 \
    --roi-interval 50 \
    --init-ff-interval 10000 \
    --num-rois 3
```

#### cc
```
../gem5/build/X86/gem5.opt --outdir ${GEM5_OUT}/baseline_tage_sc_l_64/cc -re \
    -- se_custom_binary_periodic.py \
    --input-bin /scratch/cluster/speedway/cs395t/hw2/part2/gap/cc \
    --input-args "-r 1 -f /scratch/cluster/speedway/cs395t/hw2/part2/gap/g17.el" \
    --ff-interval 10000 \
    --warmup-interval 25 \
    --roi-interval 50 \
    --init-ff-interval 10000 \
    --num-rois 3
```

### Run CMDs host
#### Baseline tage
```
./scripts/run-cmds.host.py ../commands_baseline_tage_sc_l_64.txt --num-workers=8
```
