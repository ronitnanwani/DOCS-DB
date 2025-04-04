#ifndef PROBABILISTICSET_H
#define PROBABILISTICSET_H

#include <bitset>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

class ProbabilisticSet
{
private:
    static const int ARRAY_SIZE = 100000;
    static const int MAX_ITEMS = 10000;
    const std::vector<size_t> multipliers;
    std::bitset<ARRAY_SIZE> bitVector;
    int numHashFunctions;

    // Generates multiple hash values for a given string
    std::vector<size_t> calculateHashes(const std::string &key) const;

public:
    // Constructor to initialize the ProbabilisticSet
    ProbabilisticSet(int maxItems = MAX_ITEMS);

    // Adds a key to the probabilistic set
    void insert(const std::string &key);

    // Checks if a key might exist in the probabilistic set
    bool exists(const std::string &key) const;
};

#endif // PROBABILISTICSET_H
