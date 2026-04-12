### Run main.cpp

```
cd build
cmake ..
cmake --build
./toguz_benchmark
```

### Run Tests

```
cd build
cmake ..
cmake --build
./toguznative_tests
```

### Run Tabular Self-Q Learning

```
cd build
cmake ..
cmake --build .

# Train with defaults and save Q-table to qtable_selfplay.txt
./qlearning_selfplay

# Example custom run:
# episodes alpha gamma eps_start eps_min eps_decay log_every qtable_out qtable_in delta_scale terminal_bonus terminal_score_scale
./qlearning_selfplay 50000 0.1 0.99 0.2 0.02 0.99995 1000 qtable_selfplay.txt "" 1.0 100.0 1.0
```


```
cd build
rm -rf *  # Clean old build

# Release build (optimized, portable)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Or Debug build
cmake .. -DCMAKE_TYPE=Debug

# Specify compiler explicitly if needed
cmake .. -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Release
cmake .. -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_BUILD_TYPE=Release

cmake --build .
```

```
python main.py --file-path toguz_data.bin --batch-size 256 --num-epochs 100 --learning-rate 0.001 --num-workers 4 --input-size 902 --hidden-size 22
```