#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include <functional>
#include <cmath>
#include <thread>
#include <math.h>
#include <assert.h>
#include <utility>
#include <fstream>
using namespace std;

namespace lshbox {
class IdAndDstPair {
    public:
        float distance;
        int id;

        IdAndDstPair(int id, float dst) {
            this->id = id;
            this->distance = dst;
        }
};

class MaxHeapCMP{
    public:
        bool operator()(const IdAndDstPair& a, const IdAndDstPair& b) {
            if (a.distance != b.distance) 
                return a.distance < b.distance;
            else 
                return a.id < b.id;
        }
};

class TopK {
    public:
        TopK(int K) {
            K_ = K; 
        }

        void insert(const IdAndDstPair& pair) {
            if (maxHeap_.size() < K_)
                maxHeap_.push(pair);
            else {
                if (pair.distance < maxHeap_.top().distance) {
                    maxHeap_.pop();
                    maxHeap_.push(pair);
                }
            }
        }

        void insert(const pair<int, float>& p) {
            insert(IdAndDstPair(p.first, p.second));
        }

        void collect(const vector<IdAndDstPair>& pairs) {
            for(const auto& pair : pairs) {
                insert(pair);
            }
        }

        void collect(const vector<pair<int, float>>& pairs) {
            for(const auto& pair : pairs) {
                insert(IdAndDstPair(pair.first, pair.second));
            }
        }

        vector<IdAndDstPair> getTopK() const {
            priority_queue<IdAndDstPair, vector<IdAndDstPair>, MaxHeapCMP> heap = maxHeap_;
            vector<IdAndDstPair> results;
            while (!heap.empty()) {
                results.push_back(heap.top());
                heap.pop();
            }
            std::sort(results.begin(), results.end(), MaxHeapCMP());
            return results;
        }

        vector<pair<int, float>> getTopKPairs() const {
            priority_queue<IdAndDstPair, vector<IdAndDstPair>, MaxHeapCMP> heap = maxHeap_;
            vector<pair<int, float>> results;
            while (!heap.empty()) {
                results.push_back(std::make_pair(heap.top().id, heap.top().distance));
                heap.pop();
            }
            std::sort(
                results.begin(),
                results.end(), 
                [](const pair<int, float>& a, const pair<int, float>& b) {
                    if (a.second != b.second) {return a.second < b.second;}
                    else return a.first < b.first;
            });
            return results;
        }

        int getK() const {
            return K_;
        }
    private:
        int K_;
        priority_queue<IdAndDstPair, vector<IdAndDstPair>, MaxHeapCMP> maxHeap_;
};

float calEuclideanDist(const vector<float>& query, const vector<float>& item) {
    float l2Dst = 0;
    assert(query.size() == item.size());
    for (int i = 0; i < query.size(); ++i) {
        l2Dst += (query[i] - item[i]) * (query[i] - item[i]);
    }
    return sqrt(l2Dst);
}

static float calAngularDist(const vector<float>& query, const vector<float>& item) {
    float cosDst = 0;
    assert(query.size() == item.size());
    float qNorm = 0;
    float iNorm = 0;
    for (int i = 0; i < query.size(); ++i) {
        qNorm += query[i] * query[i];
        iNorm += item[i] * item[i];
    }
    qNorm = sqrt(qNorm);
    iNorm = sqrt(iNorm);
    for (int i = 0; i < query.size(); ++i) {
        cosDst += query[i] * item[i];
    } 
    cosDst /= (qNorm * iNorm); 
    return acos(cosDst);
}

static float calInnerProductDist(const vector<float >& query, const vector<float >& item) {
    float ipDist = 0;
    assert(query.size() == item.size());
    for (int i = 0; i < query.size(); ++i) {
        ipDist -= (query[i] * item[i]);
    }
    return ipDist;
}

template<typename FeatureType>
class GTQuery {
    public: 
        vector<FeatureType> content;
        TopK topk;
        std::function<float(const vector<FeatureType>&, const vector<FeatureType>&)> distor;

        GTQuery(const vector<FeatureType>& cont, int K, std::function<float(const vector<FeatureType>&, const vector<FeatureType>&)> functor) : topk(K){
            this->content = cont;
            this->distor = functor;
        }

        virtual void evaluate(const vector<FeatureType>& item, int itemId) {
            float distance;
            distance = distor(this->content, item);
            topk.insert(IdAndDstPair(itemId, distance));
        }

        vector<IdAndDstPair> getTopK() const {
            return this->topk.getTopK();
        }

        int getK() const {
            return topk.getK();
        }
};

template<typename FeatureType>
void updateQueries(vector<GTQuery<FeatureType>*> queries, const vector<vector<FeatureType>>* itemsPtr, int itemStartIdx) {
    for (auto& query: queries) {
        for (int i = 0; i < itemsPtr->size(); ++i) {
            query->evaluate((*itemsPtr)[i], itemStartIdx + i);
        }
    }
}

template<typename FeatureType>
void updateAll(vector<GTQuery<FeatureType>>& queries, const vector<vector<FeatureType>>& items, int itemStartIdx, int numThreads = 4) {
    vector<thread> threads;
    int numQueriesPerThread = queries.size() / numThreads + 1;

    int queryIdx = 0;
    vector<GTQuery<FeatureType>*> queryLinks;
    while(queryIdx < queries.size()) {
        queryLinks.push_back(&queries[queryIdx++]);
        if (queryLinks.size() == numQueriesPerThread) {
            threads.push_back(thread(updateQueries<FeatureType>, queryLinks, &items, itemStartIdx));
            queryLinks.clear();
        }
    }
    threads.push_back(thread(updateQueries<FeatureType>, queryLinks, &items, itemStartIdx));
    for (int i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }
}

class GroundWriter {
public:
    template<typename FeatureType>
    void writeLSHBOX(const char* lshboxBenchFileName, const vector<GTQuery<FeatureType>>& queryObjs) {
        // lshbox file
        ofstream lshboxFout(lshboxBenchFileName);
        if (!lshboxFout) {
            cout << "cannot create output file " << lshboxBenchFileName << endl;
            assert(false);
        }
        int K = queryObjs[0].getK();
        lshboxFout << queryObjs.size() << "\t" << K << endl;
        for (int i = 0; i < queryObjs.size(); ++i) {
            assert(queryObjs[i].getK() == K);
            lshboxFout << i << "\t";
            vector<IdAndDstPair> topker = queryObjs[i].getTopK();
            for (int idx = 0; idx < topker.size(); ++idx) {
                lshboxFout << topker[idx].id << "\t" << topker[idx].distance << "\t";
            }
            lshboxFout << endl;
        }
        lshboxFout.close();
        cout << "lshbox groundtruth are written into " << lshboxBenchFileName << endl;
    }

    template<typename FeatureType>
    void writeIVECS(const char* ivecsBenchFileName, const vector<GTQuery<FeatureType>>& queryObjs) {
        // ivecs file
        ofstream fout(ivecsBenchFileName, ios::binary);
        if (!fout) {
            cout << "cannot create output file " << ivecsBenchFileName << endl;
            assert(false);
        }
        int K = queryObjs[0].getK();
        for (int i = 0; i < queryObjs.size(); ++i) {
            assert(queryObjs[i].getK() == K);
            fout.write((char*)&K, sizeof(int));
            vector<IdAndDstPair> topker = queryObjs[i].getTopK();
            for (int idx = 0; idx < topker.size(); ++idx) {
                fout.write((char*)&topker[idx].id, sizeof(int));
            }
        }
        fout.close();
        cout << "ivecs groundtruth are written into " << ivecsBenchFileName << endl;
     }
};
}
