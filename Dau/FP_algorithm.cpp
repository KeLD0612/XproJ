#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

map<int, vector<pair<string, int>>> zippedTrans;
map<string, int> itemFrequency;

struct FPNode {
    string item;
    int count;
    FPNode* parent;
    unordered_map<string, FPNode*> children;
    FPNode* next;

    FPNode(string item, int count = 1, FPNode* parent = nullptr)
        : item(item), count(count), parent(parent), next(nullptr) {}
};

struct HeaderTableEntry {
    int frequency;
    FPNode* head;
    FPNode* tail;

    HeaderTableEntry() : frequency(0), head(nullptr), tail(nullptr) {}
};

class FPTree {
private:
    FPNode* root;
    unordered_map<string, HeaderTableEntry> headerTable;

    void deleteTree(FPNode* node) {
        if (!node) return;
        for (auto& child : node->children) {
            deleteTree(child.second);
        }
        delete node;
    }

public:
    FPTree() : root(new FPNode("NULL")) {}

    ~FPTree() {
        deleteTree(root);
    }

    void insertTransaction(const vector<pair<string, int>>& transaction) {
        FPNode* currentNode = root;
        for (const auto& item : transaction) {
            auto& child = currentNode->children[item.first];
            if (!child) {
                child = new FPNode(item.first, item.second, currentNode);
                auto& entry = headerTable[item.first];
                entry.frequency += item.second;
                if (!entry.head) {
                    entry.head = entry.tail = child;
                } else {
                    entry.tail->next = child;
                    entry.tail = child;
                }
            } else {
                child->count += item.second;
                headerTable[item.first].frequency += item.second;
            }
            currentNode = child;
        }
    }

    vector<pair<vector<pair<string, int>>, int>> getConditionalPatterns(const string& item) {
        vector<pair<vector<pair<string, int>>, int>> patterns;
        if (headerTable.find(item) == headerTable.end()) return patterns;

        FPNode* node = headerTable[item].head;
        while (node) {
            vector<pair<string, int>> path;
            FPNode* current = node->parent;
            while (current && current->item != "NULL") {
                path.emplace_back(current->item, node->count); // Use node->count for path
                current = current->parent;
            }
            if (!path.empty()) {
                reverse(path.begin(), path.end());
                patterns.emplace_back(path, node->count);
            }
            node = node->next;
        }
        return patterns;
    }

    FPTree* buildConditionalFPTree(const vector<pair<vector<pair<string, int>>, int>>& patterns, int minsup) {
        FPTree* condTree = new FPTree();
        unordered_map<string, int> condFreq;

        // Calculate local frequency
        for (const auto& pattern : patterns) {
            int pathCount = pattern.second;
            for (const auto& item : pattern.first) {
                condFreq[item.first] += pathCount;
            }
        }

        // Build conditional transactions
        for (const auto& pattern : patterns) {
            vector<pair<string, int>> filteredPath;
            for (const auto& item : pattern.first) {
                if (condFreq[item.first] >= minsup) {
                    filteredPath.emplace_back(item.first, pattern.second);
                }
            }
            if (!filteredPath.empty()) {
                sort(filteredPath.begin(), filteredPath.end(),
                    [&](const pair<string, int>& a, const pair<string, int>& b) {
                        return condFreq[a.first] > condFreq[b.first] ||
                               (condFreq[a.first] == condFreq[b.first] && a.first < b.first);
                    });
                condTree->insertTransaction(filteredPath);
            }
        }
        return condTree;
    }

    vector<pair<string, HeaderTableEntry>> getSortedHeaderTable(int minsup) {
        vector<pair<string, HeaderTableEntry>> sortedItems;
        for (const auto& entry : headerTable) {
            if (entry.second.frequency >= minsup) {
                sortedItems.emplace_back(entry.first, entry.second);
            }
        }
        sort(sortedItems.begin(), sortedItems.end(),
            [](const pair<string, HeaderTableEntry>& a, const pair<string, HeaderTableEntry>& b) {
                return a.second.frequency > b.second.frequency ||
                       (a.second.frequency == b.second.frequency && a.first < b.first);
            });
        return sortedItems;
    }

