#include "HEADER.h"
#include "avl_tree.cpp"
#include "probabilistic_set.cpp"
// #include "synchronisation.cpp"
#include <thread>
#include <mutex>

/// Mutex to protect the SSTable list
mutex mtx_sstablelist;
AVLTree tree;

/// Time for compaction, initialized to MAX_COMP_TIME
int comp_time = MAX_COMP_TIME;

/// Encodes a key-value pair as a string
/**
 * Encodes a given key-value pair into a single string with a delimiter separating the key and value.
 * 
 * @param key_str The key as a string.
 * @param value_str The value as a string.
 * 
 * @return A string that contains the key-value pair, separated by a delimiter.
 */
string encodeKeyValuePair(const string &key_str, const string &value_str)
{
    string result = key_str + DELIMITER + value_str + DELIMITER;
    return result;
}

/// Decodes a key-value pair from a combined string
/**
 * Decodes a key-value pair from a string that contains both the key and value, separated by a delimiter.
 * 
 * @param combinedStr The combined string containing the key and value.
 * 
 * @return A pair of strings, where the first element is the key and the second is the value.
 */
pair<string, string> decodeKeyValuePair(const string &combinedStr)
{
    size_t pos = combinedStr.find(DELIMITER);
    string key_str, value_str;

    if (pos != string::npos)
    {
        key_str = combinedStr.substr(0, pos);
        value_str = combinedStr.substr(pos + 1);
    }
    value_str.pop_back();
    return {key_str, value_str};
}

/// Splits a string into a vector of tokens using the delimiter
/**
 * This function splits a string into smaller strings (tokens) based on a delimiter.
 * 
 * @param str The input string to split.
 * 
 * @return A vector of strings representing the tokens.
 */
vector<string> splitString(const string &str)
{
    vector<string> tokens;
    string token;
    for (char ch : str)
    {
        if (ch == DELIMITER)
        {
            if (!token.empty())
            {
                tokens.push_back(token);
                token.clear();
            }
        }
        else
        {
            token += ch;
        }
    }
    if (!token.empty())
    {
        tokens.push_back(token);
    }
    return tokens;
}

/// Extracts a pair of integers from a binary file at a specific index
/**
 * This function reads a pair of integers from a binary file at a given index.
 * 
 * @param filename The name of the binary file.
 * @param pair_idx The index of the pair to read.
 * 
 * @return A pair of integers representing the key and value, or {-1, -1} if there is an error.
 */
pair<int32_t, int32_t> extractPair(const string &filename, int pair_idx)
{
    ifstream file(filename, ios::binary);
    if (!file)
    {
        cerr << "Failed to open file: " << filename << endl;
        return {-1, -1};
    }

    // Calculate the offset: each pair takes 8 bytes (2 integers of 4 bytes each)
    streampos offset = pair_idx * 8;
    file.seekg(offset, ios::beg);

    int32_t key, value;
    file.read(reinterpret_cast<char *>(&key), sizeof(int32_t));
    file.read(reinterpret_cast<char *>(&value), sizeof(int32_t));

    if (!file)
    {
        cerr << "Error reading from file: " << filename << endl;
        return {-1, -1}; // Return an invalid pair if reading fails
    }

    return {key, value};
}

/// Extracts a key-value pair from a file at a specific position
/**
 * This function extracts a key-value pair from a text file at a given position.
 * 
 * @param filename The name of the file.
 * @param position The position within the file to start reading.
 * 
 * @return A string containing the key-value pair.
 */
string extractKeyValuePair(const string &filename, streampos position)
{
    ifstream inFile(filename, ios::binary);
    if (!inFile)
    {
        throw runtime_error("Cannot open file");
    }

    inFile.seekg(position);
    if (!inFile)
    {
        throw runtime_error("Invalid position in file");
    }

    string result;
    char ch;
    int delimiterCount = 0;

    while (inFile.get(ch))
    {
        result += ch;
        if (ch == DELIMITER)
        {
            delimiterCount++;
            if (delimiterCount == 2)
            {
                break;
            }
        }
    }

    if (delimiterCount < 2)
    {
        throw runtime_error("Second occurrence of DELIMITER not found");
    }

    return result;
}

