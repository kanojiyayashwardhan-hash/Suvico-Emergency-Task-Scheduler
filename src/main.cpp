#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

struct Task {
    std::string id, title, location, status, createdAt;
    int severity = 1;                 // 4 Critical, 3 High, 2 Medium, 1 Low
    int deadlineMinutes = 60;
    long long sequence = 0;
};

static std::string nowText() {
    std::time_t t = std::time(nullptr);
    char b[32]; std::strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return b;
}
static json toJson(const Task& t) {
    return {{"id",t.id},{"title",t.title},{"location",t.location},{"status",t.status},
            {"severity",t.severity},{"deadlineMinutes",t.deadlineMinutes},
            {"createdAt",t.createdAt},{"sequence",t.sequence}};
}
static Task fromJson(const json& j) {
    Task t;
    t.id=j.value("id",""); t.title=j.value("title",""); t.location=j.value("location","");
    t.status=j.value("status","Pending"); t.severity=j.value("severity",1);
    t.deadlineMinutes=j.value("deadlineMinutes",60); t.createdAt=j.value("createdAt","");
    t.sequence=j.value("sequence",0LL); return t;
}

// Binary max-heap: higher severity first, then earlier deadline, then earlier arrival.
class TaskHeap {
    std::vector<Task> a;
    static bool better(const Task& x, const Task& y) {
        if (x.severity != y.severity) return x.severity > y.severity;
        if (x.deadlineMinutes != y.deadlineMinutes) return x.deadlineMinutes < y.deadlineMinutes;
        return x.sequence < y.sequence;
    }
    void up(size_t i) {
        while (i > 0) { size_t p=(i-1)/2; if (!better(a[i],a[p])) break;
            std::swap(a[i],a[p]); i=p; }
    }
    void down(size_t i) {
        while (true) { size_t l=2*i+1,r=l+1,b=i;
            if(l<a.size() && better(a[l],a[b])) b=l;
            if(r<a.size() && better(a[r],a[b])) b=r;
            if(b==i) break; std::swap(a[i],a[b]); i=b; }
    }
public:
    void push(const Task& t) { a.push_back(t); up(a.size()-1); }
    bool empty() const { return a.empty(); }
    Task pop() { Task t=a.front(); a.front()=a.back(); a.pop_back(); if(!a.empty()) down(0); return t; }
};

class Scheduler {
    std::unordered_map<std::string,Task> tasks; // expected O(1) ID lookup
    std::vector<Task> history;                  // array-backed task collection
    std::vector<json> undoStack;                 // stack of prior task states
    long long sequence = 0;
    const std::string dataFile = SUVICO_DATA_FILE;