    bool isEmpty() const {
        return root->children.empty();
    }

    void mineFPTree(int minsup, const vector<string>& prefix, vector<pair<vector<string>, int>>& result) {
        auto sortedItems = getSortedHeaderTable(minsup);
        for (const auto& entry : sortedItems) {
            string item = entry.first;
            int freq = entry.second.frequency;

            vector<string> newPrefix = prefix;
            newPrefix.push_back(item);
            result.emplace_back(newPrefix, freq);

            auto conditionalPatterns = getConditionalPatterns(item);
            FPTree* condTree = buildConditionalFPTree(conditionalPatterns, minsup);

            if (!condTree->isEmpty()) {
                condTree->mineFPTree(minsup, newPrefix, result);
            }
            delete condTree;
        }
    }
};

vector<string> split(const string& line, char delimiter) {
    vector<string> tokens;
    string token;
    stringstream ss(line);
    while (getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

int main() {
    string filename = "test_Data.hui";
    int minsup = 2;
    // string filename;
    // cout << "Enter file name: ";
    // cin >> filename;
    // int minsup;
    // cout << "Enter MinSup: ";
    // cin >> minsup;

    if (minsup <= 0) {
        cerr << "MinSup must be positive!" << endl;
        return 1;
    }

    auto start = high_resolution_clock::now();

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Cannot open file!" << endl;
        return 1;
    }

    string line;
    if (!getline(file, line)) {
        cerr << "Empty file!" << endl;
        file.close();
        return 1;
    }

    vector<string> header = split(line, ' ');
    if (header.size() < 2) {
        cerr << "Invalid header format!" << endl;
        file.close();
        return 1;
    }

    int numTransactions = stoi(header[0]);
    int numItems = stoi(header[1]);

    while (getline(file, line)) {
        vector<string> tokens = split(line, ' ');
        if (tokens.size() != 3) continue; // Skip invalid lines
        try {
            int transID = stoi(tokens[0]);
            string item = tokens[1];
            int count = stoi(tokens[2]);
            if (count <= 0) continue; // Skip invalid counts
            zippedTrans[transID].push_back({item, count});
            itemFrequency[item] += count;
        } catch (const exception& e) {
            cerr << "Error parsing line: " << line << endl;
            continue;
        }
    }
    file.close();

    FPTree tree;
    for (const auto& trans : zippedTrans) {
        vector<pair<string, int>> items = trans.second;
        vector<pair<string, int>> selectedTransaction;
        for (const auto& item : items) {
            if (itemFrequency[item.first] >= minsup) {
                selectedTransaction.push_back(item);
            }
        }
        if (!selectedTransaction.empty()) {
            sort(selectedTransaction.begin(), selectedTransaction.end(),
                [&](const pair<string, int>& a, const pair<string, int>& b) {
                    return itemFrequency[a.first] > itemFrequency[b.first] ||
                           (itemFrequency[a.first] == itemFrequency[b.first] && a.first < b.first);
                });
            tree.insertTransaction(selectedTransaction);
        }
    }

    vector<pair<vector<string>, int>> frequentItemsets;
    tree.mineFPTree(minsup, {}, frequentItemsets);
    // write to file
    remove("frequent_itemsets.txt");
    ofstream outFile("frequent_itemsets.txt");
    if (!outFile.is_open()) {
        cerr << "Cannot open output file!" << endl;
        return 1;
    }
    outFile << "Frequent Itemsets (MinSup = " << minsup << "):\n";
    for (const auto& itemset : frequentItemsets) {
        outFile << "{";
        for (size_t i = 0; i < itemset.first.size(); ++i) {
            outFile << itemset.first[i];
            if (i < itemset.first.size() - 1) outFile << ", ";
        }
        outFile << "} : " << itemset.second << endl;
    }
    outFile.close();

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);
    cout << "Execution time: " << duration.count() << " ms" << endl;

    return 0;
}
