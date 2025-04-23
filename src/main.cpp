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
#include <iomanip>
#include <thread>
#include <mutex>
#include "solution.h"

using namespace std;

const int B = 27;
#define END_TIME (450)
#define REP_TIME (100)
#define THREAD_NUM (2)

#define JUMP_MUL (1.4)
#define JUMP_MUL_FAC (0.8)
#define TAG_PREDICT_RATE (0.5)
#define DISCARD_DECREASE (0.3)
#define DISCARD_RATE (0.8)
#define DISCARD_TIME (200)
#define TIME_BIAS (60)
#define TAG_SLICING (1800)
#define MAX_BLOCK_NUM (B + 5)
#define MAX_TAG_SLICING ((MAX_TIME - 1) / TAG_SLICING + 5)

int f[MAX_G + GEXTRA + 10][MAX_C];
int fr[MAX_G + GEXTRA + 10][MAX_C];

bool p2r[1 << MAX_PR][MAX_C];
int p2rmp[65];
int cost[MAX_C];
int C = 0;
auto init_p2r = [](){
    cost[0] = 64;
    while(cost[C] != 16){
        C++;
        cost[C] = std::max(16, (int)ceil(cost[C - 1] * 0.8));
        p2rmp[cost[C]] = C;
    }

    for(int s = 0; s < (1 << MAX_PR); s++){
        for(int t = 0; t <= C; t++){
            for(int i = 0; i <= MAX_PR; i++){
                for(int j = 0; j <= C; j++){
                    f[i][j] = INF;
                }
            }
            f[0][t] = 0;
            for(int i = 0; i < MAX_PR; i++){
                int p = (s >> i) & 1;
                for(int j = 0; j <= C; j++){
                    if(f[i][j] == INF) continue;
                    int nj = j == C ? j : j + 1;
                    if(f[i + 1][nj] > f[i][j] + cost[j]){
                        f[i + 1][nj] = f[i][j] + cost[j];
                        fr[i + 1][nj] = j;
                    }
                    if(!((s >> i) & 1)){
                        if(f[i + 1][0] > f[i][j] + 1){
                            f[i + 1][0] = f[i][j] + 1;
                            fr[i + 1][0] = j;
                        }
                    }
                }
            }
            int mx = INF;
            int resy = 0;
            for(int j = 0; j <= C; j++){
                if(f[MAX_PR][j] <= mx){
                    mx = f[MAX_PR][j];
                    resy = j;
                }
            }
            for(int i = MAX_PR, j = resy; i > 0; i--){
                if(j == 0) p2r[s][t] = false;
                else p2r[s][t] = true;
                j = fr[i][j];
            }
        }
    }
    return true;
}();

mt19937 engine;

vector <Request> request;
Object object[MAX_OBJECT_NUM];

int tag_disk[MAX_TAG_NUM][MAX_DISK_NUM];
vector <pair <int, int> > tag_block[MAX_TAG_NUM];

