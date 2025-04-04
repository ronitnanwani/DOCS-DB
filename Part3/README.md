1) Run 'make all' for all the relevant executable

2) Run ./server in one terminal

3) from the other terminal use redis-benchmark tool for testing

4) If the value of -r flag in redis-benchmark is less then we get good results and a balanced database (good for read heavy as well as write heavy operations). If the value of this flag is high write performance remains more of less the same but read would be slow (write heavy workloads)

5) Run 'make clean' to clean the folder