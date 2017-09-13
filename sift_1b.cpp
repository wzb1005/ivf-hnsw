#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <chrono>
#include "hnswlib.h"

#include <map>
#include <unordered_set>
using namespace std;
using namespace hnswlib;

class StopW {
	std::chrono::steady_clock::time_point time_begin;
public:
	StopW() {
		time_begin = std::chrono::steady_clock::now();
	}
	float getElapsedTimeMicro() {
		std::chrono::steady_clock::time_point time_end = std::chrono::steady_clock::now();
		return (std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin).count());
	}
	void reset() {
		time_begin = std::chrono::steady_clock::now();
	}

};


/*
* Author:  David Robert Nadeau
* Site:    http://NadeauSoftware.com/
* License: Creative Commons Attribution 3.0 Unported License
*          http://creativecommons.org/licenses/by/3.0/deed.en_US
*/

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/resource.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#include <fcntl.h>
#include <procfs.h>

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#include <stdio.h>

#endif

#else
#error "Cannot define getPeakRSS( ) or getCurrentRSS( ) for an unknown OS."
#endif



/**
* Returns the peak (maximum so far) resident set size (physical
* memory use) measured in bytes, or zero if the value cannot be
* determined on this OS.
*/
size_t getPeakRSS()
{
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
	return (size_t)info.PeakWorkingSetSize;

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
	/* AIX and Solaris ------------------------------------------ */
	struct psinfo psinfo;
	int fd = -1;
	if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
		return (size_t)0L;      /* Can't open? */
	if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo))
	{
		close(fd);
		return (size_t)0L;      /* Can't read? */
	}
	close(fd);
	return (size_t)(psinfo.pr_rssize * 1024L);

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	/* BSD, Linux, and OSX -------------------------------------- */
	struct rusage rusage;
	getrusage(RUSAGE_SELF, &rusage);
#if defined(__APPLE__) && defined(__MACH__)
	return (size_t)rusage.ru_maxrss;
#else
	return (size_t)(rusage.ru_maxrss * 1024L);
#endif

#else
	/* Unknown OS ----------------------------------------------- */
	return (size_t)0L;          /* Unsupported. */
#endif
}


/**
* Returns the current resident set size (physical memory use) measured
* in bytes, or zero if the value cannot be determined on this OS.
*/
size_t getCurrentRSS()
{
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
	return (size_t)info.WorkingSetSize;

#elif defined(__APPLE__) && defined(__MACH__)
	/* OSX ------------------------------------------------------ */
	struct mach_task_basic_info info;
	mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
		(task_info_t)&info, &infoCount) != KERN_SUCCESS)
		return (size_t)0L;      /* Can't access? */
	return (size_t)info.resident_size;

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
	/* Linux ---------------------------------------------------- */
	long rss = 0L;
	FILE* fp = NULL;
	if ((fp = fopen("/proc/self/statm", "r")) == NULL)
		return (size_t)0L;      /* Can't open? */
	if (fscanf(fp, "%*s%ld", &rss) != 1)
	{
		fclose(fp);
		return (size_t)0L;      /* Can't read? */
	}
	fclose(fp);
	return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);

#else
	/* AIX, BSD, Solaris, and Unknown OS ------------------------ */
	return (size_t)0L;          /* Unsupported. */
#endif
}


template <typename dist_t>
static void get_gt(unsigned int *massQA, size_t qsize, vector<std::priority_queue< std::pair<dist_t, labeltype >>> &answers, size_t k = 1)
{
	(vector<std::priority_queue< std::pair<dist_t, labeltype >>>(qsize)).swap(answers);
    cout << qsize << "\n";
	for (int i = 0; i < qsize; i++) {
		for (int j = 0; j < k; j++) {
			answers[i].emplace(0.0f, massQA[1000*i + j]);
		}
	}
}

