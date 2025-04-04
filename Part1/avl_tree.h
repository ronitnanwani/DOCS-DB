#ifndef AVLTREE_H
#define AVLTREE_H

#include <string>
#include <vector>
#include <utility>

class AVLTreeNode
{
public:
    AVLTreeNode *left;
    AVLTreeNode *right;
    int count;
    int height;
    std::string key;
    std::string value;

    AVLTreeNode(const std::string &key, const std::string &value);

    void updateValues();
    int balanceFactor();

    AVLTreeNode *left_rotate();
    AVLTreeNode *right_rotate();
};

class AVLTree
{
private:
    AVLTreeNode *root;
    int _size;

    void balance(std::vector<AVLTreeNode **> &path);
    void clearHelper(AVLTreeNode *node);

public:
    AVLTree();
    ~AVLTree();

    void insert(const std::string &key, const std::string &value);
    void erase(const std::string &key);
    void clear();
    bool empty() const;
    int size() const;

    std::pair<bool, std::string> find(const std::string &key) const;
    std::string operator[](int idx) const;
    std::vector<std::pair<std::string, std::string>> getSortedPairs() const;
};

#endif // AVLTREE_H