/// Creates a folder with the given name if it doesn't exist
/**
 * This function creates a folder in the filesystem with the specified name.
 * 
 * @param folder_name The name of the folder to create.
 */
void createFolder(const string &folder_name)
{
    try
    {
        // Check if the folder already exists
        if (fs::exists(folder_name))
        {
            // cout << "Folder '" << folder_name << "' already exists." << endl;
        }
        else
        {
            // Create the folder
            if (fs::create_directory(folder_name))
            {
                // cout << "Folder '" << folder_name << "' created successfully." << endl;
            }
            else
            {
                cerr << "Failed to create folder '" << folder_name << "'." << endl;
            }
        }
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
    }
}

/// Deletes a folder and its contents
/**
 * This function deletes the folder and all its contents.
 * 
 * @param folder_name The name of the folder to delete.
 */
void deleteFolder(const string &folder_name)
{
    try
    {
        // Check if the folder exists
        if (!fs::exists(folder_name))
        {
            return;
        }

        // Remove the folder and its contents
        fs::remove_all(folder_name);

        // cout << "Folder deleted successfully: " << folder_name << endl;
    }
    catch (const fs::filesystem_error &e)
    {
        cerr << "Error deleting folder: " << e.what() << endl;
    }
}

/// Class to represent a Single SSTable (Sorted String Table)
class SSTable
{
private:
    string folder_name;    ///< Folder name associated with this SSTable
    ProbabilisticSet bfilter;  ///< Bloom filter for fast lookups
    int num_keys = 0;         ///< Number of keys in the SSTable

public:
    /// Constructor to initialize an SSTable
    /**
     * The constructor creates a folder and stores key-value pairs into binary files.
     * 
     * @param data Data to be inserted into the SSTable (size and key-value pairs).
     * @param fname Folder name where SSTable data will be stored. Defaults to TOMBSTONE if not provided.
     */
    SSTable(const pair<int, pair<string, string> *> &data = {0, nullptr}, string fname = TOMBSTONE)
    {
        if (fname == TOMBSTONE)
        {
            int idx = SSTable_list.size();
            folder_name = "SSTable_" + to_string(idx);
        }
        else
        {
            folder_name = fname;
        }
        createFolder(folder_name);

        num_keys = data.first;
        pair<string, string> *keyval_array = data.second;

        string *hashed_data = new string[num_keys];

        for (int i = 0; i < num_keys; ++i)
        {
            hashed_data[i] = encodeKeyValuePair(keyval_array[i].first, keyval_array[i].second);
            bfilter.insert(keyval_array[i].first);
        }

        int indices_size = 0;
        pair<int, int> *indices = store_keyval_data(hashed_data, num_keys);
        store_keyval_index(indices, num_keys);

        delete[] hashed_data;
        delete[] indices;
    }

    /// Destructor to clean up resources
    ~SSTable()
    {
        deleteFolder(folder_name);
    }

    /// Gets the number of keys in the SSTable
    /**
     * This function returns the number of key-value pairs in the SSTable.
     * 
     * @return The number of keys in the SSTable.
     */
    int get_num_keys()
    {
        return num_keys;
    }

    /// Gets the folder name of the SSTable
    /**
     * This function returns the folder name where the SSTable data is stored.
     * 
     * @return The folder name associated with the SSTable.
     */
    string get_folder_name()
    {
        return folder_name;
    }