template <typename dist_t>
static float test_approx(unsigned char *massQ, size_t qsize, HierarchicalNSW<dist_t> &appr_alg,
                         size_t vecdim, vector<std::priority_queue< std::pair<dist_t, labeltype >>> &answers,
                         size_t k, unordered_set<int> &cluster_idx_set, bool pq = false)
{
	size_t correct = 0;

	//uncomment to test in parallel mode:
	//#pragma omp parallel for
	for (int i = 0; i < qsize; i++) {
		std::priority_queue< std::pair<dist_t, labeltype >> result;
        if (pq)
            result = appr_alg.searchKnn(massQ + vecdim*i, k, cluster_idx_set, i);
        else
            result = appr_alg.searchKnn(massQ + vecdim*i, k, cluster_idx_set);

		std::priority_queue< std::pair<dist_t, labeltype >> gt(answers[i]);
		unordered_set <labeltype> g;

        float dist2gt = appr_alg.space->fstdistfunc((void*)(massQ + vecdim*i),//appr_alg.getDataByInternalId(gt.top().second),
                                                     appr_alg.getDataByInternalId(appr_alg.enterpoint0));
        appr_alg.nev9zka += dist2gt / qsize;

		while (gt.size()) {
			g.insert(gt.top().second);
			gt.pop();
		}

		while (result.size()) {
			if (g.find(result.top().second) != g.end())
				correct++;
			result.pop();
		}
	}
	return 1.0f*correct / qsize;
}

template <typename dist_t>
static void test_vs_recall(unsigned char *massQ, size_t qsize, HierarchicalNSW<dist_t> &appr_alg,
                           size_t vecdim, vector<std::priority_queue< std::pair<dist_t, labeltype >>> &answers,
                           size_t k, unordered_set<int> &cluster_idx_set, bool pq = false)
{
	vector<size_t> efs; //= {30, 100, 460};
    if (k < 30) {
        for (int i = k; i < 30; i++) efs.push_back(i);
        for (int i = 30; i < 100; i += 10) efs.push_back(i);
        for (int i = 100; i <= 500; i += 40) efs.push_back(i);
    }
	else if (k < 100) {
        for (int i = k; i < 100; i += 10) efs.push_back(i);
        for (int i = 100; i <= 500; i += 40) efs.push_back(i);
    } else
        for (int i = k; i <= 500; i += 40) efs.push_back(i);

	for (size_t ef : efs) {
		appr_alg.ef_ = ef;
        appr_alg.dist_calc = 0;
        appr_alg.nev9zka = 0.0;
        appr_alg.hops = 0.0;
        appr_alg.hops0 = 0.0;
		StopW stopw = StopW();

		float recall = test_approx<dist_t>(massQ, qsize, appr_alg, vecdim, answers, k, cluster_idx_set, pq);
		float time_us_per_query = stopw.getElapsedTimeMicro() / qsize;
		float avr_dist_count = appr_alg.dist_calc*1.f / qsize;
		cout << ef << "\t" << recall << "\t" << time_us_per_query << " us\t" << avr_dist_count << " dcs\t" << appr_alg.hops0 + appr_alg.hops << " hps\n";
		if (recall > 1.0) {
			cout << recall << "\t" << time_us_per_query << " us\t" << avr_dist_count << " dcs\n";
			break;
		}
    }
    cout << "Average hops on levels 1+: " << appr_alg.hops << endl;
    cout << "Average distance from 0 level entry point to query: " << appr_alg.nev9zka << endl;
}

inline bool exists_test(const std::string& name) {
	ifstream f(name.c_str());
	return f.good();
}

/**
 * Print Configuration
 **/
template <typename dist_t>
static void printInfo(HierarchicalNSW<dist_t> *hnsw)
{
    if (hnsw == NULL)
        throw "Empty HNSW";

    cout << "Information about constructed HNSW" << endl;
    cout << "efConstruction: " << hnsw->efConstruction_<< endl;

    map<char, int> table = map<char, int>();
    for (char layerNum : hnsw->elementLevels) {
        if (table.count(layerNum) == 0) {
            table[layerNum] = 1;
        } else {
            table[layerNum]++;
        }
    }
    for (auto elementsPerLayer : table){
        cout << "Number of elements on the " << (int) elementsPerLayer.first << "level: " << elementsPerLayer.second << endl;
    }
}

