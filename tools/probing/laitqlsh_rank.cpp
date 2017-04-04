////////////////////////////////////////////////////////////////////////////// /// Copyright (C) 2014 Gefu Tang <tanggefu@gmail.com>. All Rights Reserved.  /// /// This file is part of LSHBOX.  /// /// LSHBOX is free software: you can redistribute it and/or modify it under /// the terms of the GNU General Public License as published by the Free /// Software Foundation, either version 3 of the License, or(at your option) /// any later version.  ///
/// LSHBOX is distributed in the hope that it will be useful, but WITHOUT
/// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
/// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
/// more details.
///
/// You should have received a copy of the GNU General Public License along
/// with LSHBOX. If not, see <http://www.gnu.org/licenses/>.
///
/// @version 0.1
/// @author Gefu Tang & Zhifeng Xiao
/// @date 2014.6.30
//////////////////////////////////////////////////////////////////////////////

/**
 * @file laitqlsh_test.cpp
 *
 * @brief Example of using Iterative Quantization LSH index for L2 distance.
 */
#include <lshbox.h>
#include <lshbox/lsh/laitqlsh.h>
#include <map>
#include <fstream>
#include <lshbox/lsh/hammingranking.h>
// #include <lshbox/lsh/lossranking.h>

int main(int argc, char const *argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: ./laitqlsh_test data_file lsh_file benchmark_file" << std::endl;
        return -1;
    }
    std::cout << "Example of using Iterative Quantization" << std::endl << std::endl;
    typedef float DATATYPE;
    std::cout << "LOADING DATA ..." << std::endl;
    lshbox::timer timer;
    lshbox::Matrix<DATATYPE> data(argv[1]);
    std::cout << "LOAD TIME: " << timer.elapsed() << "s." << std::endl;
    std::cout << "CONSTRUCTING INDEX ..." << std::endl;
    timer.restart();
    std::string file(argv[2]);

    bool use_index = false;
    if (argc >= 5) { 
        std::string cmd4_index(argv[4]);
        if (cmd4_index == "1") {
            use_index = true;
        }
    }
    std::cout << "use_index: " << use_index << std::endl;

    lshbox::laItqLsh<DATATYPE> mylsh;
    if (use_index)
    {
        mylsh.load(file);
    }
    else
    {
        lshbox::laItqLsh<DATATYPE>::Parameter param;
        param.M = 1; // number of buckets in a table, useless in this example
        param.L = 1;  // number of tables
        param.D = data.getDim();
        param.N = 20;  // number of bits
        param.S = 1000000; // number of vectors
        param.I = 50;
        mylsh.reset(param);
        mylsh.train(data);
        mylsh.hash(data);
        mylsh.save(file);
        std::cout << "CONSTRUCTING TIME: " << timer.elapsed() << "s." << std::endl;
        return 0;
    }
    std::cout << "CONSTRUCTING TIME: " << timer.elapsed() << "s." << std::endl;
    std::cout << "LOADING BENCHMARK ..." << std::endl;
    timer.restart();
    lshbox::Matrix<DATATYPE>::Accessor accessor(data);
    lshbox::Metric<DATATYPE> metric(data.getDim(), L2_DIST);
    lshbox::Benchmark bench;
    std::string benchmark(argv[3]);
    bench.load(benchmark);
    unsigned K = bench.getK();

    // initialize scanner
    lshbox::Scanner<lshbox::Matrix<DATATYPE>::Accessor> initScanner(
        accessor,
        metric,

        K
    );
    std::cout << "LOADING TIME: " << timer.elapsed() << "s." << std::endl;
    std::cout << "RUNING QUERY ..." << std::endl;
    timer.restart();

    int maxProbedBK = 16;
    if (argc >= 6)
        maxProbedBK = std::atoi(argv[5]);

    std::ofstream fout("result.csv");
    fout << "probed buckets" << "," << "recall" << "," << "precision" << "," << "avg query time" << "\n";

    // initialize prober
    // typedef LossRanking<lshbox::Matrix<DATATYPE>::Accessor> PROBER;
    typedef HammingRanking<lshbox::Matrix<DATATYPE>::Accessor> PROBER;

    void* raw_memory = operator new[]( 
        sizeof(PROBER) * bench.getQ());
    PROBER* probers = static_cast<PROBER*>(raw_memory);
    for (int i = 0; i < bench.getQ(); ++i) {
        new(&probers[i]) PROBER(
            data[bench.getQuery(i)],
            initScanner,
            mylsh);
    }

    // probe
    for (int numBK = 1; numBK <= maxProbedBK; numBK *= 2) {
        lshbox::Stat cost, recall, precision;
        lshbox::progress_display pd(bench.getQ());
        timer.restart();
        for (unsigned i = 0; i != bench.getQ(); ++i)
        {
            // mylsh.queryRankingByLoss(data[bench.getQuery(i)], probers[i], numBK);
            mylsh.queryRankingByHamming(data[bench.getQuery(i)], probers[i], numBK);

            // scanner.reset(data[bench.getQuery(i)]);
            // mylsh.queryProbeByLoss(data[bench.getQuery(i)], probers[i], numBK);

            // parameter order wrong: float thisRecall = bench.getAnswer(i).recall(scanner.topk());
            // parameter order wrong: float thisPrecision = bench.getAnswer(i).precision(scanner.topk());

            lshbox::Scanner<lshbox::Matrix<DATATYPE>::Accessor> 
                scanner = probers[i].getScanner();
           // scanner = probers[i].getScanner();
            scanner.topk().genTopk(); // must getTopk for scanner, other wise will wrong
            float thisRecall = scanner.topk().recall(bench.getAnswer(i));

            // not true precision, library is wrong
            // float thisPrecision = scanner.topk().precision(bench.getAnswer(i));

            // modified version
            float matched = thisRecall * (K - 1); 
            float thisPrecision;
            assert(scanner.cnt() > 0);
            if(scanner.cnt() == 1)
                thisPrecision = 0;
            else
                thisPrecision = matched / (scanner.cnt() - 1);
            float thisCost = float(scanner.cnt()) / float(data.getSize());

            recall << thisRecall;
            precision << thisPrecision;
            cost << thisCost;
            ++pd;
        }
        std::cout << "MEAN QUERY TIME: " << timer.elapsed() / bench.getQ() << "s." << std::endl;
        std::cout << "RECALL   : " << recall.getAvg() << " +/- " << recall.getStd() << std::endl;
        std::cout << "COST     : " << cost.getAvg() << " +/- " << cost.getStd() << std::endl;
        std::cout << "PRECISION     : " << precision.getAvg() << " +/- " << precision.getStd() << std::endl;
        std::cout << "HASH TABLE SIZE    : " << mylsh.getTableSize() << std::endl;
        std::cout << "LARGEST BUCKET SIZE    : " << mylsh.getMaxBucketSize() << std::endl;

        fout << numBK << "," << recall.getAvg() << "," << precision.getAvg() << "," << timer.elapsed() / bench.getQ() << "\n" ;
    }
    fout.close();

    // release memory of prober;
    std::cout << "bench.getQ: " << bench.getQ() << std::endl;
    for (unsigned i = bench.getQ() - 1; i !=0; --i) {
        probers[i].~PROBER();
    }

    std::cout << "end of program" << std::endl;
    // operator delete[](raw_memory);
}