    /// Finds a value for the given key using the Bloom filter and binary search
    /**
     * This function searches for a key in the SSTable using the Bloom filter and binary search.
     * 
     * @param key The key to search for.
     * 
     * @return A pair indicating if the key was found and its associated value (or TOMBSTONE if not found).
     */
    pair<bool, string> find(const string key)
    {
        if (bfilter.exists(key) && num_keys > 0)
        {
            int lo = 0, hi = num_keys - 1;
            while (lo <= hi)
            {
                int mid = (lo + hi) / 2;
                int file_idx = mid / INDEX_SIZE;
                int pair_idx = mid % INDEX_SIZE;
                string filename = folder_name + "/" + to_string(file_idx) + ".bin";

                pair<int32_t, int32_t> result = extractPair(filename, pair_idx);
                if (result.first == -1 || result.second == -1)
                {
                    cerr << "Error: Could not read pair at index " << pair_idx << " in file " << filename << endl;
                    exit(1);
                }

                int key_value_file_idx = result.first, key_value_pos_idx = result.second;
                string key_value_filename = folder_name + "/" + to_string(key_value_file_idx) + ".txt";

                string key_value = extractKeyValuePair(key_value_filename, key_value_pos_idx);
                auto p = decodeKeyValuePair(key_value);
                string curr_key = p.first, curr_value = p.second;

                if (curr_key == key)
                {
                    return make_pair(true, curr_value);
                }
                else if (curr_key > key)
                {
                    hi = mid - 1;
                }
                else
                {
                    lo = mid + 1;
                }
            }
        }
        return make_pair(false, TOMBSTONE);
    }

    /// Stores the index of key-value pairs into binary files
    /**
     * This function stores key-value pair indices into multiple binary files.
     * 
     * @param data A pointer to the array of key-value pair indices.
     * @param num_pairs The total number of pairs.
     */
    void store_keyval_index(const pair<int, int> *data, int num_pairs)
    {
        const size_t maxPairsPerFile = INDEX_SIZE; ///< Maximum pairs per file
        size_t fileIndex = 0;

        for (int i = 0; i < num_pairs; i += maxPairsPerFile)
        {
            string filename = folder_name + "/" + to_string(fileIndex++) + ".bin";
            ofstream outFile(filename, ios::binary);
            if (!outFile)
            {
                cerr << "Error opening file: " << filename << endl;
                return;
            }

            size_t pairsToWrite = min(static_cast<size_t>(maxPairsPerFile), static_cast<size_t>(num_pairs - i));
            for (size_t j = 0; j < pairsToWrite; ++j)
            {
                outFile.write(reinterpret_cast<const char *>(&data[i + j].first), sizeof(int));
                outFile.write(reinterpret_cast<const char *>(&data[i + j].second), sizeof(int));
            }

            outFile.close();
        }
    }

    /// Stores key-value data into text files and returns the file offsets
    /**
     * This function stores the encoded key-value pairs into text files and returns the offsets where each pair is stored.
     * 
     * @param data An array of encoded key-value pairs.
     * @param num_keys The total number of key-value pairs to store.
     * 
     * @return A pointer to an array of offsets for each key-value pair stored in files.
     */
    pair<int, int> *store_keyval_data(const string *data, int num_keys)
    {
        const size_t maxFileSize = MAX_FILE_SIZE; ///< Maximum file size for each text file
        pair<int, int> *fileOffsets = new pair<int, int>[num_keys];

        int fileIndex = 0;
        size_t currentFileSize = 0;
        ofstream outFile;
        string filename = folder_name + "/" + "0.txt";
        outFile.open(filename);

        int offsetIndex = 0;
        for (int i = 0; i < num_keys; ++i)
        {
            const string &str = data[i];
            size_t newSize = currentFileSize + str.size() + 1;

            // If adding the current string exceeds max file size, open a new file
            if (newSize > maxFileSize)
            {
                outFile.close();
                currentFileSize = 0;
                filename = folder_name + "/" + to_string(++fileIndex) + ".txt";
                outFile.open(filename);  ///< Open a new text file
            }

            fileOffsets[offsetIndex++] = {fileIndex, static_cast<int>(currentFileSize)};
            outFile << str;
            currentFileSize += str.size();
        }

        outFile.close();
        return fileOffsets;
    }
};