/**
 * Main SIFT Test Function
*/
//void sift_test1B() {
//    const int subset_size_milllions = 100;
//    const int efConstruction = 240;
//    const int M = 4;//16;
//    const int M_cluster = 16;
//
//    const size_t clustersize = 5263157;
//    const vector<size_t> elements_per_layer = {100000000, 5000000, 250000, 12500, 625, 32};
//
//    const size_t vecsize = subset_size_milllions * 1000000;
//    const size_t qsize = 10000;
//    const size_t vecdim = 128;
//
//    char path_index[1024];
//    char path_gt[1024];
//    const char *path_q = "/sata2/dbaranchuk/bigann/bigann_query.bvecs";
//    const char *path_data = "/sata2/dbaranchuk/bigann/bigann_base.bvecs";
//    const char *path_clusters = "/sata2/dbaranchuk/synthetic_100m_5m/bigann_base_100m_clusters.bvecs";
//
//    sprintf(path_index, "/sata2/dbaranchuk/synthetic_100m_5m/sift%dm_ef_%d_M_%d_cM_%d_1layer.bin", subset_size_milllions, efConstruction, M,
//            M_cluster);
//    sprintf(path_gt, "/sata2/dbaranchuk/bigann/gnd/idx_%dM.ivecs", subset_size_milllions);
//
//    cout << "Loading GT:\n";
//    ifstream inputGT(path_gt, ios::binary);
//    unsigned int *massQA = new unsigned int[qsize * 1000];
//    for (int i = 0; i < qsize; i++) {
//        int t;
//        inputGT.read((char *) &t, 4);
//        inputGT.read((char *) (massQA + 1000 * i), t * 4);
//        if (t != 1000) {
//            cout << "err";
//            exit(1);
//        }
//    }
//    inputGT.close();
//
//    cout << "Loading queries:\n";
//    unsigned char massQ[qsize * vecdim];
//    ifstream inputQ(path_q, ios::binary);
//
//    for (int i = 0; i < qsize; i++) {
//        int in = 0;
//        inputQ.read((char *) &in, 4);
//        if (in != vecdim) {
//            cout << "file error";
//            exit(1);
//        }
//        inputQ.read((char *) (massQ + i * vecdim), in);
//    }
//    inputQ.close();
//
//    L2SpaceI l2space(vecdim);
//
//    HierarchicalNSW<int> *appr_alg;
//    if (exists_test(path_index)) {
//        appr_alg = new HierarchicalNSW<int>(&l2space, path_index, false);
//        cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";
//    } else {
//        cout << "Building index:\n";
//        unsigned char massb[vecdim];
//
//        int j1 = 0, in = 0;
//        appr_alg = new HierarchicalNSW<int>(&l2space, vecsize, M, efConstruction, clustersize, M_cluster);
//        appr_alg->setElementLevels(elements_per_layer);
//
//        StopW stopw = StopW();
//        StopW stopw_full = StopW();
//
//        cout << "Adding elements\n";
//        ifstream input(path_data, ios::binary);
//
//        input.read((char *) &in, 4);
//        if (in != vecdim) {
//            cout << "file error\n";
//            exit(1);
//        }
//        input.read((char *) massb, in);
//
//        appr_alg->addPoint((void *) (massb), (size_t) j1);
//
//        size_t report_every = 1000000;
//#pragma omp parallel for num_threads(32)
//        for (int i = 0; i < vecsize; i++) {
//            unsigned char massb[vecdim];
//#pragma omp critical
//            {
//                input.read((char *) &in, 4);
//                if (in != vecdim) {
//                    cout << "file error";
//                    exit(1);
//                }
//                input.read((char *) massb, in);
//                j1++;
//                if ((j1 - clustersize) % report_every == 0) {
//                    cout << (j1 - clustersize) / (0.01 * vecsize) << " %, "
//                         << report_every / (1000.0 * 1e-6 * stopw.getElapsedTimeMicro()) << " kips " << " Mem: "
//                         << getCurrentRSS() / 1000000 << " Mb \n";
//                    stopw.reset();
//                }
//            }
//            appr_alg->addPoint((void *) (massb), (size_t) j1);
//        }
//        input.close();
//        cout << "Build time:" << 1e-6 * stopw_full.getElapsedTimeMicro() << "  seconds\n";
//        appr_alg->SaveIndex(path_index);
//    }
//    printInfo(appr_alg);
//
//    //FILE *fin = fopen("/sata2/dbaranchuk/synthetic_100m_5m/new_cluster_idx.dat", "rb");
//    //int *cluster_idx_table = new int[clustersize];
//    //int ret = fread(cluster_idx_table, sizeof(int), clustersize, fin);
//    unordered_set<int> cluster_idx_set;
//    for (int i = 0; i < clustersize; i++) {
//    //    cluster_idx_set.insert(cluster_idx_table[i]);
//        cluster_idx_set.insert(i);
//    }
//    //delete cluster_idx_table;
//    //fclose(fin);
//    //
//
//	vector<std::priority_queue< std::pair<int, labeltype >>> answers;
//	size_t k = 1;
//	cout << "Parsing gt:\n";
//	get_gt<int>(massQA, qsize, answers, k);
//	cout << "Loaded gt\n";
//    test_vs_recall<int>(massQ, qsize, *appr_alg, vecdim, answers, k, cluster_idx_set);
//	cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";
//
//    delete massQA;
//}

