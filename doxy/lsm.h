#ifndef HEADER_H
#define HEADER_H

#include <string>
#include <vector>
#include <mutex>
#include <utility>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <experimental/filesystem>
#include "avl_tree.h"
#include "probabilistic_set.h"

// namespace fs = std::experimental::filesystem;
namespace fs = std::filesystem;

// Constants
const char DELIMITER = '#';    // Used for encoding key-value pairs
const std::string TOMBSTONE = "tombstone"; // Special marker for deleted keys
const int MAX_TREE_SIZE = 1000;      // Maximum number of keys in the AVL tree
const int INDEX_SIZE = 512;          // Maximum number of indices per binary file
const int MAX_FILE_SIZE = 4096;      // Maximum file size (bytes) for storing key-value pairs
const int MAX_COMP_TIME = 100000;   // Maximum compaction time (microseconds)
const int MIN_COMP_TIME = 1;      // Minimum compaction time (microseconds)

// Classes
class SSTable;

// Global Variables
extern  AVLTree tree;
extern  std::vector<SSTable *> SSTable_list;
extern  std::mutex mtx_sstablelist;
extern int comp_time;

// Function Declarations
std::string encodeKeyValuePair(const std::string &key_str, const std::string &value_str);
std::pair<std::string, std::string> decodeKeyValuePair(const std::string &combinedStr);
std::vector<std::string> splitString(const std::string &str);
std::pair<int32_t, int32_t> extractPair(const std::string &filename, int pair_idx);
std::string extractKeyValuePair(const std::string &filename, std::streampos position);
void createFolder(const std::string &folder_name);
void deleteFolder(const std::string &folder_name);
void create_SSTable(std::vector<std::pair<std::string, std::string>> &data);
std::pair<std::string, std::string> *read_SSTable(std::string &folder_name, int data_size);
std::pair<int, std::pair<std::string, std::string> *> mergeSortedSSTables(
    const std::pair<int, std::pair<std::string, std::string> *> &recent_sstable,
    const std::pair<int, std::pair<std::string, std::string> *> &old_sstable);
void compact();
void start_compaction();

// SSTable Class
class SSTable
{
private:
    std::string folder_name;
    ProbabilisticSet bfilter;
    int num_keys;

public:
    SSTable(const std::pair<int, std::pair<std::string, std::string> *> &data = {0, nullptr}, std::string fname = TOMBSTONE);
    ~SSTable();
    int get_num_keys();
    std::string get_folder_name();
    std::pair<bool, std::string> find(const std::string key);

private:
    void store_keyval_index(const std::pair<int, int> *data, int num_pairs);
    std::pair<int, int> *store_keyval_data(const std::string *data, int num_keys);
};

// C-Style Interface for External Use

    void SET(char* key1, char* value1);
    void DEL(char* key);
    const char* GET(char* key1);
    void start_compaction();


#endif // HEADER_H