/// Creates an SSTable from a vector of key-value pairs
/**
 * This function creates an SSTable from a list of key-value pairs by converting them into an array
 * and calling the appropriate constructor of the SSTable class.
 * 
 * @param data A vector of key-value pairs.
 */
void create_SSTable(vector<pair<string, string>> &data)
{
    int num_keys = data.size();
    pair<string, string>* data_array = new pair<string, string>[num_keys];

    for (int i = 0; i < num_keys; ++i)
    {
        data_array[i] = data[i];
    }

    pair<int, pair<string, string>*> data_pair = {num_keys, data_array};
    
    mtx_sstablelist.lock();
    SSTable_list.push_back(new SSTable(data_pair));
    mtx_sstablelist.unlock();

    delete[] data_array;
}

extern "C"{
    /// Sets a key-value pair into the system
    /**
     * This function inserts a key-value pair into the AVL tree, and if the tree reaches its maximum size,
     * an SSTable is created and the tree is cleared.
     * 
     * @param key1 The key to insert.
     * @param value1 The value associated with the key.
     */
    void SET(char* key1, char* value1)
    {
        if (comp_time < MAX_COMP_TIME)
        {
            comp_time *= 10;
        }
        string key = std::string(key1);
        string value = std::string(value1);

        tree.insert(key, value);
        if (tree.size() == MAX_TREE_SIZE)
        {
            vector<pair<string, string>> data = tree.getSortedPairs();
            create_SSTable(data);
            tree.clear();
        }
    }

    /// Marks a key as deleted by inserting a tombstone
    /**
     * This function inserts a tombstone (a special marker for deleted keys) into the system.
     * 
     * @param key The key to mark as deleted.
     */
    void DEL(char* key)
    {
        char* temp = "tombstone";
        SET(key, temp);
    }

    /// Retrieves the value for a given key
    /**
     * This function searches for the value associated with a given key in both the AVL tree
     * and the SSTables.
     * 
     * @param key1 The key to search for.
     * 
     * @return The value associated with the key, or a tombstone marker if the key is not found.
     */
    const char* GET(char* key1)
    {
        string key = std::string(key1);
        auto value = tree.find(key);
        if (value.first)
        {
            return value.second.c_str();
        }
        if (comp_time > MIN_COMP_TIME)
        {
            comp_time /= 10;
        }
        mtx_sstablelist.lock();
        for (int i = (int)SSTable_list.size() - 1; i >= 0; i--)
        {
            if (SSTable_list[i] == nullptr) continue;
            value = SSTable_list[i]->find(key);
            if (value.first)
            {
                mtx_sstablelist.unlock();
                return value.second.c_str();
            }
        }
        mtx_sstablelist.unlock();
        return TOMBSTONE.c_str();
    }
}

/// Reads an SSTable from the filesystem and returns the key-value pairs
/**
 * This function reads all key-value pairs from an SSTable stored in the filesystem.
 * 
 * @param folder_name The name of the folder where the SSTable is stored.
 * @param data_size The number of key-value pairs in the SSTable.
 * 
 * @return A pointer to an array of key-value pairs.
 */
pair<string, string> *read_SSTable(string &folder_name, int data_size)
{
    auto *data = new pair<string, string>[data_size];

    int idx = 0; // Current index in the array
    for (int i = 0;; i++)
    {
        string file_name = folder_name + "/" + to_string(i) + ".txt";

        if (!fs::exists(file_name))
        {
            break;
        }

        ifstream file(file_name);
        if (!file.is_open())
        {
            cerr << "Error opening file: " << file_name << endl;
            continue;
        }

        string line;
        while (getline(file, line))
        {
            vector<string> tokens = splitString(line);
            for (size_t j = 0; j < tokens.size(); j += 2)
            {
                string _key = tokens[j];
                string _value = (j + 1 < tokens.size()) ? tokens[j + 1] : TOMBSTONE;
                data[idx++] = {_key, _value};
            }
        }

        file.close();
    }

    return data;
}