template<typename dist_t>
static void _hnsw_test(const char *path_codebooks, const char *path_tables, const char *path_data, const char *path_q,
                       const char *path_gt, const char *path_info, const char *path_edges,
                       L2SpaceType l2SpaceType,
                       const int k, const int vecsize, const int qsize,
                       const int vecdim, const int efConstruction, const int M)
{
    const int M_PQ = 16;
    const bool PQ = (path_codebooks && path_tables);

    const map<size_t, size_t> M_map = {{vecsize, M}};//{{50000000, 32}, {100000000, 24}, {150000000, 16}, {800000000, 8}, {900000000, 6}, {1000000000, 4}};
    const vector<size_t> elements_per_level = {vecsize};//{947368422, 50000000, 2500000, 125000, 6250, 312, 16};

    cout << "Loading GT:\n";
    const int gt_size = 1000;
    ifstream inputGT(path_gt, ios::binary);
    unsigned int *massQA = new unsigned int[qsize * gt_size];
    for (int i = 0; i < qsize; i++) {
        int t;
        inputGT.read((char *)&t, 4);
        inputGT.read((char *)(massQA + gt_size*i), t * 4);
        if (t != gt_size) {
            cout << "error\n";
            exit(1);
        }
    }

    cout << "Loading queries:\n";
    unsigned char massQ[qsize * vecdim];
    ifstream inputQ(path_q, ios::binary);

    for (int i = 0; i < qsize; i++) {
        int in = 0;
        inputQ.read((char *)&in, 4);
        if (in != vecdim)
        {
            cout << "file error";
            exit(1);
        }
        inputQ.read((char *)(massQ + i*vecdim), in);
    }
    inputQ.close();


    SpaceInterface *l2space;

    switch(l2SpaceType) {
        case L2SpaceType::PQ:
            l2space = new L2SpacePQ(vecdim, M_PQ, 256);
            dynamic_cast<L2SpacePQ *>(l2space)->set_codebooks(path_codebooks);
            dynamic_cast<L2SpacePQ *>(l2space)->set_construction_tables(path_tables);
            dynamic_cast<L2SpacePQ *>(l2space)->compute_query_tables(massQ, qsize);
            break;
        case L2SpaceType::Float:
            l2space = new L2Space(vecdim);
            break;
        case L2SpaceType::Int:
            l2space = new L2SpaceI(vecdim);
            break;
    }

    HierarchicalNSW<dist_t> *appr_alg;
    if (exists_test(path_info) && exists_test(path_edges) && exists_test(path_info)) {
        appr_alg = new HierarchicalNSW<dist_t>(l2space, path_info, path_data, path_edges);
        cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";
    } else {
        cout << "Building index:\n";
        unsigned char massb[PQ ? M_PQ : vecdim];

        int j1 = 0, in = 0;
        appr_alg = new HierarchicalNSW<dist_t>(l2space, vecsize, M_map, efConstruction);
        appr_alg->setElementLevels(elements_per_level);

        StopW stopw = StopW();
        StopW stopw_full = StopW();

        cout << "Adding elements\n";
        ifstream input(path_data, ios::binary);
        input.read((char *) &in, 4);
        if (in != (PQ ? M_PQ : vecdim)) {
            cout << (PQ ? M_PQ : vecdim) << endl;
            cout << "file error\n";
            exit(1);
        }
        input.read((char *) massb, in);

        appr_alg->addPoint((void *) (massb), (size_t) j1);

        size_t report_every = 1000000;
#pragma omp parallel for num_threads(32)
        for (int i = 1; i < vecsize; i++) {
            unsigned char massb[PQ ? M_PQ : vecdim];
#pragma omp critical
            {
                input.read((char *) &in, 4);
                if (in != (PQ ? M_PQ : vecdim)) {
                    cout << "file error";
                    exit(1);
                }
                input.read((char *) massb, in);
                j1++;
                if (j1 % report_every == 0) {
                    cout << j1 / (0.01 * vecsize) << " %, "
                         << report_every / (1000.0 * 1e-6 * stopw.getElapsedTimeMicro()) << " kips " << " Mem: "
                         << getCurrentRSS() / 1000000 << " Mb \n";
                    stopw.reset();
                }
            }
            appr_alg->addPoint((void *) (massb), (size_t) j1);
        }
        input.close();
        cout << "Build time:" << 1e-6 * stopw_full.getElapsedTimeMicro() << "  seconds\n";
        //appr_alg->SaveIndex(path_index);
        appr_alg->SaveInfo(path_info);
        appr_alg->SaveEdges(path_edges);
    }
    printInfo(appr_alg);
    //appr_alg->printListsize();

    unordered_set<int> cluster_idx_set;
    vector<std::priority_queue< std::pair<dist_t, labeltype >>> answers;

    cout << "Parsing gt:\n";
    get_gt<dist_t>(massQA, qsize, answers);
    cout << "Loaded gt\n";
    test_vs_recall<dist_t>(massQ, qsize, *appr_alg, vecdim, answers, k, cluster_idx_set, PQ);
    cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";

    delete massQA;
    delete l2space;
}