    void persist() {
        json j=json::array(); for(const auto& t:history) j.push_back(toJson(t));
        std::ofstream out(dataFile); if(out) out << j.dump(2);
    }
    void rebuildIndex() {
        tasks.clear(); for(const auto& t:history) tasks[t.id]=t;
    }
    void load() {
        std::ifstream in(dataFile); if(!in) return;
        try { json j; in>>j; if(j.is_array()) for(auto& x:j) {
            Task t=fromJson(x); history.push_back(t); sequence=std::max(sequence,t.sequence);
        } rebuildIndex(); } catch(...) { std::cerr<<"Could not parse tasks.json; starting with an empty task list.\n"; }
    }
    void snapshot(const Task& t) { undoStack.push_back(toJson(t)); }
public:
    Scheduler() { std::filesystem::create_directories(std::filesystem::path(dataFile).parent_path()); load(); }
    json all(const std::string& sortBy="priority") const {
        std::vector<Task> v=history;
        if(sortBy=="deadline") std::stable_sort(v.begin(),v.end(),[](auto&a,auto&b){return a.deadlineMinutes<b.deadlineMinutes;});
        else if(sortBy=="arrival") std::stable_sort(v.begin(),v.end(),[](auto&a,auto&b){return a.sequence<b.sequence;});
        else std::stable_sort(v.begin(),v.end(),[](auto&a,auto&b){
            if(a.severity!=b.severity)return a.severity>b.severity;
            if(a.deadlineMinutes!=b.deadlineMinutes)return a.deadlineMinutes<b.deadlineMinutes;
            return a.sequence<b.sequence;
        });
        json out=json::array(); for(auto&t:v) out.push_back(toJson(t)); return out;
    }
    json add(const json& body) {
        std::string title=body.value("title","");
        if(title.empty()) throw std::runtime_error("Task title is required.");
        int sev=body.value("severity",1), dl=body.value("deadlineMinutes",60);
        if(sev<1||sev>4) throw std::runtime_error("Severity must be from 1 to 4.");
        if(dl<1||dl>10080) throw std::runtime_error("Deadline must be from 1 to 10080 minutes.");
        Task t; t.id="SV"+std::to_string(++sequence); t.sequence=sequence;
        t.title=title; t.location=body.value("location","Unspecified");
        t.severity=sev; t.deadlineMinutes=dl; t.status="Pending"; t.createdAt=nowText();
        history.push_back(t); tasks[t.id]=t; persist(); return toJson(t);
    }
    json updateStatus(const std::string& id, const std::string& status) {
        if(status!="Pending"&&status!="In Progress"&&status!="Completed"&&status!="Cancelled")
            throw std::runtime_error("Invalid status.");
        auto it=tasks.find(id); if(it==tasks.end()) throw std::runtime_error("Task ID not found.");
        snapshot(it->second);
        it->second.status=status;
        for(auto& t:history) if(t.id==id) t.status=status;
        persist(); return toJson(it->second);
    }
    json search(const std::string& id) const {
        auto it=tasks.find(id); if(it==tasks.end()) return nullptr; return toJson(it->second);
    }
    json nextTask() const {
        TaskHeap heap;
        for(const auto& t:history) if(t.status=="Pending") heap.push(t);
        if(heap.empty()) return nullptr;
        return toJson(heap.pop());
    }
    json undo() {
        if(undoStack.empty()) throw std::runtime_error("Nothing to undo.");
        json previous=undoStack.back(); undoStack.pop_back();
        Task old=fromJson(previous); auto it=tasks.find(old.id);
        if(it==tasks.end()) throw std::runtime_error("Task no longer exists.");
        it->second=old; for(auto& t:history) if(t.id==old.id) t=old;
        persist(); return toJson(old);
    }
    json stats() const {
        int pending=0,progress=0,completed=0,cancelled=0,critical=0;
        for(const auto&t:history) {
            if(t.status=="Pending") pending++; else if(t.status=="In Progress") progress++;
            else if(t.status=="Completed") completed++; else if(t.status=="Cancelled") cancelled++;
            if(t.severity==4 && t.status=="Pending") critical++;
        }
        return {{"total",(int)history.size()},{"pending",pending},{"inProgress",progress},
                {"completed",completed},{"cancelled",cancelled},{"criticalPending",critical}};
    }
    json route(const std::string& from, const std::string& to) const {
        // Sample road graph; weights are illustrative travel minutes, not live traffic.
        const std::vector<std::string> nodes={"Control Center","Hospital","Fire Station","Market","Residential Area","Industrial Area"};
        const int n=(int)nodes.size();
        std::unordered_map<std::string,int> idx; for(int i=0;i<n;i++)idx[nodes[i]]=i;
        if(!idx.count(from)||!idx.count(to)) throw std::runtime_error("Choose locations from the sample road network.");
        std::vector<std::vector<std::pair<int,int>>> g(n);
        auto edge=[&](int a,int b,int w){g[a].push_back({b,w});g[b].push_back({a,w});};
        edge(0,1,5); edge(0,2,4); edge(0,3,6); edge(1,3,3); edge(1,4,7);
        edge(2,5,5); edge(3,4,4); edge(3,5,8); edge(4,5,6);
        const int INF=std::numeric_limits<int>::max()/4;
        std::vector<int>d(n,INF),prev(n,-1); using P=std::pair<int,int>;
        std::priority_queue<P,std::vector<P>,std::greater<P>> pq;
        d[idx[from]]=0; pq.push({0,idx[from]});
        while(!pq.empty()){auto [dist,u]=pq.top();pq.pop();if(dist!=d[u])continue;
            for(auto [v,w]:g[u])if(d[v]>dist+w){d[v]=dist+w;prev[v]=u;pq.push({d[v],v});}}
        int target=idx[to]; std::vector<std::string> path;
        for(int at=target;at!=-1;at=prev[at])path.push_back(nodes[at]);
        std::reverse(path.begin(),path.end());
        return {{"from",from},{"to",to},{"estimatedMinutes",d[target]},{"path",path},
                {"note","Illustrative sample-network estimate; not live navigation."}};
    }
};

int main() {
    Scheduler scheduler;
    httplib::Server server;
    server.set_mount_point("/", SUVICO_PUBLIC_DIR);
    auto error=[](httplib::Response& res,const std::exception& e){
        res.status=400; res.set_content(json{{"error",e.what()}}.dump(),"application/json");
    };
    server.Get("/api/tasks",[&](const httplib::Request& req,httplib::Response& res){
        std::string sort=req.has_param("sort")?req.get_param_value("sort"):"priority";
        res.set_content(scheduler.all(sort).dump(),"application/json");
    });
    server.Get("/api/stats",[&](const httplib::Request&,httplib::Response&res){res.set_content(scheduler.stats().dump(),"application/json");});
    server.Get(R"(/api/tasks/([A-Za-z0-9_-]+))",[&](const httplib::Request&req,httplib::Response&res){
        auto j=scheduler.search(req.matches[1]); if(j.is_null()){res.status=404;res.set_content(R"({"error":"Task ID not found."})","application/json");}
        else res.set_content(j.dump(),"application/json");
    });
    server.Post("/api/tasks",[&](const httplib::Request&req,httplib::Response&res){
        try{res.status=201;res.set_content(scheduler.add(json::parse(req.body)).dump(),"application/json");}
        catch(const std::exception&e){error(res,e);}
    });
    server.Post(R"(/api/tasks/([A-Za-z0-9_-]+)/status)",[&](const httplib::Request&req,httplib::Response&res){
        try{auto b=json::parse(req.body);res.set_content(scheduler.updateStatus(req.matches[1],b.value("status","Pending")).dump(),"application/json");}
        catch(const std::exception&e){error(res,e);}
    });
    server.Get("/api/next",[&](const httplib::Request&,httplib::Response&res){res.set_content(scheduler.nextTask().dump(),"application/json");});
    server.Post("/api/undo",[&](const httplib::Request&,httplib::Response&res){
        try{res.set_content(scheduler.undo().dump(),"application/json");}catch(const std::exception&e){error(res,e);}
    });
    server.Get("/api/route",[&](const httplib::Request&req,httplib::Response&res){
        try{res.set_content(scheduler.route(req.get_param_value("from"),req.get_param_value("to")).dump(),"application/json");}
        catch(const std::exception&e){error(res,e);}
    });
    std::cout<<"Suvico is running at http://localhost:8080\nPress Ctrl+C to stop.\n";
    server.listen("127.0.0.1",8080);
}