int T, M, N, V, G, K;
pair <int, int> disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_size[MAX_DISK_NUM];
int disk_pointer[MAX_DISK_NUM][POINTER_NUM];
vector <int> disk_reqs[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_reqs_activate[MAX_DISK_NUM][MAX_DISK_SIZE];
int timestamp;

int savedata[MAX_TAG_NUM][MAX_TOTAL_TIME] = {};
int deldata[MAX_TAG_NUM][MAX_TOTAL_TIME] = {};
int readdata[MAX_TAG_NUM][MAX_TOTAL_TIME] = {};

vector <int> del_object[MAX_TOTAL_TIME];
vector <pair <int, pair <int, int> > > write_object[MAX_TOTAL_TIME];
vector <pair <int, int> > read[MAX_TOTAL_TIME];

vector <pair <int, int> > block_order;



int block_size;
Block disk_block[MAX_DISK_NUM][MAX_BLOCK_NUM];
vector <vector <pair <int, int> > > same_block;
int save_count[MAX_TAG_NUM], read_count[MAX_TAG_NUM];
int req_count[MAX_DISK_NUM][MAX_DISK_SIZE];



deque <int> request_queue;
bool cross_block[MAX_DISK_NUM][POINTER_NUM];
unsigned obj_vis[MAX_OBJECT_NUM][MAX_OBJECT_SIZE];
unsigned vcnt = 0;
int last_vis[MAX_DISK_NUM][MAX_BLOCK_NUM];


bool overload = false;
bool valid_block[MAX_DISK_NUM][MAX_BLOCK_NUM] = {};

vector <int> output_delete[MAX_TOTAL_TIME];
vector <pair <int, vector <vector <int> > > > output_write[MAX_TOTAL_TIME];
string output_pointer[MAX_TOTAL_TIME][MAX_DISK_NUM][POINTER_NUM];
vector <int> output_answer[MAX_TOTAL_TIME];
vector <int> output_busy[MAX_TOTAL_TIME];
vector <pair <int, int> > output_gc[MAX_TOTAL_TIME][MAX_DISK_NUM];

Solution solution[THREAD_NUM];

int lst[MAX_DISK_NUM][POINTER_NUM];

void timestamp_action()
{
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

void do_object_delete(vector <int> &object_unit, int disk_id, int size, int tag)
{
    disk_size[disk_id] += size;
    for (int i = 1; i <= size; i++) {
        disk[disk_id][object_unit[i]] = {0, 0};
        disk_reqs[disk_id][object_unit[i]].clear();
        disk_reqs_activate[disk_id][object_unit[i]] = 0;
        int block_id = (object_unit[i] - 1) / block_size + 1;
        disk_block[disk_id][block_id].count--;
        disk_block[disk_id][block_id].tag_count[tag]--;
        // if(disk_block[disk_id][block_id].tag_count[tag] == 0 && disk_block[disk_id][block_id].init_tag != tag){
        //     disk_block[disk_id][block_id].tag ^= 1 << tag;
        // }
        // if(disk_block[disk_id][block_id].count == 0){
        //     disk_block[disk_id][block_id].tag = -1;
        // }
    }
}

void do_request_delete(int request_id)
{
    int object_id = request[request_id].object_id;
    for(int i = 1; i <= REP_NUM; i++){
        for(int j = 1; j <= object[object_id].size; j++){
            if(!request[request_id].vis[j]){
                disk_reqs_activate[object[object_id].replica[i]][object[object_id].unit[i][j]] -= (object[object_id].size + 1) * 120 / object[object_id].size;
            }
        }
    }
}

void delete_action()
{
    int n_delete;
    static int _id[MAX_OBJECT_NUM];

    n_delete = del_object[timestamp].size();
    for (int i = 1; i <= n_delete; i++) {
        _id[i] = del_object[timestamp][i - 1];
    }

    vector <int> output;

    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        save_count[object[id].tag] -= object[id].size;
        for(auto j : object[id].reqs){
            if (request[j].is_done == false) {
                output.push_back(j);
                request[j].is_done = true;
                if(request[j].is_activate) do_request_delete(j);
            }
            if(timestamp - request[j].timestamp <= 105){
                for(int x = 1; x <= REP_NUM; x++){
                    for(int k = 1; k <= object[id].size; k++){
                        req_count[object[id].replica[x]][object[id].unit[x][k]]--;
                    }
                }
            }
        }
        for (int j = 1; j <= REP_NUM; j++) {
            do_object_delete(object[id].unit[j], object[id].replica[j], object[id].size, object[id].tag);
        }
        object[id].is_delete = true;
    }

    for(auto j : output){
        output_delete[timestamp].push_back(j);
    }
}

bool do_object_write_block(vector <int> &object_unit, int disk_id, int block_id, int size, int object_id)
{
    int current_write_point = 0;
    
    int i = disk_block[disk_id][block_id].L;
    bool ok = false;
    for (; i + size - 1 <= disk_block[disk_id][block_id].R; i++) {
        bool flag = true;
        for(int j = 0; j < size; j++){
            if(disk[disk_id][i + j] != make_pair(0, 0)){
                flag = false;
                break;
            }
        }
        if(flag){
            ok = true;
            break;
        }
    }

    if(!ok){
        i = disk_block[disk_id][block_id].L;
    }

    disk_size[disk_id] -= size;
    disk_block[disk_id][block_id].count += size;
    disk_block[disk_id][block_id].tag_count[object[object_id].tag] += size;

    for (; i <= disk_block[disk_id][block_id].R; i++) {
        if (disk[disk_id][i] == make_pair(0, 0)) {
            object_unit[++current_write_point] = i;
            disk[disk_id][i] = {object_id, current_write_point};
            if (current_write_point == size) {
                break;
            }
        }
    }

    return current_write_point == size;
}

void write_action()
{
    int n_write;
    n_write = write_object[timestamp].size();
    for (int i = 1; i <= n_write; i++) {
        int id = write_object[timestamp][i - 1].first;
        auto [size, tag] = write_object[timestamp][i - 1].second;
        object[id].size = size;
        if(tag == 0) tag = M;
        object[id].tag = tag;
        object[id].is_delete = false;
        object[id].reqs.clear();
        save_count[object[id].tag] += object[id].size;

        vector <int> vec;
        for(int j = 1; j <= N; j++){
            if(disk_size[tag_disk[tag][j]] >= size) vec.push_back(tag_disk[tag][j]);
        }

        // use block
        bool vis[MAX_DISK_NUM] = {};
        int sd, sb;
        for (int j = 1; j <= 1; j++) {
            object[id].unit[j].resize(size + 1);
            bool flag = false;
            // try initial tag
            for(auto [disk_id, k] : tag_block[tag]){
                if(vis[disk_id]) continue;
                if(disk_block[disk_id][k].R - disk_block[disk_id][k].L + 1 >= size + disk_block[disk_id][k].count){
                    object[id].replica[j] = disk_id;
                    bool ret = do_object_write_block(object[id].unit[j], object[id].replica[j], k, size, id);
                    if(!ret) continue;
                    sd = disk_id, sb = k;
                    vis[disk_id] = true; flag = true;
                    break;
                }
                if(flag) break;
            }
            if(flag) continue;
            
            // add tag to new block
            pair <int, int> mx = {-100, 0};
            int d, b;
            for(auto [disk_id, k] : block_order){
                if(vis[disk_id]) continue;
                if(disk_block[disk_id][k].tag == -1) continue;
                if(disk_block[disk_id][k].R - disk_block[disk_id][k].L + 1 - disk_block[disk_id][k].count < size) continue;
                int bcnt = -__builtin_popcount(disk_block[disk_id][k].tag);
                if(-bcnt >= MAX_BLOCK_DIFF) continue;

                if(make_pair(bcnt, disk_block[disk_id][k].R - disk_block[disk_id][k].L + 1 - disk_block[disk_id][k].count) >= mx){
                    mx = {bcnt, disk_block[disk_id][k].R - disk_block[disk_id][k].L + 1 - disk_block[disk_id][k].count};
                    d = disk_id;
                    b = k;
                }
            }
            disk_block[d][b].tag |= 1 << tag;
            tag_block[tag].push_back({d, b});
            for(auto [disk_id, k] : tag_block[tag]){
                if(vis[disk_id]) continue;
                if(disk_block[disk_id][k].tag == -1) continue;
                if(!((disk_block[disk_id][k].tag >> tag) & 1)) continue;
                if(disk_block[disk_id][k].R - disk_block[disk_id][k].L + 1 >= size + disk_block[disk_id][k].count){
                    object[id].replica[j] = disk_id;
                    bool ret = do_object_write_block(object[id].unit[j], object[id].replica[j], k, size, id);
                    if(!ret) continue;
                    sd = disk_id, sb = k;
                    vis[disk_id] = true; flag = true;
                    break;
                }
                if(flag) break;
            }
        }
        int j = 2;
        auto &R = same_block[disk_block[sd][sb].block_num];
        for (auto [disk_id, k] : R) {
            if(disk_id == sd) continue;
            object[id].unit[j].resize(size + 1);
            disk_block[disk_id][k].tag |= 1 << tag;
            object[id].replica[j] = disk_id;
            auto ret = do_object_write_block(object[id].unit[j], object[id].replica[j], k, size, id);
            j++;
        }

        output_write[timestamp].push_back({id, {}});
        for (int j = 1; j <= REP_NUM; j++) {
            output_write[timestamp].back().second.push_back({});
            output_write[timestamp].back().second.back().push_back(object[id].replica[j]);
            for (int k = 1; k <= size; k++) {
                output_write[timestamp].back().second.back().push_back(object[id].unit[j][k]);
            }
        }
    }
}

vector <bool> getoneclip(int disk_id, int st, int lst){
    int s = 0;
    for(int j = 0; j < MAX_PR; j++){
        int p = st + j;
        if(p > V) p -= V;
        if(disk_reqs_activate[disk_id][p]){
            s |= 1 << j;
        }
    }
    int token = G;
    vector <bool> ret(B, 0);
    for(int i = 0; i < G; i++){
        int p = st + i;
        if(p > V) p -= V;
        if(p2r[s][p2rmp[lst]]){
            if(token - lst < 0) break;
            token -= lst;
            lst = max(16, (int)ceil(lst * 0.8));
        }else{
            if(token == 0) break;
            lst = 64;
            token--;
        }
        if(p % block_size == 1) ret[(p - 1) / block_size] = true;
        p = p + MAX_PR;
        if(p > V) p -= V;
        if(disk_reqs_activate[disk_id][p]){
            s |= 1 << MAX_PR;
        }
        s >>= 1;
    }
    return ret;
}

int gettime(int disk_id, int st, int lst){
    int s = 0;
    for(int j = 0; j < MAX_PR; j++){
        int p = st + j;
        if(p > V) p -= V;
        if(req_count[disk_id][p]){
            s |= 1 << j;
        }
    }
    int token = 0;
    for(int i = 0; i < block_size; i++){
        int p = st + i;
        if(p > V) p -= V;
        if(p2r[s][p2rmp[lst]]){
            if((token + lst - 1) / G != (token - 1) / G){
                token = ((token - 1) / G + 1) * G;
            }
            token += lst;
            lst = max(16, (int)ceil(lst * 0.8));
        }else{
            lst = 64;
            token++;
        }
        p = p + MAX_PR;
        if(p > V) p -= V;
        if(req_count[disk_id][p] && i + MAX_PR < block_size){
            s |= 1 << MAX_PR;
        }
        s >>= 1;
    }
    return token;
}

void read_action()
{
    vector <int> output, busy;
    int n_read;
    int request_id, object_id;
    n_read = read[timestamp].size();
    vector <pair <int, int> > reads;
    for (int i = 1; i <= n_read; i++) {
        request_id = read[timestamp][i - 1].first, object_id = read[timestamp][i - 1].second;
        reads.push_back({request_id, object_id});
        read_count[object[object_id].tag] += object[object_id].size;
        for(int j = 1; j <= REP_NUM; j++){
            for(int k = 1; k <= object[object_id].size; k++){
                req_count[object[object_id].replica[j]][object[object_id].unit[j][k]]++;
            }
        }
    }

    if(timestamp % DISCARD_TIME == 1){
        int max_save = N * 105 * G * POINTER_NUM * REP_NUM * DISCARD_RATE;
        vector <pair <pair <double, int>, pair <int, int> > > blocks;
        double csum = 0;
        int count[MAX_DISK_NUM][MAX_BLOCK_NUM] = {};
        for(int t = max(1, timestamp - 300); t <= timestamp; t++){
            for(auto [_, id] : read[t]){
                if(object[id].is_delete) continue;
                for(int i = 1; i <= REP_NUM; i++){
                    count[object[id].replica[i]][(object[id].unit[i][1] - 1) / block_size + 1] += object[id].size + 1;
                }
            }
        }
        for(int i = 1; i <= N; i++){
            for(int k = 1; k <= B; k++){
                int num = disk_block[i][k].block_num;
                if(make_pair(i, k) != same_block[num].front()) continue;
                double x = 0;
                x = count[i][k];
                int token = 0;
                for(auto [a, b] : same_block[num]){
                    token += gettime(a, disk_block[a][b].L, 64);
                }
                blocks.push_back({{x, token}, {i, k}});
            }
        }
        sort(blocks.begin(), blocks.end(), [&](auto x, auto y){
            return x > y;
        });
        
        for(int i = 1; i <= N; i++){
            for(int j = 1; j <= B; j++){
                valid_block[i][j] = false;
            }
        }
        struct item {
            double x;
            double init_x;
            double token;
            int time;
        };
        auto cmp = [&](const item &x, const item &y){
            return x.x < y.x;
        };
        priority_queue <item, vector<item>, decltype(cmp)> q(cmp);
        for(int j = 0; j < blocks.size(); j += 1){
            if(blocks[j].first.first){
                while(q.size() && q.top().x > blocks[j].first.first){
                    auto A = q.top(); q.pop();
                    csum += A.token * DISCARD_DECREASE;
                    A.time *= 2;
                    A.x = A.init_x / (A.time * 2);
                    q.push(A);
                }
            }
            bool flag = false;
            for(int x = j; x <= j + 0; x++){
                auto [i, k] = blocks[x].second;
                if(csum >= max_save) flag = true;
            }
            if(!flag){
                for(int x = j; x <= j + 0; x++){
                    auto [i, k] = blocks[x].second;
                    csum += blocks[x].first.second;
                    q.push((item){blocks[x].first.first / 2, blocks[x].first.first, (double)blocks[x].first.second, 1});
                }
            }else{
                int num = disk_block[blocks[j].second.first][blocks[j].second.second].block_num;
                for(auto [i, k] : same_block[num]){
                    valid_block[i][k] = true;
                    last_vis[i][k] = timestamp + DISCARD_TIME;
                }
                bool flag = false;
                for(auto [i, k] : same_block[num]){
                    if(disk_block[i][k].L <= disk_pointer[i][0] && disk_pointer[i][0] <= disk_block[i][k].R) flag = true;
                    if(disk_block[i][k].L <= disk_pointer[i][1] && disk_pointer[i][1] <= disk_block[i][k].R) flag = true;
                }
                if(!flag){
                    auto [i, k] = blocks[j].second;
                    for(int j = disk_block[i][k].L; j <= disk_block[i][k].R; j++){
                        if(disk_reqs_activate[i][j]){
                            auto [object_id, unit_id] = disk[i][j];
                            for(auto request_id : disk_reqs[i][j]){
                                if(!request[request_id].is_activate) continue;
                                if(request[request_id].vis[unit_id]) continue;
                                request[request_id].vis[unit_id] = true;
                                if(--request[request_id].cnt == 0){
                                    request[request_id].is_done = true;
                                    busy.push_back(request_id);
                                }
                            }
                            for(int x = 1; x <= REP_NUM; x++){
                                disk_reqs[object[object_id].replica[x]][object[object_id].unit[x][unit_id]].clear();
                                disk_reqs_activate[object[object_id].replica[x]][object[object_id].unit[x][unit_id]] = 0;
                            }
                        }
                    }
                }
            }
        }
    }
    if constexpr (discard_stage != -1){
        if(discard_stage == 0){
            for(auto [request_id, object_id] : reads){
                request[request_id].object_id = object_id;
                request[request_id].timestamp = timestamp;
                request_queue.push_back(request_id);
                object[object_id].reqs.push_back(request_id);
                busy.push_back(request_id);
                request[request_id].is_done = true;
                request[request_id].is_activate = false;
            }
            swap(output, output_answer[timestamp]);
            swap(busy, output_busy[timestamp]);
            return;
        }
    }

    for(auto [request_id, object_id] : reads){
        request[request_id].object_id = object_id;
        request[request_id].timestamp = timestamp;
        request_queue.push_back(request_id);
        object[object_id].reqs.push_back(request_id);
        if(valid_block[object[object_id].replica[1]][(object[object_id].unit[1][1] - 1) / block_size + 1]){
            busy.push_back(request_id);
            request[request_id].is_done = true;
            request[request_id].is_activate = false;
            continue;
        }
        request[request_id].is_done = false;
        request[request_id].is_activate = true;
        request[request_id].cnt = object[object_id].size;
        for(int j = 1; j <= object[object_id].size; j++){
            request[request_id].vis[j] = false;
        }

        for(int j = 1; j <= REP_NUM; j++){
            for(int k = 1; k <= object[object_id].size; k++){
                disk_reqs[object[object_id].replica[j]][object[object_id].unit[j][k]].push_back(request_id);
                disk_reqs_activate[object[object_id].replica[j]][object[object_id].unit[j][k]] += (object[object_id].size + 1) * 120 / object[object_id].size;
            }
        }
    }

    string answer[N + 1][POINTER_NUM];

    vector <pair <int, pair <int, int> > > vec;

    for(int disk_id = 1; disk_id <= N; disk_id++){
        for(int pid = 0; pid < POINTER_NUM; pid++){
            int j = 0;
            for(; j < V; j++){
                int p = disk_pointer[disk_id][pid] + j;
                if(p > V) p -= V;
                if(disk_reqs_activate[disk_id][p]) break;
            }
            vec.push_back({j, {disk_id, pid}});
        }
    }
    sort(vec.begin(), vec.end());

    for(auto [_, __] : vec){
        auto [disk_id, pid] = __;
        double now = 0;
        bool is_end = false;
        for(int j = 0; j < block_size; j++){
            int p = disk_pointer[disk_id][pid] + j;
            if(p > V) p -= V;
            if(p % block_size == 1 && j > 0) break;
            now += disk_reqs_activate[disk_id][p];
        }
        if(now == 0) cross_block[disk_id][pid] = true, is_end = true;
        if(cross_block[disk_id][pid]){
            vcnt++;
            for(int i = 1; i <= N; i++){
                for(int k = 0; k < POINTER_NUM; k++){
                    if(disk_id == i && pid == k) continue;
                    for(int j = 0; j < block_size; j++){
                        int p = disk_pointer[i][k] + j;
                        if(p > V) p -= V;
                        if(p % block_size == 1 && j > 0) break;
                        obj_vis[disk[i][p].first][disk[i][p].second] = vcnt;
                    }
                }
            }
            double mx = -1;
            double mx2 = -1;
            double sum = 0;
            double now = 0;
            int L = block_size;
            int x = 0;
            for(int j = 0; j < L; j++){
                int p = disk_pointer[disk_id][pid] + j;
                if(p > V) p -= V;
                if(obj_vis[disk[disk_id][p].first][disk[disk_id][p].second] != vcnt) now += disk_reqs_activate[disk_id][p];
            }
            vector <bool> oneclipvis = getoneclip(disk_id, disk_pointer[disk_id][pid], lst[disk_id][pid]);
            for(int j = 1; j < L; j++){
                if(obj_vis[disk[disk_id][j].first][disk[disk_id][j].second] != vcnt) sum += disk_reqs_activate[disk_id][j];
            }
            for(int j = 1; j <= V; j++){
                int p = j + L - 1;
                if(p > V) p -= V;
                if(obj_vis[disk[disk_id][p].first][disk[disk_id][p].second] != vcnt) sum += disk_reqs_activate[disk_id][p];
                if(j % block_size == 1 && j <= block_size * B){
                    if(mx < sum){
                        mx = sum;
                    }
                    if(mx2 < sum * (max(0, (min(105, timestamp - last_vis[disk_id][(j - 1) / block_size + 1]) - TIME_BIAS)) + 1)){
                        mx2 = sum * (max(0, (min(105, timestamp - last_vis[disk_id][(j - 1) / block_size + 1]) - TIME_BIAS)) + 1);
                        x = j;
                    }
                }
                if(obj_vis[disk[disk_id][j].first][disk[disk_id][j].second] != vcnt) sum -= disk_reqs_activate[disk_id][j];
            }
            
            if((now * JUMP_MUL < mx || is_end) && !oneclipvis[(x - 1) / block_size]){
                lst[disk_id][pid] = 64;
                answer[disk_id][pid] = "";
                disk_pointer[disk_id][pid] = x;
                while(x){
                    answer[disk_id][pid].push_back('0' + x % 10);
                    x /= 10;
                }
                answer[disk_id][pid].push_back(' '); answer[disk_id][pid].push_back('j');
                reverse(answer[disk_id][pid].begin(), answer[disk_id][pid].end());
                cross_block[disk_id][pid] = false;
                continue;
            }
        }
        int p = disk_pointer[disk_id][pid];
        if((p - 1) / block_size + 1 <= B){
            auto &R = same_block[disk_block[disk_id][(p - 1) / block_size + 1].block_num];
            for (auto [disk_id, k] : R) {
                last_vis[disk_id][k] = timestamp;
            }
        }
        cross_block[disk_id][pid] = false;
        for(int i = 0; i <= G + GEXTRA; i++){
            for(int j = 0; j <= C; j++){
                f[i][j] = INF;
            }
        }
        f[0][p2rmp[lst[disk_id][pid]]] = 0;
        int resx = 0, resy = 0;
        for(int i = 0; i <= G + GEXTRA; i++){
            int p = disk_pointer[disk_id][pid] + i;
            p = (p - 1) % V + 1;
            bool flag = false;
            for(int j = 0; j <= C; j++){
                if(f[i][j] > G + GEXTRA) continue;
                int nj = j == C ? j : j + 1;
                if(f[i + 1][nj] > f[i][j] + cost[j]){
                    f[i + 1][nj] = f[i][j] + cost[j];
                    fr[i + 1][nj] = j;
                }
                if(!disk_reqs_activate[disk_id][p]){
                    if(f[i + 1][0] > f[i][j] + 1){
                        f[i + 1][0] = f[i][j] + 1;
                        fr[i + 1][0] = j;
                    }
                }
                if(f[i][j] <= G + GEXTRA){
                    resx = i;
                    resy = j;
                    flag = true;
                }
            }
            if(!flag) break;
        }

        for(int i = resx, j = resy; i > 0; i--){
            if(j == 0) answer[disk_id][pid].push_back('p');
            else answer[disk_id][pid].push_back('r');
            j = fr[i][j];
        }
        reverse(answer[disk_id][pid].begin(), answer[disk_id][pid].end());
        
        int token = G;
        int sz = 0;
        for(auto op : answer[disk_id][pid]){
            if(op == 'r'){
                if(token - lst[disk_id][pid] < 0) break;
                token -= lst[disk_id][pid];
                lst[disk_id][pid] = max(16, (int)ceil(lst[disk_id][pid] * 0.8));
            }else{
                if(token == 0) break;
                lst[disk_id][pid] = 64;
                token--;
            }
            sz++;
            int &p = disk_pointer[disk_id][pid];
            if(disk_reqs_activate[disk_id][p]){
                auto [object_id, unit_id] = disk[disk_id][p];
                for(auto request_id : disk_reqs[disk_id][p]){
                    if(!request[request_id].is_activate) continue;
                    if(request[request_id].vis[unit_id]) continue;
                    request[request_id].vis[unit_id] = true;
                    if(--request[request_id].cnt == 0){
                        request[request_id].is_done = true;
                        output.push_back(request_id);
                    }
                }
                for(int i = 1; i <= REP_NUM; i++){
                    disk_reqs[object[object_id].replica[i]][object[object_id].unit[i][unit_id]].clear();
                    disk_reqs_activate[object[object_id].replica[i]][object[object_id].unit[i][unit_id]] = 0;
                }
            }
            p++;
            if(p > V) p = 1;
            if(p % block_size == 1) cross_block[disk_id][pid] = true;
        }
        answer[disk_id][pid].resize(sz);
    }

    for(int disk_id = 1; disk_id <= N; disk_id++) for(int pid = 0; pid < POINTER_NUM; pid++){
        bool flag = false;
        for(auto c : answer[disk_id][pid]){
            if(c == 'r'){
                flag = true;
                break;
            }
        }
        if(flag || answer[disk_id][pid][0] == 'j') continue;
        vcnt++;
        int x = disk_pointer[disk_id][pid ^ 1];
        lst[disk_id][pid] = 64;
        answer[disk_id][pid] = "";
        disk_pointer[disk_id][pid] = x;
        while(x){
            answer[disk_id][pid].push_back('0' + x % 10);
            x /= 10;
        }
        answer[disk_id][pid].push_back(' '); answer[disk_id][pid].push_back('j');
        reverse(answer[disk_id][pid].begin(), answer[disk_id][pid].end());
        cross_block[disk_id][pid] = false;
    }

    for(int i = 1; i <= N; i++) for(int j = 0; j < POINTER_NUM; j++){
        output_pointer[timestamp][i][j] = answer[i][j];
    }

    overload = false;

    while(request_queue.size() && timestamp - request[request_queue.front()].timestamp >= 105){
        int current_request = request_queue.front();
        int object_id = request[current_request].object_id;
        read_count[object[object_id].tag] -= object[object_id].size;
        if(!object[object_id].is_delete){
            for(int j = 1; j <= REP_NUM; j++){
                for(int k = 1; k <= object[object_id].size; k++){
                    req_count[object[object_id].replica[j]][object[object_id].unit[j][k]]--;
                }
            }
        }
        if(request[current_request].is_done){
            request_queue.pop_front();
        }else if(!request[current_request].is_activate){
            request_queue.pop_front();
        }else if(timestamp - request[current_request].timestamp >= 105){
            request[current_request].is_activate = false;
            request[current_request].is_done = true;
            do_request_delete(current_request);
            busy.push_back(current_request);
            request_queue.pop_front();
            overload = true;
        }else{
            break;
        }
    }

    swap(output, output_answer[timestamp]);
    swap(busy, output_busy[timestamp]);
}

void gc_action()
{
    for(int disk_id = 1; disk_id <= N; disk_id++){
        vector <pair <int, int> > vec;
        vector <pair <int, int> > gc;
        for(int i = 1; i <= B; i++){
            int sum = 0;
            int lst = 1;
            for(int j = disk_block[disk_id][i].L; j <= disk_block[disk_id][i].R; j++){
                int state = disk[disk_id][j] != make_pair(0, 0);
                if(lst != state) sum++;
                lst = state;
            }
            int init_tag = (__builtin_popcount(disk_block[disk_id][i].tag) == 1 && __builtin_ctz(disk_block[disk_id][i].tag) != M) * 2;
            init_tag += (__builtin_popcount(disk_block[disk_id][i].tag) == 2);
            vec.push_back({-sum - (!valid_block[disk_id][i] * 100000) - (init_tag * 1000000), i});
        }
        sort(vec.begin(), vec.end());
        int token = K;
        for(auto [_, i] : vec){
            if(token <= 0) break;
            int l = disk_block[disk_id][i].L, r = disk_block[disk_id][i].R;
            while(l < r && token > 0){
                while(l <= disk_block[disk_id][i].R && disk[disk_id][l] != make_pair(0, 0)) l++;
                while(r >= disk_block[disk_id][i].L && disk[disk_id][r] == make_pair(0, 0)) r--;
                if(l >= r) break;
                token--;
                gc.push_back({l, r});

                auto [object_id, unit_id] = disk[disk_id][l];
                if(object_id){
                    int k = 1;
                    for(; k <= REP_NUM; k++){
                        if(object[object_id].replica[k] == disk_id) break;
                    }
                    object[object_id].unit[k][unit_id] = r;
                }

                object_id = disk[disk_id][r].first, unit_id = disk[disk_id][r].second;
                if(object_id){
                    int k = 1;
                    for(; k <= REP_NUM; k++){
                        if(object[object_id].replica[k] == disk_id) break;
                    }
                    object[object_id].unit[k][unit_id] = l;
                }
                swap(disk[disk_id][l], disk[disk_id][r]);
                swap(disk_reqs_activate[disk_id][l], disk_reqs_activate[disk_id][r]);
                swap(disk_reqs[disk_id][l], disk_reqs[disk_id][r]);
                swap(req_count[disk_id][l], req_count[disk_id][r]);
            }
        }
        output_gc[timestamp][disk_id] = gc;
    }
}

int main()
{
    int K1, K2;
    request.resize(MAX_REQUEST_NUM);
    scanf("%d%d%d%d%d%d%d", &T, &M, &N, &V, &G, &K1, &K2);
    K = K1;
    M++;

    for(int i = 1; i <= N; i++){
        for(int j = 0; j < POINTER_NUM; j++) disk_pointer[i][j] = 1, cross_block[i][j] = true, lst[i][j] = 64;
        disk_size[i] = V;
    }

    block_size = V / B;
    for(int i = 1; i <= N; i++){
        for(int j = 1; j <= B; j++){
            disk_block[i][j].L = (j - 1) * block_size + 1;
            disk_block[i][j].R = min(j * block_size, V);
            disk_block[i][j].tag = -1;
            disk_block[i][j].init_tag = -1;
            disk_block[i][j].count = 0;
            memset(disk_block[i][j].tag_count, 0, sizeof(disk_block[i][j].tag_count));
        }
        disk_block[i][B].tag = -1;
    }

    for(int i = 1; i <= M; i++){
        for(int j = 1; j <= N; j++){
            tag_disk[i][j] = j;
        }
        shuffle(tag_disk[i] + 1, tag_disk[i] + N + 1, engine);
    }

    int block_count[MAX_TAG_NUM] = {};
    block_count[1] = B * N / REP_NUM;

    bool finished = true;
    vector <int> tnum[MAX_TAG_NUM];
    vector <int> order;
    while(finished){
        finished = false;
        vector <int> vec;
        same_block.clear();
        order.clear();
        for(int i = 1; i <= M; i++){
            for(int j = 1; j <= block_count[i]; j++){
                vec.push_back(i);
            }
            tnum[i].clear();
        }
        shuffle(vec.begin(), vec.end(), engine);

        int acnt[MAX_DISK_NUM] = {};
        int cnt[MAX_DISK_NUM] = {};
        for(int i = 1; i <= N; i++){
            cnt[i] = B / REP_NUM;
        }
        for(auto tag : vec){
            vector <pair <int, int> > ids;
            for(int i = 1; i <= N; i++){
                if(acnt[i] < B / REP_NUM) ids.push_back({acnt[i], i});
            }
            shuffle(ids.begin(), ids.end(), engine);
            sort(ids.begin(), ids.end(), [&](auto x, auto y){
                return x.first < y.first;
            });
            int d = ids[0].second;
            disk_block[d][++acnt[d]].tag = (1 << tag);
            disk_block[d][acnt[d]].init_tag = tag;
            disk_block[d][acnt[d]].block_num = same_block.size();
            same_block.push_back({{d, acnt[d]}});
            tnum[tag].push_back(disk_block[d][acnt[d]].block_num);
            order.push_back(tnum[tag].size());

            ids.clear();
            for(int i = 1; i <= N; i++){
                if(cnt[i] < B && i != d) ids.push_back({cnt[i], i});
            }
            if(ids.size() < REP_NUM - 1){
                finished = true;
                break;
            }
            shuffle(ids.begin(), ids.end(), engine);
            sort(ids.begin(), ids.end(), [&](auto x, auto y){
                return x.first < y.first;
            });

            for(int i = 0; i < REP_NUM - 1; i++){
                int j = ids[i].second;
                disk_block[j][++cnt[j]].tag = (1 << tag);
                disk_block[j][cnt[j]].init_tag = tag;
                disk_block[j][cnt[j]].block_num = disk_block[d][acnt[d]].block_num;
                same_block[disk_block[d][acnt[d]].block_num].push_back({j, cnt[j]});
            }
        }
    }

    for(auto &A : same_block){
        A.clear();
    }

    for(int i = 1; i <= N; i++){
        vector <int> index;
        for(int j = 0; j <= M; j++) index.push_back(j);
        shuffle(index.begin(), index.end(), engine);
        sort(disk_block[i] + 1, disk_block[i] + B + 1, [&](auto &x, auto &y){
            if(x.init_tag != y.init_tag) return index[x.init_tag] < index[y.init_tag];
            return order[x.block_num] < order[y.block_num];
        });
        for(int j = 1; j <= B; j++){
            disk_block[i][j].L = (j - 1) * block_size + 1;
            disk_block[i][j].R = min(j * block_size, V);
            disk_block[i][j].init_tag = 0;
            disk_block[i][j].tag = 0;
            same_block[disk_block[i][j].block_num].push_back({i, j});
        }
    }

    for(int i = 1; i <= N; i++){
        for(int j = 1; j <= B; j++){
            block_order.push_back({i, j});
        }
    }
    shuffle(block_order.begin(), block_order.end(), engine);

    printf("OK\n");
    fflush(stdout);

#ifdef DHXH
    long long disk_cnt = 0;
#endif

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        {
            int n;
            scanf("%d", &n);
            for (int j = 1; j <= n; j++) {
                int x;
                scanf("%d", &x);
                del_object[t].push_back(x);
            }
            delete_action();
            printf("%d\n", output_delete[t].size());
            for (auto j : output_delete[t]) {
                printf("%d\n", j);
            }
            fflush(stdout);
        }
        {
            int n;
            scanf("%d", &n);
            for (int j = 1; j <= n; j++) {
                int id, size, tag;
                scanf("%d%d%d", &id, &size, &tag);
                write_object[t].push_back({id, {size, tag}});
            }
            write_action();
            for(auto &A : output_write[t]){
                printf("%d\n", A.first);
                for (auto &vec : A.second) {
                    printf("%d", vec[0]);
                    for (int k = 1; k < vec.size(); k++) {
                        printf(" %d", vec[k]);
                    }
                    printf("\n");
                }
            }
            fflush(stdout);
        }
        {
            int n;
            scanf("%d", &n);
            for (int j = 1; j <= n; j++) {
                int request_id, object_id;
                scanf("%d%d", &request_id, &object_id);
                read[t].push_back({request_id, object_id});
            }
            read_action();
            for(int i = 1; i <= N; i++) for(int j = 0; j < POINTER_NUM; j++){
                if(output_pointer[t][i][j].size() == 0 || output_pointer[t][i][j].back() == 'r' || output_pointer[t][i][j].back() == 'p'){
                    printf("%s#\n", output_pointer[t][i][j].c_str());
                }else{
                    printf("%s\n", output_pointer[t][i][j].c_str());
                }
            }
            printf("%d\n", output_answer[t].size());
            for(auto i : output_answer[t]){
                printf("%d\n", i);
            }

            printf("%d\n", output_busy[t].size());
            for(auto i : output_busy[t]){
                printf("%d\n", i);
            }

            fflush(stdout);
        }
        if (t % FRE_PER_SLICING == 0) {
            scanf("%*s %*s");
            gc_action();
            printf("GARBAGE COLLECTION\n");
            for(int i = 1; i <= N; i++){
                printf("%d\n", output_gc[t][i].size());
                for(auto [x, y] : output_gc[t][i]){
                    printf("%d %d\n", x, y);
                }
            }
            fflush(stdout);
        }
#ifdef DHXH
        if(30000 < t && t <= 60000){
            for(int i = 1; i <= N; i++){
                for(int j = 1; j <= V; j++){
                    if(disk[i][j] != make_pair(0, 0)){
                        disk_cnt++;
                    }
                }
            }
        }
#endif
    }

#ifdef DHXH
    cerr << 1.0 * disk_cnt / N / V / 30000 << endl;
#endif

    int n;
    scanf("%d", &n);
    for(int i = 1; i <= n; i++){
        int id, tag;
        scanf("%d%d", &id, &tag);
        object[id].tag = tag;
    }

    for(int t = 1; t <= T + EXTRA_TIME; t++){
        for(auto id : del_object[t]){
            deldata[object[id].tag][t] += object[id].size;
        }
        for(auto &A : write_object[t]){
            int id = A.first;
            int &tag = A.second.second;
            tag = object[id].tag;
            savedata[object[id].tag][t] += object[id].size;
        }
        for(auto [_, id] : read[t]){
            readdata[object[id].tag][t] += object[id].size;
        }
    }

    double req[MAX_TAG_NUM][MAX_TAG_SLICING] = {};
    double req_time[MAX_TAG_NUM][MAX_TAG_SLICING] = {};
    double total_save[MAX_TAG_NUM][MAX_TAG_SLICING] = {};

    int save[MAX_TAG_NUM] = {};
    for(int t = 1; t <= T + EXTRA_TIME; t++){
        int p = (t - 1) / TAG_SLICING + 1;
        for(int i = 1; i <= M; i++){
            save[i] -= deldata[i][t];
            save[i] += savedata[i][t];
            req[i][p] += readdata[i][t];
            total_save[i][p] += save[i];
        }
    }
    M--;

    bitset <MAX_TAG_SLICING> bit[MAX_TAG_NUM];
    double avg[MAX_TAG_SLICING] = {};
    for(int t = 1; t <= T / TAG_SLICING; t++){
        double sum = 0;
        for(int i = 1; i <= M; i++){
            req[i][t] /= total_save[i][t];
            sum += req[i][t];
        }
        sum /= M;
        sum *= TAG_PREDICT_RATE;
        avg[t] = sum;
        for(int i = 1; i <= M; i++){
            bit[i][t] = req[i][t] >= sum;
        }
    }

    vector <int> start_time(MAX_OBJECT_NUM, 1);
    vector <int> end_time(MAX_OBJECT_NUM, T);
    vector <int> object_id;

    for(int t = 1; t <= T + EXTRA_TIME; t++){
        for(auto id : del_object[t]){
            end_time[id] = t;
        }
        for(auto &A : write_object[t]){
            int id = A.first;
            int &tag = A.second.second;
            if(tag == M + 1) object_id.push_back(id);
            start_time[id] = t;
        }
    }
    
    // todo: use bit to predict tag.

    /*for(auto id : object_id){
        int now[MAX_TAG_SLICING];
        memset(now, -1, sizeof(now));
        double sum[MAX_TAG_SLICING] = {};
        for(auto request_id : object[id].reqs){
            int t = request[request_id].timestamp;
            sum[(t - 1) / TAG_SLICING + 1]++;
        }
        for(int t = 1; t <= T / TAG_SLICING; t++){
            int l = max(t * TAG_SLICING - TAG_SLICING + 1, start_time[id]);
            int r = min(t * TAG_SLICING, end_time[id]);
            if(l <= r){
                sum[t] /= (r - l) + 1;
                now[t] = sum[t] >= avg[t];
            }
        }
        int mx = 0, tag = 1;
        for(int i = 1; i <= M; i++){
            int cnt = 0;
            for(int t = 1; t <= T / TAG_SLICING; t++){
                if(now[t] != -1 && now[t] == bit[i][t]){
                    cnt++;
                }
            }
            if(cnt > mx){
                mx = cnt;
                tag = i;
            }
        }
        object[id].tag = tag;
    }*/
    
    // use vector to predict tag.

    for(auto id : object_id){
        int now[MAX_TAG_SLICING];
        memset(now, -1, sizeof(now));
        double sum[MAX_TAG_SLICING] = {};
        for(auto request_id : object[id].reqs){
            int t = request[request_id].timestamp;
            sum[(t - 1) / TAG_SLICING + 1]++;
        }
        for(int t = 1; t <= T / TAG_SLICING; t++){
            int l = max(t * TAG_SLICING - TAG_SLICING + 1, start_time[id]);
            int r = min(t * TAG_SLICING, end_time[id]);
            if(l <= r){
                sum[t] /= (r - l) + 1;
                now[t] = sum[t] >= avg[t];
            }
        }
        int tag = 1;
        double mx = INF;
        for(int i = 1; i <= M; i++){
            double a = 0, b = 0, c = 0;
            for(int t = 1; t <= T / TAG_SLICING; t++){
                if(now[t] != -1){
                    a += (sum[t] - req[i][t]) * (sum[t] - req[i][t]);
                }
            }
            double cnt = a;
            if(cnt < mx){
                mx = cnt;
                tag = i;
            }
        }
        object[id].tag = tag;
    }

    vector <pair <int, int> > obj_tags;
    vector <int> init_tag(MAX_OBJECT_NUM);

    memset(deldata, 0, sizeof(deldata));
    memset(savedata, 0, sizeof(savedata));
    memset(readdata, 0, sizeof(readdata));
    for(int t = 1; t <= T + EXTRA_TIME; t++){
        for(auto id : del_object[t]){
            deldata[object[id].tag][t] += object[id].size;
        }
        for(auto &A : write_object[t]){
            int id = A.first;
            int &tag = A.second.second;
            tag = object[id].tag;
            obj_tags.push_back({id, tag});
            init_tag[id] = tag;
            savedata[object[id].tag][t] += object[id].size;
        }
        for(auto [_, id] : read[t]){
            readdata[object[id].tag][t] += object[id].size;
        }
    }
    sort(obj_tags.begin(), obj_tags.end());

#ifdef DHXH
    cerr << obj_tags.size() << endl;
#endif

    double max_score = -1;
    for(int pid = 0; pid < THREAD_NUM; pid++){
        auto s = engine();
        solution[pid].engine.seed(s);
    }

    thread th[THREAD_NUM];
    mutex lock;

    swap(request, solution[0].request);

    for(int tid = 0; tid < THREAD_NUM; tid++){
        th[tid] = std::thread([&](int pid){
            solution[pid].pid = pid;
            solution[pid].T = T;
            solution[pid].N = N;
            solution[pid].M = M + 1;
            solution[pid].V = V;
            solution[pid].G = G;
            solution[pid].K = K2;
            solution[pid].obj_tags = obj_tags;
            solution[pid].init_tag = init_tag;
            solution[pid].del_object = del_object;
            solution[pid].read = read;
            for(int t = 1; t <= T + EXTRA_TIME; t++){
                solution[pid].write_object[t] = write_object[t];
            }
            for(int _ = 1; _ <= REP_TIME; _++){
                solution[pid].init();
                for (int t = 1; t <= T + EXTRA_TIME; t++) {
                    solution[pid].timestamp = t;
                    solution[pid].G = G;
                    solution[pid].delete_action();
                    solution[pid].write_action();
                    solution[pid].read_action();
                    if (t % FRE_PER_SLICING == 0) {
                        solution[pid].gc_action();
#ifdef DHXH
                cerr << pid << " " << t << " " << setprecision(12) << solution[pid].score << endl;
#endif
                    }
                    if(runtime() >= END_TIME){
                        solution[pid].score = -1;
                        break;
                    }
                }
                if(runtime() >= END_TIME) break;
#ifdef DHXH
                cerr << pid << " " << _ << " " << setprecision(12) << solution[pid].score << endl;
#endif
                lock.lock();
                if(max_score < solution[pid].score){
                    max_score = solution[pid].score;
                    for (int t = 1; t <= T + EXTRA_TIME; t++) {
                        swap(solution[pid].output_delete[t], output_delete[t]);
                        swap(solution[pid].output_write[t], output_write[t]);
                        swap(solution[pid].output_answer[t], output_answer[t]);
                        swap(solution[pid].output_busy[t], output_busy[t]);
                        for(int i = 1; i <= N; i++) for(int j = 0; j < POINTER_NUM; j++){
                            swap(solution[pid].output_pointer[t][i][j], output_pointer[t][i][j]);
                        }
                        if (t % FRE_PER_SLICING == 0) {
                            for(int i = 1; i <= N; i++){
                                swap(solution[pid].output_gc[t][i], output_gc[t][i]);
                            }
                        }
                    }
                }
                lock.unlock();
                if(runtime() >= END_TIME) break;
                solution[pid].calc_tag();
            }
        }, tid);
    }
    for(int i = 0; i < THREAD_NUM; i++){
        th[i].join();
    }
    
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        vector <int> vec;
        swap(vec, output_busy[t]);
        for(auto x : vec){
            output_busy[solution[0].request[x].timestamp].push_back(x);
        }
    }

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        {
            printf("%d\n", output_delete[t].size());
            for (auto j : output_delete[t]) {
                printf("%d\n", j);
            }
            fflush(stdout);
        }
        {
            for(auto &A : output_write[t]){
                printf("%d\n", A.first);
                for (auto &vec : A.second) {
                    printf("%d", vec[0]);
                    for (int k = 1; k < vec.size(); k++) {
                        printf(" %d", vec[k]);
                    }
                    printf("\n");
                }
            }
            fflush(stdout);
        }
        {
            for(int i = 1; i <= N; i++) for(int j = 0; j < POINTER_NUM; j++){
                if(output_pointer[t][i][j].size() == 0 || output_pointer[t][i][j].back() == 'r' || output_pointer[t][i][j].back() == 'p'){
                    printf("%s#\n", output_pointer[t][i][j].c_str());
                }else{
                    printf("%s\n", output_pointer[t][i][j].c_str());
                }
            }
            printf("%d\n", output_answer[t].size());
            for(auto i : output_answer[t]){
                printf("%d\n", i);
            }

            printf("%d\n", output_busy[t].size());
            for(auto i : output_busy[t]){
                printf("%d\n", i);
            }
            fflush(stdout);
        }
        if (t % FRE_PER_SLICING == 0) {
            printf("GARBAGE COLLECTION\n");
            for(int i = 1; i <= N; i++){
                printf("%d\n", output_gc[t][i].size());
                for(auto [x, y] : output_gc[t][i]){
                    printf("%d %d\n", x, y);
                }
            }
            fflush(stdout);
        }
    }

    return 0;
}