void hnsw_test(const char *l2space_type,
               const char *path_codebooks, const char *path_tables, const char *path_data, const char *path_q,
               const char *path_gt, const char *path_info, const char *path_edges,
               const int k, const int vecsize, const int qsize,
               const int vecdim, const int efConstruction, const int M)
{
    char path_gt_[1024], path_edges_[1024], path_info_[1024];
    const int subset_size_milllions = 10;
    const int M_PQ = 16;

    if (!path_q) path_q = "/sata2/dbaranchuk/bigann/bigann_query.bvecs";
    if (!path_data) path_data = "/sata2/dbaranchuk/bigann/bigann_base.bvecs";
    //if (!path_codebooks) path_codebooks = "/sata2/dbaranchuk/bigann/base1B_M16/codebooks.fvecs";
    //if (!path_tables) path_tables = "/sata2/dbaranchuk/bigann/base1B_M16/distance_tables.dat";
    if (!path_gt){
        sprintf(path_gt_, "/sata2/dbaranchuk/bigann/gnd/idx_%dM.ivecs", subset_size_milllions);
        path_gt = path_gt_;
    }
    if (!path_edges) {
        sprintf(path_edges_, "/sata2/dbaranchuk/bigann/sift%dm_ef%d_M%d_edges.ivecs",
                subset_size_milllions, efConstruction, M);
        path_edges = path_edges_;
    }
    if (!path_info) {
        sprintf(path_info_, "/sata2/dbaranchuk/bigann/sift%dm_ef%d_M%d_info.bin",
                subset_size_milllions, efConstruction, M);
        path_info = path_info_;
    }
    //if (!path_edges) sprintf(path_edges, "/sata2/dbaranchuk/bigann/base1B_M16/sift%dm_ef%d_M%d_edges.ivecs",
    //                         subset_size_milllions, efConstruction, M);
    //if (!path_info) sprintf(path_info, "/sata2/dbaranchuk/bigann/base1B_M16/sift%dm_ef%d_M%d_info.bin",
    //                        subset_size_milllions, efConstruction, M);


    if (!strcmp (l2space_type, "int")) {
        if (path_codebooks && path_tables) {
            std::cerr << "Use l2space_type = float for PQ" << std::endl;
            exit(1);
        }
        _hnsw_test<int>(path_codebooks, path_tables, path_data, path_q,
                        path_gt, path_info, path_edges,
                        L2SpaceType::Int,
                        k, vecsize, qsize, vecdim, efConstruction, M);
    } else if (!strcmp (l2space_type, "float"))
        _hnsw_test<float>(path_codebooks, path_tables, path_data, path_q,
                          path_gt, path_info, path_edges,
                          (path_codebooks && path_tables) ? L2SpaceType::PQ : L2SpaceType::Float,
                          k, vecsize, qsize, vecdim, efConstruction, M);
    else {
        std::cerr << "Available l2space_type: float or int" << std::endl;
        exit(1);
    }
}











































