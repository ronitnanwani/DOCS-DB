#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <bits/stdc++.h>

using namespace std;

/**
 * @struct AVLTreeNode
 * @brief Represents a node in the AVL Tree, which contains a key-value pair and maintains
 *        balance information for AVL tree operations.
 */
struct AVLTreeNode
{
    AVLTreeNode *left;          ///< Pointer to the left child node
    AVLTreeNode *right;         ///< Pointer to the right child node
    string key;                 ///< The key of the node
    string value;               ///< The value associated with the key
    int count;                  ///< The size of the subtree rooted at this node
    int height;                 ///< The height of the node in the AVL tree

    /**
     * @brief Constructor to create a new AVL tree node with a key-value pair.
     * @param key The key for the node.
     * @param value The value associated with the key.
     */
    AVLTreeNode(const string &key, const string &value);

    /**
     * @brief Updates the count and height of the node.
     * The count is the total number of nodes in the subtree, including this node.
     * The height is the maximum depth of the node from its children.
     */
    void updateValues();

    /**
     * @brief Computes the balance factor of the node.
     * @return The difference between the height of the left and right subtrees.
     */
    int balanceFactor();

    /**
     * @brief Performs a left rotation on the node.
     * @return The new root of the rotated subtree.
     */
    AVLTreeNode *left_rotate();

    /**
     * @brief Performs a right rotation on the node.
     * @return The new root of the rotated subtree.
     */
    AVLTreeNode *right_rotate();
};

/**
 * @class AVLTree
 * @brief A self-balancing binary search tree (AVL tree) that supports insertion, deletion,
 *        search, and retrieval of sorted key-value pairs.
 */
class AVLTree
{
    AVLTreeNode *root;   ///< Pointer to the root node of the AVL tree
    int _size;           ///< The total number of nodes in the AVL tree

    /**
     * @brief Balances the tree along the path of nodes starting from the root to the newly
     *        inserted or removed node.
     * @param path A vector of pointers to nodes along the insertion or deletion path.
     */
    void balance(vector<AVLTreeNode **> &path);

    /**
     * @brief Helper function to recursively delete all nodes in the tree.
     * @param node The current node being deleted.
     */
    void clearHelper(AVLTreeNode *node);

public:
    /**
     * @brief Default constructor that initializes an empty AVL tree.
     */
    AVLTree();

    /**
     * @brief Destructor that clears the AVL tree and frees memory.
     */
    ~AVLTree();

    /**
     * @brief Clears the AVL tree, removing all nodes and resetting the size.
     */
    void clear();

    /**
     * @brief Checks if the AVL tree is empty.
     * @return true if the tree is empty, false otherwise.
     */
    bool empty() const;

    /**
     * @brief Returns the number of nodes in the AVL tree.
     * @return The size of the AVL tree.
     */
    int size() const;

    /**
     * @brief Inserts a key-value pair into the AVL tree.
     * If the key already exists, its value is updated.
     * @param key The key to insert.
     * @param value The value associated with the key.
     */
    void insert(const string &key, const string &value);

    /**
     * @brief Deletes a key-value pair from the AVL tree.
     * @param key The key to delete.
     */
    void erase(const string &key);

    /**
     * @brief Searches for a key in the AVL tree.
     * @param key The key to search for.
     * @return A pair where the first element is a boolean indicating whether the key exists,
     *         and the second element is the value associated with the key (or a tombstone).
     */
    pair<bool, string> find(const string &key) const;

    /**
     * @brief Overloads the subscript operator to access values by index.
     * @param idx The index of the value to retrieve.
     * @return The value at the given index in the AVL tree.
     */
    string operator[](int idx) const;

    /**
     * @brief Retrieves all key-value pairs in the tree, sorted by key.
     * @return A vector of pairs representing the sorted key-value pairs.
     */
    vector<pair<string, string>> getSortedPairs() const;
};

AVLTreeNode::AVLTreeNode(const string &key, const string &value)
    : key(key), value(value), left(nullptr), right(nullptr), count(1), height(1) {}

void AVLTreeNode::updateValues()
{
    count = (left ? left->count : 0) + (right ? right->count : 0) + 1;
    height = max(left ? left->height : 0, right ? right->height : 0) + 1;
}

int AVLTreeNode::balanceFactor()
{
    return (left ? left->height : 0) - (right ? right->height : 0);
}

AVLTreeNode *AVLTreeNode::left_rotate()
{
    AVLTreeNode *R = right;
    right = right->left;
    R->left = this;
    updateValues();
    R->updateValues();
    return R;
}

