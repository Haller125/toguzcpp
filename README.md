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