//void sift_test1B_PQ()
//{
//    const int subset_size_milllions = 1000;
//    const int efConstruction = 500;
//    const int M = 4;
//    const int M_PQ = 16;
//    const int M_cluster = 20;
//    const int k = 100;
//
//    const size_t clustersize = 52631578;
//    const vector<size_t> elements_per_level = {947368422, 50000000, 2500000, 125000, 6250, 312, 16};
//
//    const size_t vecsize = 947368422;//subset_size_milllions * 1000000;
//    const size_t qsize = 10000;
//    const size_t vecdim = 128;
//
//    char path_index[1024];
//    char path_gt[1024];
//    const char *path_q = "/sata2/dbaranchuk/bigann/bigann_query.bvecs";
//    const char *path_data = "/sata2/dbaranchuk/bigann/base1B_M16/bigann_base_pq.bvecs";
//    const char *path_clusters = "/sata2/dbaranchuk/bigann/base1B_M16/bigann_base_100m_clusters_pq.bvecs";
//
//    const char *path_codebooks = "/sata2/dbaranchuk/bigann/base1B_M16/codebooks.fvecs";
//    const char *path_tables = "/sata2/dbaranchuk/bigann/base1B_M16/distance_tables.dat";
//
//    sprintf(path_index, "/sata2/dbaranchuk/bigann/base1B_M%d/sift%dm_ef_%d_M%d_cM%d.bin", M_PQ, subset_size_milllions, efConstruction, M, M_cluster);
//    sprintf(path_gt,"/sata2/dbaranchuk/bigann/gnd/idx_%dM.ivecs", subset_size_milllions);
//    //sprintf(path_gt,"/sata2/dbaranchuk/bigann/base1B_M%d/idx_%dM_pq.ivecs", M_PQ, subset_size_milllions);
//
//    cout << "Loading GT:\n";
//    const int gt_size = 1000;
//    ifstream inputGT(path_gt, ios::binary);
//    unsigned int *massQA = new unsigned int[qsize * gt_size];
//    for (int i = 0; i < qsize; i++) {
//        int t;
//        inputGT.read((char *)&t, 4);
//        inputGT.read((char *)(massQA + gt_size*i), t * 4);
//        if (t != gt_size) {
//            cout << "error\n";
//            exit(1);
//        }
//    }
//
//    cout << "Loading queries:\n";
//    unsigned char massQ[qsize * vecdim];
//    ifstream inputQ(path_q, ios::binary);
//
//    for (int i = 0; i < qsize; i++) {
//        int in = 0;
//        inputQ.read((char *)&in, 4);
//        if (in != vecdim)
//        {
//            cout << "file error";
//            exit(1);
//        }
//        inputQ.read((char *)(massQ + i*vecdim), in);
//    }
//    inputQ.close();
//
//    L2SpacePQ l2space(vecdim, M_PQ, 256);
//
//    l2space.set_codebooks(path_codebooks);
//    l2space.set_construction_tables(path_tables);
//    l2space.compute_query_tables(massQ, qsize);
//
//    HierarchicalNSW<float> *appr_alg;
//    if (exists_test(path_index)) {
//        appr_alg = new HierarchicalNSW<float>(&l2space, path_index, false);
//        cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";
//    } else {
//        cout << "Building index:\n";
//        unsigned char massb[M_PQ];
//
//        int j1 = 0, in = 0;
//        appr_alg = new HierarchicalNSW<float>(&l2space, vecsize, M, efConstruction, clustersize, M_cluster);
//        appr_alg->setElementLevels(elements_per_level);
//
//        StopW stopw = StopW();
//        StopW stopw_full = StopW();
//
////        cout << "Adding clustets:\n";
////        ifstream inputC(path_clusters, ios::binary);
////        inputC.read((char *) &in, 4);
////        if (in != M_PQ) {
////            cout << "file error\n";
////            exit(1);
////        }
////        inputC.read((char *) massb, in);
////        appr_alg->addPoint((void *) (massb), (size_t) j1);
////
////#pragma omp parallel for
////        for (int i = 1; i < clustersize; i++) {
////            unsigned char massb[M_PQ];
////#pragma omp critical
////            {
////                inputC.read((char *) &in, 4);
////                if (in != M_PQ) {
////                    cout << "file error";
////                    exit(1);
////                }
////                inputC.read((char *) massb, in);
////                j1++;
////            }
////            appr_alg->addPoint((void *) (massb), (size_t) j1);
////        }
////        inputC.close();
////        cout << "Clusters have been added" << endl;
//
//        cout << "Adding elements\n";
//        ifstream input(path_data, ios::binary);
//        //
//        input.read((char *) &in, 4);
//        if (in != M_PQ) {
//            cout << "file error\n";
//            exit(1);
//        }
//        input.read((char *) massb, in);
//
//        appr_alg->addPoint((void *) (massb), (size_t) j1);
//        //
//        size_t report_every = 1000000;
//#pragma omp parallel for num_threads(32)
//        for (int i = 1; i < clustersize + vecsize; i++) {
//            unsigned char massb[M_PQ];
//#pragma omp critical
//            {
//                input.read((char *) &in, 4);
//                if (in != M_PQ) {
//                    cout << "file error";
//                    exit(1);
//                }
//                input.read((char *) massb, in);
//                j1++;
//                if (j1 % report_every == 0) {
//                    cout << j1 / (0.01 * (clustersize + vecsize)) << " %, "
//                         << report_every / (1000.0 * 1e-6 * stopw.getElapsedTimeMicro()) << " kips " << " Mem: "
//                         << getCurrentRSS() / 1000000 << " Mb \n";
//                    stopw.reset();
//                }
//            }
//            appr_alg->addPoint((void *) (massb), (size_t) j1);
//        }
//        input.close();
//        cout << "Build time:" << 1e-6 * stopw_full.getElapsedTimeMicro() << "  seconds\n";
//        appr_alg->SaveIndex(path_index);
//    }
//    printInfo(appr_alg);
//    appr_alg->printListsize();
//
//    unordered_set<int> cluster_idx_set;
//    //for (int i = 0; i < clustersize; i++)
//    //    cluster_idx_set.insert(i);
//
//    vector<std::priority_queue< std::pair<float, labeltype >>> answers;
//
//    cout << "Parsing gt:\n";
//    get_gt<float>(massQA, qsize, answers);
//    cout << "Loaded gt\n";
//    test_vs_recall<float>(massQ, qsize, *appr_alg, vecdim, answers, k, cluster_idx_set, true);
//    cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";
//
//    delete massQA;
//}