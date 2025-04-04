#include <bitset>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

using namespace std;

class ProbabilisticSet
{
private:
    static const int ARRAY_SIZE = 100000;
    static const int MAX_ITEMS = 10000;
    const vector<size_t> multipliers = {773, 311, 563, 647, 13, 839, 317, 673, 109, 503, 467, 827, 293, 283, 601, 61, 7, 857, 521, 419, 809, 307, 503, 419, 367, 521, 193, 179, 113, 811};
    bitset<ARRAY_SIZE> bitVector; // Bit vector of size 10
    int numHashFunctions;

    // Generates multiple hash values for a given string
    vector<size_t> calculateHashes(const string &key) const
    {
        vector<size_t> hashValues;
        hash<string> stringHasher;
        for (int i = 0; i < numHashFunctions; i++)
        {
            // Calculate hash using a combination of the string, multiplier, and index
            size_t hashedValue = (stringHasher(key + to_string(i)) * multipliers[i] + i) % bitVector.size();
            hashValues.push_back(hashedValue);
        }
        return hashValues;
    }

public:
    ProbabilisticSet(int maxItems = MAX_ITEMS)
    {
        // Determine the optimal number of hash functions based on array size and maximum items
        int vectorSize = bitVector.size();
        numHashFunctions = ceil((vectorSize / maxItems) * log(2));
    }

    // Adds a key to the probabilistic set
    void insert(const string &key)
    {
        vector<size_t> hashValues = calculateHashes(key);
        for (size_t hash : hashValues)
        {
            bitVector[hash] = 1;
        }
    }

    // Checks if a key might exist in the probabilistic set
    bool exists(const string &key) const
    {
        vector<size_t> hashValues = calculateHashes(key);
        for (size_t hash : hashValues)
        {
            if (!bitVector[hash])
            {
                return false; // Key is definitely not in the set
            }
        }
        return true; // Key might be in the set
    }
};
