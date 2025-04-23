#pragma once
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <random>
#include <chrono>
#include <bitset>

#define MAX_BLOCK_NUM2 (36 + 5)
const int discard_stage = -1;

#define MAX_PR (10)
#define MAX_C (8)
#define MAX_G (1000 + 5)
#define GEXTRA (500)
#define MAX_BLOCK_DIFF (3)
#define MAX_OBJECT_SIZE (5 + 1)
#define MAX_TAG_NUM (16 + 5)
#define MAX_DISK_NUM (10 + 1)
#define MAX_DISK_SIZE (16384 + 1)
#define MAX_REQUEST_NUM (30000000 + 1)
#define MAX_OBJECT_NUM (100000 + 1)
#define REP_NUM (3)
#define POINTER_NUM (2)
#define FRE_PER_SLICING (1800)
#define MAX_TIME (86400)
#define MAX_SLICING ((MAX_TIME - 1) / FRE_PER_SLICING + 5)
#define EXTRA_TIME (105)
#define MAX_TOTAL_TIME (MAX_TIME + EXTRA_TIME + 5)
const int INF = 0x3f3f3f3f;

const auto start_time = std::chrono::steady_clock::now();
double runtime();

double fscore(int x);
double gscore(int x);
double hscore(int x);

struct Request {
    int object_id;
    int cnt;
    int timestamp;
    bool is_activate;
    bool vis[MAX_OBJECT_SIZE];
    bool is_done;
};

struct Object {
    int replica[REP_NUM + 1];
    std::vector <int> unit[REP_NUM + 1];
    int size;
    int tag;
    bool is_delete;
    std::vector <int> reqs;
};

struct Block{
    int L, R;
    int tag;
    int init_tag;
    int tag_count[MAX_TAG_NUM];
    int count;
    int block_num;
    int finished;
};

class Solution {
    bool p2r[1 << MAX_PR][MAX_C];
    int p2rmp[65];
    int cost[MAX_C];
    int C = 0;
public:
    Solution();
    void init();
    void delete_action();
    void write_action();
    void read_action();
    void gc_action();
    std::vector <std::pair <int, int> > obj_tags;
    void calc_tag();
    void do_object_delete(std::vector <int> &object_unit, int disk_id, int size, int tag);
    void do_request_delete(int request_id);
    bool do_object_write_block(std::vector <int> &object_unit, int disk_id, int block_id, int size, int object_id);
    std::vector <bool> getoneclip(int disk_id, int st, int lst);
    int gettime(int disk_id, int st, int lst);

    int f[MAX_G + GEXTRA + 10][MAX_C];
    int fr[MAX_G + GEXTRA + 10][MAX_C];
    int _id[MAX_OBJECT_NUM];
    int lst[MAX_DISK_NUM][POINTER_NUM];

    std::mt19937 engine;
    int pid;

    std::vector<Request> request;
    Object object[MAX_OBJECT_NUM];
    std::vector <int> init_tag;
    int finished_count[MAX_OBJECT_NUM];

    int tag_disk[MAX_TAG_NUM][MAX_DISK_NUM];
    std::vector <std::pair <int, int> > tag_block[MAX_TAG_NUM];

    int T, M, N, V, G, K;
    std::pair <int, int> disk[MAX_DISK_NUM][MAX_DISK_SIZE];
    int disk_size[MAX_DISK_NUM];
    int disk_pointer[MAX_DISK_NUM][POINTER_NUM];
    std::vector <int> disk_reqs[MAX_DISK_NUM][MAX_DISK_SIZE];
    int disk_reqs_activate[MAX_DISK_NUM][MAX_DISK_SIZE];
    int timestamp;

    int savedata[MAX_TAG_NUM][MAX_TOTAL_TIME] = {};
    int deldata[MAX_TAG_NUM][MAX_TOTAL_TIME] = {};
    int readdata[MAX_TAG_NUM][MAX_TOTAL_TIME] = {};
    int max_suf[MAX_TAG_NUM][MAX_TOTAL_TIME] = {};

    std::vector <int> *del_object;
    std::vector <std::pair <int, std::pair <int, int> > > write_object[MAX_TOTAL_TIME];
    std::vector <std::pair <int, int> > *read;



    int block_size;
    Block disk_block[MAX_DISK_NUM][MAX_BLOCK_NUM2];
    std::vector <std::vector <std::pair <int, int> > > same_block;
    int save_count[MAX_TAG_NUM], read_count[MAX_TAG_NUM];
    int req_count[MAX_DISK_NUM][MAX_DISK_SIZE];



    std::deque <int> request_queue;
    bool cross_block[MAX_DISK_NUM][POINTER_NUM];
    unsigned obj_vis[MAX_OBJECT_NUM][MAX_OBJECT_SIZE];
    unsigned vcnt = 0;
    int last_vis[MAX_DISK_NUM][MAX_BLOCK_NUM2];
    int object_future[MAX_OBJECT_NUM];


    bool overload = false;
    bool valid_block[MAX_DISK_NUM][MAX_BLOCK_NUM2] = {};

    std::vector <int> output_delete[MAX_TOTAL_TIME];
    std::vector <std::pair <int, std::vector <std::vector <int> > > > output_write[MAX_TOTAL_TIME];
    std::string output_pointer[MAX_TOTAL_TIME][MAX_DISK_NUM][POINTER_NUM];
    std::vector <int> output_answer[MAX_TOTAL_TIME];
    std::vector <int> output_busy[MAX_TOTAL_TIME];
    std::vector <std::pair <int, int> > output_gc[MAX_TOTAL_TIME][MAX_DISK_NUM];

    double score;
};