/// Merges two sorted SSTables into one
/**
 * This function merges two sorted SSTables into a single sorted SSTable. Duplicate keys are removed,
 * and the merged results are returned.
 * 
 * @param recent_sstable The most recent SSTable.
 * @param old_sstable The older SSTable.
 * 
 * @return A pair containing the size and an array of merged key-value pairs.
 */
pair<int, pair<string, string> *> mergeSortedSSTables(const pair<int, pair<string, string> *> &recent_sstable, const pair<int, pair<string, string> *> &old_sstable)
{
    int recent_size = recent_sstable.first;
    int old_size = old_sstable.first;
    pair<string, string> *recent_array = recent_sstable.second;
    pair<string, string> *old_array = old_sstable.second;

    int merged_size = recent_size + old_size;
    pair<string, string> *merged_array = new pair<string, string>[merged_size];

    int i = 0, j = 0, k = 0;

    while (i < recent_size && j < old_size)
    {
        if (recent_array[i].first < old_array[j].first)
        {
            merged_array[k++] = recent_array[i++];
        }
        else if (recent_array[i].first > old_array[j].first)
        {
            merged_array[k++] = old_array[j++];
        }
        else
        {
            merged_array[k++] = recent_array[i++];
            j++;
        }
    }

    while (i < recent_size)
    {
        merged_array[k++] = recent_array[i++];
    }

    while (j < old_size)
    {
        merged_array[k++] = old_array[j++];
    }

    pair<string, string> *resized_array = new pair<string, string>[k];
    for (int idx = 0; idx < k; ++idx)
    {
        resized_array[idx] = merged_array[idx];
    }
    delete[] merged_array;

    return {k, resized_array};
}

/// Compaction process to merge SSTables and reduce storage usage
/**
 * This function performs compaction by periodically merging the last two SSTables
 * when there are too many SSTables in the system. The merged SSTable is stored and
 * old SSTables are deleted.
 */
void compact()
{
    while (1)
    {
        int ll = -1, rr = -1;
        bool make_compact = false;
        if (SSTable_list.size() > 100)
        {
            mtx_sstablelist.lock();
            rr = (int)SSTable_list.size() - 1, ll = rr - 1;
            if (SSTable_list[rr] == nullptr)
            {
                SSTable_list.pop_back();
                if (SSTable_list[ll] == nullptr)
                {
                    SSTable_list.pop_back();
                }
            }
            else
            {
                if (SSTable_list[ll] == nullptr)
                {
                    SSTable_list[ll] = SSTable_list[rr];
                    SSTable_list.pop_back();
                }
                else
                {
                    make_compact = true;
                }
            }
            mtx_sstablelist.unlock();
        }
        if (make_compact)
        {
            int num_keys_ll = SSTable_list[ll]->get_num_keys(), num_keys_rr = SSTable_list[rr]->get_num_keys();
            string folder_name_ll = SSTable_list[ll]->get_folder_name(), folder_name_rr = SSTable_list[rr]->get_folder_name();
            pair<string, string> *keyval_ll = read_SSTable(folder_name_ll, num_keys_ll);
            pair<string, string> *keyval_rr = read_SSTable(folder_name_rr, num_keys_rr);

            pair<int, pair<string, string> *> keyval_comp = mergeSortedSSTables(make_pair(num_keys_rr, keyval_rr), make_pair(num_keys_ll, keyval_ll));

            delete[] keyval_ll;
            delete[] keyval_rr;

            mtx_sstablelist.lock();
            delete SSTable_list[ll];
            delete SSTable_list[rr];
            SSTable *table_comp = new SSTable(keyval_comp, folder_name_ll);
            SSTable_list[ll] = table_comp;
            SSTable_list[rr] = nullptr;
            mtx_sstablelist.unlock();

            delete[] keyval_comp.second;
        }
        usleep(comp_time);
    }
}

/// Starts the compaction process in a separate thread
/**
 * This function starts the compaction process in a new thread.
 */
extern "C"{
    void start_compaction()
    {
        thread compaction_thread(compact);
        compaction_thread.detach();
    }
}