AVLTreeNode *AVLTreeNode::right_rotate()
{
    AVLTreeNode *L = left;
    left = left->right;
    L->right = this;
    updateValues();
    L->updateValues();
    return L;
}

AVLTree::AVLTree() : root(nullptr), _size(0) {}

AVLTree::~AVLTree()
{
    clear();
}

void AVLTree::clear()
{
    clearHelper(root);
    root = nullptr;
    _size = 0;
}

void AVLTree::clearHelper(AVLTreeNode *node)
{
    if (node)
    {
        clearHelper(node->left);
        clearHelper(node->right);
        delete node;
    }
}

bool AVLTree::empty() const
{
    return _size == 0;
}

int AVLTree::size() const
{
    return _size;
}

void AVLTree::insert(const string &key, const string &value)
{
    AVLTreeNode **indirect = &root;
    vector<AVLTreeNode **> path;

    while (*indirect != nullptr)
    {
        path.push_back(indirect);

        if ((*indirect)->key == key)
        {
            (*indirect)->value = value;
            return;
        }
        else if ((*indirect)->key > key)
            indirect = &((*indirect)->left);
        else
            indirect = &((*indirect)->right);
    }

    *indirect = new AVLTreeNode(key, value);
    path.push_back(indirect);

    balance(path);
    _size++;
}

void AVLTree::erase(const string &key)
{
    AVLTreeNode **indirect = &root;
    vector<AVLTreeNode **> path;

    while (*indirect != nullptr && (*indirect)->key != key)
    {
        path.push_back(indirect);

        if ((*indirect)->key > key)
            indirect = &((*indirect)->left);
        else
            indirect = &((*indirect)->right);
    }

    if (*indirect == nullptr)
        return;

    AVLTreeNode *toRemove = *indirect;
    if (!(*indirect)->left && !(*indirect)->right)
    {
        *indirect = nullptr;
    }
    else if (!(*indirect)->right)
    {
        *indirect = (*indirect)->left;
    }
    else if (!(*indirect)->left)
    {
        *indirect = (*indirect)->right;
    }
    else
    {
        AVLTreeNode **successor = &((*indirect)->right);
        while ((*successor)->left)
        {
            path.push_back(successor);
            successor = &((*successor)->left);
        }

        AVLTreeNode *temp = *successor;
        *successor = (*successor)->right;

        temp->left = (*indirect)->left;
        temp->right = (*indirect)->right;
        *indirect = temp;
    }

    delete toRemove;
    balance(path);
    _size--;
}

pair<bool, string> AVLTree::find(const string &key) const
{
    AVLTreeNode *direct = root;

    while (direct != nullptr)
    {
        if (direct->key == key)
            return make_pair(true, direct->value);
        else if (direct->key > key)
            direct = direct->left;
        else
            direct = direct->right;
    }
    return make_pair(false, "tombstone"); // Key not found
}

string AVLTree::operator[](int idx) const
{
    AVLTreeNode *cur = root;
    int left = cur->left ? cur->left->count : 0;

    while (left != idx)
    {
        if (left < idx)
        {
            idx -= left + 1;
            cur = cur->right;
            left = cur->left ? cur->left->count : 0;
        }
        else
        {
            cur = cur->left;
            left = cur->left ? cur->left->count : 0;
        }
    }

    return cur->value;
}

void AVLTree::balance(vector<AVLTreeNode **> &path)
{
    reverse(path.begin(), path.end());

    for (auto indirect : path)
    {
        (*indirect)->updateValues();

        if ((*indirect)->balanceFactor() >= 2 && (*indirect)->left->balanceFactor() >= 1)
            *indirect = (*indirect)->right_rotate();

        else if ((*indirect)->balanceFactor() >= 2)
        {
            (*indirect)->left = (*indirect)->left->left_rotate();
            *indirect = (*indirect)->right_rotate();
        }
        else if ((*indirect)->balanceFactor() <= -2 && (*indirect)->right->balanceFactor() <= -1)
            *indirect = (*indirect)->left_rotate();

        else if ((*indirect)->balanceFactor() <= -2)
        {
            (*indirect)->right = (*indirect)->right->right_rotate();
            *indirect = (*indirect)->left_rotate();
        }
    }
}

vector<pair<string, string>> AVLTree::getSortedPairs() const
{
    vector<pair<string, string>> result;
    function<void(AVLTreeNode *)> inOrderTraversal = [&](AVLTreeNode *node)
    {
        if (node == nullptr)
            return;
        inOrderTraversal(node->left);
        result.emplace_back(node->key, node->value);
        inOrderTraversal(node->right);
    };
    inOrderTraversal(root);
    return result;
}

