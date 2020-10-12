int IsPowerOfTwo(uint64_t x){
    return (x != 0) && ((x & (x - 1)) == 0);
}
