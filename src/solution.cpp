#include "solution.h"

using namespace std;

const int B = 24;
#define JUMP_MUL (1.3)
#define JUMP_MUL_FAC (0.8)
#define DISCARD_DECREASE (0.5)
#define DISCARD_RATE (0.95)
#define DISCARD_TIME (200)
#define TIME_BIAS (105)
#define MAX_BLOCK_NUM (B + 5)

double fscore(int x){
    if(x <= 10){
        return -0.005 * x + 1;
    }else if(x <= 105){
        return -0.01 * x + 1.05;
    }
    return 0;
}
double gscore(int x){
    return (x + 1) * 0.5;
}
double hscore(int x){
    return 1.0 * x / 105;
}

double runtime(){
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time);
    return duration.count() / 1e6;
}

const auto start_time2 = std::chrono::steady_clock::now();
double runtime2() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time2);
    return duration.count() / 1e6;
}

Solution::Solution(){
    request.resize(MAX_REQUEST_NUM);
    memset(savedata, 0, sizeof(savedata));
    memset(deldata, 0, sizeof(deldata));
    memset(readdata, 0, sizeof(readdata));
    memset(max_suf, 0, sizeof(max_suf));
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
}

void Solution::do_object_delete(vector <int> &object_unit, int disk_id, int size, int tag)
{
    disk_size[disk_id] += size;
    for (int i = 1; i <= size; i++) {
        disk[disk_id][object_unit[i]] = {0, 0};
        {vector <int> tmp; swap(tmp, disk_reqs[disk_id][object_unit[i]]);};
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

void Solution::do_request_delete(int request_id)
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

void Solution::delete_action()
{
    int n_delete;

    n_delete = del_object[timestamp].size();
    for (int i = 1; i <= n_delete; i++) {
        _id[i] = del_object[timestamp][i - 1];
    }

    vector <int> output;

    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        save_count[object[id].tag] -= object[id].size;
        deldata[object[id].tag][timestamp] -= object[id].size;
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

bool Solution::do_object_write_block(vector <int> &object_unit, int disk_id, int block_id, int size, int object_id)
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
        // return false;
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

void Solution::write_action()
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
        {vector <int> tmp; swap(tmp, object[id].reqs);}
        save_count[object[id].tag] += object[id].size;
        savedata[object[id].tag][timestamp] -= object[id].size;
        for(int j = 1; j <= M; j++){
            int mx = 0;
            mx = max(savedata[j][timestamp], max_suf[j][timestamp + 1] - deldata[j][timestamp + 1] + savedata[j][timestamp]);
            int c = 0;
            for(auto [disk_id, k] : tag_block[j]){
                c++;
                if((disk_block[disk_id][k].finished >> j) & 1) break;
                if(block_size - disk_block[disk_id][k].count >= mx){
                    disk_block[disk_id][k].finished |= (1 << j);
                }
                mx -= block_size - disk_block[disk_id][k].count;
            }
        }

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

            if(tag == M){
                tag = init_tag[id];
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
            }
            
            // add tag to new block
            pair <int, int> mx = {-100, 0};
            int d, b;
            for(auto disk_id : vec){
                if(vis[disk_id]) continue;
                for(int k = 1; k <= B; k++){
                    if(disk_block[disk_id][k].init_tag == M) continue;
                    if(disk_block[disk_id][k].tag == -1) continue;
                    if(disk_block[disk_id][k].finished != disk_block[disk_id][k].tag) continue;
                    if(disk_block[disk_id][k].R - disk_block[disk_id][k].L + 1 - disk_block[disk_id][k].count < size) continue;
                    int bcnt = -__builtin_popcount(disk_block[disk_id][k].tag);
                    if(-bcnt >= MAX_BLOCK_DIFF) continue;

                    if(make_pair(bcnt, disk_block[disk_id][k].R - disk_block[disk_id][k].L + 1 - disk_block[disk_id][k].count) >= mx){
                        mx = {bcnt, disk_block[disk_id][k].R - disk_block[disk_id][k].L + 1 - disk_block[disk_id][k].count};
                        d = disk_id;
                        b = k;
                    }
                }
            }
            if(mx.first == -100){
                for(auto disk_id : vec){
                    if(vis[disk_id]) continue;
                    for(int k = 1; k <= B; k++){
                        if(disk_block[disk_id][k].init_tag == M) continue;
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

vector <bool> Solution::getoneclip(int disk_id, int st, int lst){
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

int Solution::gettime(int disk_id, int st, int lst){
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

void Solution::read_action()
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
        int count[MAX_TAG_NUM] = {};
        for(int t = max(1, timestamp - 200); t <= min(timestamp + 200, MAX_TIME + EXTRA_TIME); t++){
            for(auto [_, id] : read[t]){
                count[object[id].tag] += object[id].size + 1;
            }
        }
        for(int i = 1; i <= N; i++){
            for(int k = 1; k <= B; k++){
                int num = disk_block[i][k].block_num;
                if(disk_block[i][k].init_tag == M) continue;
                if(make_pair(i, k) != same_block[num].front()) continue;
                double x = 0;
                for(int j = disk_block[i][k].L; j <= disk_block[i][k].R; j++){
                    if(disk[i][j].first){
                        x += 1.0 * count[object[disk[i][j].first].tag] / save_count[object[disk[i][j].first].tag];
                    }
                }
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
                valid_block[i][j] = disk_block[i][j].init_tag == M;
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
                                {vector <int> tmp; swap(tmp, disk_reqs[object[object_id].replica[x]][object[object_id].unit[x][unit_id]]);};
                                disk_reqs_activate[object[object_id].replica[x]][object[object_id].unit[x][unit_id]] = 0;
                            }
                        }
                    }
                }
            }
        }
    }
    if constexpr (discard_stage != -1){
        if(discard_stage == 1){
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
            for(; j < G; j++){
                int p = disk_pointer[disk_id][pid] + j;
                if(p > V) p -= V;
                if(disk_reqs_activate[disk_id][p]) break;
            }
            vec.push_back({j, {disk_id, pid}});
        }
    }
    sort(vec.begin(), vec.end());
    
    for(auto [_, id] : read[timestamp + 1]){
        object_future[id] = timestamp;
    }

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
                for(int j = 0; j < V; j++){
                    if(disk_reqs_activate[disk_id][x] || (disk[disk_id][x].first && object_future[disk[disk_id][x].first] == timestamp)) break;
                    x++;
                    if(x > V) x -= V;
                }
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
                        finished_count[object_id]++;
                        output.push_back(request_id);
                    }
                }
                for(int i = 1; i <= REP_NUM; i++){
                    {vector <int> tmp; swap(tmp, disk_reqs[object[object_id].replica[i]][object[object_id].unit[i][unit_id]]);};
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

    for(auto i : output){
        score += fscore(timestamp - request[i].timestamp) * gscore(object[request[i].object_id].size);
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

void Solution::gc_action()
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
            vec.push_back({-sum - (!valid_block[disk_id][i] * 100000), i});
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

void Solution::calc_tag(){
    vector <int> start_time(MAX_OBJECT_NUM, 1);
    vector <int> end_time(MAX_OBJECT_NUM, T);
    vector <int> object_id;

    for(auto [id, tag] : obj_tags){
        object[id].tag = tag;
    }

    for(int t = 1; t <= T + EXTRA_TIME; t++){
        for(auto &A : write_object[t]){
            int id = A.first;
            int &tag = A.second.second;
            if(finished_count[id] == 0){
                object[id].tag = M;
                object_id.push_back(id);
            }
            start_time[id] = t;
        }
    }
    for(int t = 1; t <= T + EXTRA_TIME; t++){
        for(auto &A : write_object[t]){
            int id = A.first;
            int &tag = A.second.second;
            tag = object[id].tag;
        }
    }
}

void Solution::init(){
    for(int t = 1; t <= T + EXTRA_TIME; t++){
        for(auto &A : write_object[t]){
            int id = A.first;
            auto [size, tag] = A.second;
            object[id].tag = tag;
            object[id].size = size;
        }
    }
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
            savedata[object[id].tag][t] += object[id].size;
        }
        for(auto [_, id] : read[t]){
            readdata[object[id].tag][t] += object[id].size;
        }
    }
    memset(object_future, 0, sizeof(object_future));
    memset(finished_count, 0, sizeof(finished_count));

    block_size = V / B;

    for(int i = 1; i <= M; i++){
        for(int j = 1; j <= N; j++){
            tag_disk[i][j] = j;
        }
        shuffle(tag_disk[i] + 1, tag_disk[i] + N + 1, engine);
    }

    score = 0;
    same_block.clear();
    memset(save_count, 0, sizeof(save_count));
    memset(read_count, 0, sizeof(read_count));
    memset(req_count, 0, sizeof(req_count));
    memset(disk, 0, sizeof(disk));
    memset(disk_size, 0, sizeof(disk_size));
    memset(disk_reqs_activate, 0, sizeof(disk_reqs_activate));
    for(int i = 1; i <= N; i++){
        for(int j = 0; j < POINTER_NUM; j++) disk_pointer[i][j] = 1, cross_block[i][j] = true, lst[i][j] = 64;
        disk_size[i] = V;
        for(int j = 1; j <= V; j++){
            vector <int> tmp;
            swap(tmp, disk_reqs[i][j]);
        }
    }
    {
        deque <int> tmp;
        swap(tmp, request_queue);
        memset(last_vis, 0, sizeof(last_vis));
        memset(valid_block, 0, sizeof(valid_block));
    }
    for(int i = 1; i <= N; i++){
        for(int j = 1; j <= B; j++){
            disk_block[i][j].L = (j - 1) * block_size + 1;
            disk_block[i][j].R = min(j * block_size, V);
            disk_block[i][j].tag = 0;
            disk_block[i][j].init_tag = 0;
            disk_block[i][j].count = 0;
            memset(disk_block[i][j].tag_count, 0, sizeof(disk_block[i][j].tag_count));
        }
    }
    for(int i = 1; i <= T + EXTRA_TIME; i++){
        {vector <int> tmp; swap(tmp, output_delete[i]);}
        {vector <pair <int, vector <vector <int> > > > tmp; swap(tmp, output_write[i]);}
        for(int j = 1; j <= N; j++){
            for(int k = 0; k < POINTER_NUM; k++){
                output_pointer[i][j][k] = "";
            }
        }
        {vector <int> tmp; swap(tmp, output_answer[i]);}
        {vector <int> tmp; swap(tmp, output_busy[i]);}
    }

    for(int i = 1; i <= M; i++){
        for(int t = T + EXTRA_TIME; t >= 1; t--){
            max_suf[i][t] = max(savedata[i][t], max_suf[i][t + 1] - deldata[i][t + 1] + savedata[i][t]);
        }
    }

    int maxsave[MAX_TAG_NUM] = {};
    int save[MAX_TAG_NUM] = {};
    int block_count[MAX_TAG_NUM] = {};
    int cnt = B * N / REP_NUM;
    for(int i = 1; i <= M; i++){
        save[i] = 0;
    }
    
    int mx_M = 0;
    for (int j = 1; j <= T + EXTRA_TIME; j++){
        save[M] -= deldata[M][j];
        save[M] += savedata[M][j];
        mx_M = max(mx_M, save[M]);
    }
    block_count[M] = (save[M] + block_size - 1) / block_size;
    cnt -= block_count[M];

    for (int j = 1; j <= T + EXTRA_TIME; j++){
        for(int i = 1; i <= M - 1; i++){
            save[i] -= deldata[i][j];
        }
        for(int i = 1; i <= M - 1; i++){
            save[i] += savedata[i][j];
            while(cnt && save[i] > block_count[i] * block_size){
                block_count[i]++;
                cnt--;
            }
        }
    }

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
            same_block[disk_block[i][j].block_num].push_back({i, j});
        }
    }

    for(int i = 1; i <= M; i++){
        tag_block[i].clear();
        for(auto x : tnum[i]){
            tag_block[i].push_back(same_block[x][0]);
        }
    }
}