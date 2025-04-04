#include <bitset>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

using namespace std;

/**
 * @class ProbabilisticSet
 * @brief A class representing a probabilistic set using a Bloom filter.
 *
 * This class provides a probabilistic way to check for membership of an item using a
 * bit vector and multiple hash functions. It can be used for approximate membership
 * testing, where false positives are possible, but false negatives are not.
 */
class ProbabilisticSet
{
private:
    /**
     * @brief The size of the bit vector used in the Bloom filter.
     */
    static const int ARRAY_SIZE = 100000;
    
    /**
     * @brief The maximum number of items that the probabilistic set will store.
     */
    static const int MAX_ITEMS = 10000;

    /**
     * @brief A list of multipliers used in the hash functions to ensure different hash outputs.
     */
    const vector<size_t> multipliers = {773, 311, 563, 647, 13, 839, 317, 673, 109, 503, 467, 827, 293, 283, 601, 61, 7, 857, 521, 419, 809, 307, 503, 419, 367, 521, 193, 179, 113, 811};

    /**
     * @brief A bitset representing the Bloom filter for storing hashed values.
     */
    bitset<ARRAY_SIZE> bitVector;

    /**
     * @brief The number of hash functions to use in the Bloom filter.
     */
    int numHashFunctions;

    /**
     * @brief Calculates multiple hash values for a given key using a combination of hash functions.
     *
     * @param key The key to be hashed.
     * @return A vector of hash values.
     */
    vector<size_t> calculateHashes(const string &key) const
    {
        vector<size_t> hashValues;
        hash<string> stringHasher;
        for (int i = 0; i < numHashFunctions; i++)
        {
            // Calculate a hash value using the key, a multiplier, and the index
            size_t hashedValue = (stringHasher(key + to_string(i)) * multipliers[i] + i) % bitVector.size();
            hashValues.push_back(hashedValue);
        }
        return hashValues;
    }

public:
    /**
     * @brief Constructs a ProbabilisticSet with a given maximum number of items.
     *
     * This constructor determines the optimal number of hash functions based on the size of the
     * bit vector and the maximum number of items that the set will store. The number of hash functions
     * is calculated to minimize false positive rates.
     *
     * @param maxItems The maximum number of items that the set will store. Default is 10000.
     */
    ProbabilisticSet(int maxItems = MAX_ITEMS)
    {
        // Calculate the optimal number of hash functions based on bit vector size and maximum items
        int vectorSize = bitVector.size();
        numHashFunctions = ceil((vectorSize / maxItems) * log(2));
    }

    /**
     * @brief Adds a key to the probabilistic set.
     *
     * This method hashes the given key multiple times and sets the corresponding positions
     * in the bit vector to 1. The key is added to the set.
     *
     * @param key The key to insert into the set.
     */
    void insert(const string &key)
    {
        vector<size_t> hashValues = calculateHashes(key);
        for (size_t hash : hashValues)
        {
            bitVector[hash] = 1; // Set the corresponding bit in the bit vector
        }
    }

    /**
     * @brief Checks if a key might exist in the probabilistic set.
     *
     * This method hashes the key and checks if all corresponding bits are set to 1. If any of the
     * bits are 0, the key is definitely not in the set. However, if all bits are 1, the key might
     * be in the set (due to the possibility of false positives).
     *
     * @param key The key to check for existence in the set.
     * @return True if the key might exist in the set, false if the key is definitely not in the set.
     */
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

/**
 * @brief Main function for testing the ProbabilisticSet class.
 *
 * This function demonstrates how to use the ProbabilisticSet class by adding items to the set,
 * checking for their existence, and showing how false positives can occur.
 *
 * @return Exit status.
 */
int main()
{
    // Create a ProbabilisticSet with a maximum of 1000 items
    ProbabilisticSet probSet(1000);

    // Insert some keys into the set
    probSet.insert("apple");
    probSet.insert("banana");
    probSet.insert("cherry");

    // Test if some keys exist in the set
    cout << "apple exists? " << (probSet.exists("apple") ? "Yes" : "No") << endl;
    cout << "banana exists? " << (probSet.exists("banana") ? "Yes" : "No") << endl;
    cout << "grape exists? " << (probSet.exists("grape") ? "Yes" : "No") << endl;

    return 0;
}

