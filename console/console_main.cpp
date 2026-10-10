#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>
#include "../engine/Engine.h"

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

bool readInt(int& out) {
    std::string line;
    while (true) {
        if (!std::getline(std::cin, line)) return false;
        if (line.empty()) return false;
        std::istringstream ss(line);
        if (ss >> out && ss.eof()) return true;
        std::cout << "Not a valid number.\n> ";
    }
}

bool readDouble(double& out) {
    std::string line;
    while (true) {
        if (!std::getline(std::cin, line)) return false;
        if (line.empty()) return false;
        std::istringstream ss(line);
        if (ss >> out && ss.eof()) return true;
        std::cout << "Not a valid number.\n> ";
    }
}

bool readString(std::string& out) {
    if (!std::getline(std::cin, out)) return false;
    return !out.empty();
}

int main() {
    Engine e;
    
    while (true) {
        std::cout << "\n--- ReliefNet 2026 ---\n";
        std::cout << " 1  Load scenario (1 to 4)\n";
        std::cout << " 2  Add camp\n";
        std::cout << " 3  Edit camp\n";
        std::cout << " 4  Delete camp\n";
        std::cout << " 5  Search camp\n";
        std::cout << " 6  Submit request\n";
        std::cout << " 7  Process incoming\n";
        std::cout << " 8  Update request\n";
        std::cout << " 9  Handle next\n";
        std::cout << "10  Block / slow / open a road\n";
        std::cout << "11  Add road\n";
        std::cout << "12  Change average speed\n";
        std::cout << "13  Show data structures\n";
        std::cout << "14  Reset\n";
        std::cout << " 0  Exit\n";
        std::cout << "-----------------------\n";
        std::cout << "> ";
        
        int choice = -1;
        if (!readInt(choice)) {
            if (std::cin.eof()) break;
            continue;
        }
        
        if (choice == 0) {
            break;
        }
        else if (choice == 1) {
            std::cout << "Scenario number (1-4): ";
            int n;
            if (!readInt(n)) continue;
            
            std::string file;
            if (n == 1) file = "data/scenario1_normal.txt";
            else if (n == 2) file = "data/scenario2_blockage.txt";
            else if (n == 3) file = "data/scenario3_priority.txt";
            else if (n == 4) file = "data/scenario4_avl.txt";
            else {
                std::cout << "Invalid scenario.\n";
                continue;
            }
            
            std::string text = readFile(file);
            if (text.empty()) {
                std::cout << "File not found.\n";
                continue;
            }
            
            OpResult res = e.loadScenario(text);
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 2) {
            std::cout << "Camp ID: ";
            int id; if (!readInt(id)) continue;
            std::cout << "Name: ";
            std::string name; if (!readString(name)) continue;
            std::cout << "X coordinate: ";
            double x; if (!readDouble(x)) continue;
            std::cout << "Y coordinate: ";
            double y; if (!readDouble(y)) continue;
            std::cout << "Families: ";
            int fam; if (!readInt(fam)) continue;
            std::cout << "Links (space separated, 0=depot, blank=none): ";
            std::string linkLine;
            std::getline(std::cin, linkLine);
            std::vector<int> links;
            if (!linkLine.empty()) {
                std::istringstream ss(linkLine);
                int link;
                while (ss >> link) {
                    links.push_back(link);
                }
            }
            
            Camp c;
            c.id = id;
            c.name = name;
            c.x = x;
            c.y = y;
            c.families = fam;
            c.receivedFood = 0;
            c.receivedWater = 0;
            c.receivedMedicine = 0;
            
            OpResult res = e.addCamp(c, links);
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 3) {
            std::cout << "Camp ID: ";
            int id; if (!readInt(id)) continue;
            std::cout << "Name: ";
            std::string name; if (!readString(name)) continue;
            std::cout << "X coordinate: ";
            double x; if (!readDouble(x)) continue;
            std::cout << "Y coordinate: ";
            double y; if (!readDouble(y)) continue;
            std::cout << "Families: ";
            int fam; if (!readInt(fam)) continue;
            
            OpResult res = e.editCamp(id, name, x, y, fam);
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 4) {
            std::cout << "Camp ID to delete: ";
            int id; if (!readInt(id)) continue;
            
            OpResult res = e.deleteCamp(id, false);
            if (res.err == Err::ConfirmNeeded) {
                std::cout << res.message << ". Continue? (yes/no): ";
                std::string conf;
                if (!readString(conf)) continue;
                if (conf == "yes") {
                    res = e.deleteCamp(id, true);
                    std::cout << res.message << "\n";
                    std::cout << "Trace: " << e.lastTrace() << "\n";
                } else {
                    std::cout << "Cancelled.\n";
                }
            } else {
                std::cout << res.message << "\n";
                std::cout << "Trace: " << e.lastTrace() << "\n";
            }
        }
        else if (choice == 5) {
            std::cout << "Camp ID to search: ";
            int id; if (!readInt(id)) continue;
            Camp c;
            if (e.findCamp(id, c)) {
                std::printf("Camp %d: %s (%.2f, %.2f) Families: %d\n", c.id, c.name.c_str(), c.x, c.y, c.families);
                std::printf("Received: Food %d, Water %d, Medicine %d\n", c.receivedFood, c.receivedWater, c.receivedMedicine);
            } else {
                std::cout << "Camp not found.\n";
            }
        }
        else if (choice == 6) {
            std::cout << "Camp ID: ";
            int campId; if (!readInt(campId)) continue;
            std::cout << "Resource (food/water/medicine): ";
            std::string resStr; if (!readString(resStr)) continue;
            ResourceType resType;
            if (resStr == "food") resType = ResourceType::Food;
            else if (resStr == "water") resType = ResourceType::Water;
            else if (resStr == "medicine") resType = ResourceType::Medicine;
            else { std::cout << "Invalid resource.\n"; continue; }
            std::cout << "Quantity: ";
            int qty; if (!readInt(qty)) continue;
            std::cout << "Urgency (1-4): ";
            int urg; if (!readInt(urg)) continue;
            std::cout << "Families: ";
            int fam; if (!readInt(fam)) continue;
            
            OpResult res = e.submitRequest(campId, resType, qty, urg, fam);
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 7) {
            OpResult res = e.processIncoming();
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 8) {
            std::cout << "Request ID: ";
            int id; if (!readInt(id)) continue;
            std::cout << "Urgency (1-4): ";
            int urg; if (!readInt(urg)) continue;
            std::cout << "Families: ";
            int fam; if (!readInt(fam)) continue;
            std::cout << "Quantity: ";
            int qty; if (!readInt(qty)) continue;
            
            OpResult res = e.updateRequest(id, urg, fam, qty);
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 9) {
            OpResult res = e.handleNext();
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 10) {
            std::cout << "Node A: ";
            int a; if (!readInt(a)) continue;
            std::cout << "Node B: ";
            int b; if (!readInt(b)) continue;
            std::cout << "Status (open/slow/blocked): ";
            std::string status; if (!readString(status)) continue;
            RoadStatus rs;
            if (status == "open") rs = RoadStatus::Open;
            else if (status == "slow") rs = RoadStatus::Slow;
            else if (status == "blocked") rs = RoadStatus::Blocked;
            else { std::cout << "Invalid status.\n"; continue; }
            
            OpResult res = e.setRoadStatus(a, b, rs);
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 11) {
            std::cout << "Node A: ";
            int a; if (!readInt(a)) continue;
            std::cout << "Node B: ";
            int b; if (!readInt(b)) continue;
            
            OpResult res = e.addRoad(a, b);
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 12) {
            std::cout << "Speed (km/h): ";
            double s; if (!readDouble(s)) continue;
            
            OpResult res = e.setAverageSpeed(s);
            std::cout << res.message << "\n";
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else if (choice == 13) {
            while (true) {
                std::cout << "\n--- Data Structures ---\n";
                std::cout << "a. Incoming queue\n";
                std::cout << "b. Active list\n";
                std::cout << "c. Delivery log\n";
                std::cout << "d. AVL tree\n";
                std::cout << "e. Camps\n";
                std::cout << "f. Roads\n";
                std::cout << "g. Routes\n";
                std::cout << "h. Dijkstra steps\n";
                std::cout << "i. Depot status\n";
                std::cout << "(blank to return)\n";
                std::cout << "> ";
                
                std::string sub;
                std::getline(std::cin, sub);
                if (sub.empty()) break;
                
                if (sub == "a") {
                    auto q = e.queueContents();
                    std::cout << "Incoming queue (" << q.size() << " requests):\n";
                    for (const auto& r : q) {
                        std::string resName = (r.resource == ResourceType::Food) ? "food" :
                                              (r.resource == ResourceType::Water) ? "water" : "medicine";
                        std::printf("Req %d: camp %d, %d %s, urg %d, fam %d\n", 
                                    r.id, r.campId, r.quantity, resName.c_str(), r.urgency, r.families);
                    }
                }
                else if (sub == "b") {
                    auto ranked = e.rankedRequests();
                    std::cout << "Active list (" << ranked.size() << " requests):\n";
                    for (const auto& rr : ranked) {
                        const Request& r = rr.req;
                        std::string resName = (r.resource == ResourceType::Food) ? "food" :
                                              (r.resource == ResourceType::Water) ? "water" : "medicine";
                        std::printf("Req %d: camp %d needs %d %s, urg %d, fam %d.", 
                                    r.id, r.campId, r.quantity, resName.c_str(), r.urgency, r.families);
                        if (rr.hasReason) {
                            std::string reasonStr;
                            switch (rr.reason) {
                                case RankKey::Urgency: reasonStr = "urgency"; break;
                                case RankKey::Families: reasonStr = "families"; break;
                                case RankKey::Quantity: reasonStr = "quantity"; break;
                                case RankKey::Arrival: reasonStr = "arrival"; break;
                            }
                            std::printf(" [Ranked over next: %s]", reasonStr.c_str());
                        }
                        std::cout << "\n";
                    }
                }
                else if (sub == "c") {
                    auto log = e.deliveries();
                    std::cout << "Delivery log (" << log.size() << " records):\n";
                    for (const auto& dr : log) {
                        std::string resName = (dr.resource == ResourceType::Food) ? "food" :
                                              (dr.resource == ResourceType::Water) ? "water" : "medicine";
                        std::printf("Req %d: delivered %d of %d %s to camp %d in %d min.\n",
                                    dr.requestId, dr.quantityDelivered, dr.quantityRequested, resName.c_str(), dr.campId, dr.totalMinutes);
                    }
                }
                else if (sub == "d") {
                    std::vector<AVLNodeInfo> avl;
                    e.avlSnapshot(avl);
                    if (avl.empty()) {
                        std::cout << "(empty)\n";
                    } else {
                        for (const auto& n : avl) {
                            for (int i = 0; i < n.depth; ++i) std::cout << "    ";
                            if (n.side == 'T') std::cout << "ROOT ";
                            else std::cout << n.side << ": ";
                            std::printf("%d  (h=%d, bf=%d)\n", n.id, n.height, n.balance);
                        }
                    }
                }
                else if (sub == "e") {
                    auto camps = e.camps();
                    std::cout << "Camps:\n";
                    for (const auto& c : camps) {
                        std::printf("%3d | %-15s | (%.1f, %.1f) | Fam: %d | Recv: %d F, %d W, %d M\n",
                                    c.id, c.name.c_str(), c.x, c.y, c.families, c.receivedFood, c.receivedWater, c.receivedMedicine);
                    }
                }
                else if (sub == "f") {
                    auto roads = e.roads();
                    std::cout << "Roads:\n";
                    for (const auto& r : roads) {
                        std::string status = (r.status == RoadStatus::Open) ? "Open" : 
                                             (r.status == RoadStatus::Blocked) ? "Blocked" : "Slow";
                        std::printf("%3d - %3d | %6.2f km | %s\n", r.a, r.b, r.distanceKm, status.c_str());
                    }
                }
                else if (sub == "g") {
                    auto routes = e.routes();
                    std::cout << "Routes:\n";
                    for (const auto& r : routes) {
                        if (!r.reachable) {
                            std::printf("Camp %d: Unreachable: alternate delivery needed\n", r.campId);
                        } else {
                            std::printf("Camp %d: %3d min | path: ", r.campId, r.minutes);
                            for (size_t i = 0; i < r.path.size(); ++i) {
                                if (i > 0) std::cout << " -> ";
                                std::cout << r.path[i];
                            }
                            std::cout << "\n";
                        }
                    }
                }
                else if (sub == "h") {
                    auto steps = e.dijkstra().steps();
                    std::cout << "Dijkstra steps:\n";
                    for (const auto& s : steps) {
                        std::printf("pick node %d at %d min, improved: ", s.node, s.minutes);
                        if (s.improved.empty()) {
                            std::cout << "none\n";
                        } else {
                            for (size_t i = 0; i < s.improved.size(); ++i) {
                                if (i > 0) std::cout << ", ";
                                std::cout << s.improved[i];
                            }
                            std::cout << "\n";
                        }
                    }
                }
                else if (sub == "i") {
                    auto stock = e.stock();
                    std::printf("Depot position: (%.2f, %.2f)\n", e.depotX(), e.depotY());
                    std::printf("Average speed: %.2f km/h\n", e.averageSpeed());
                    std::printf("Stock: Food %d, Water %d, Medicine %d\n", stock.food, stock.water, stock.medicine);
                }
                else {
                    std::cout << "Invalid option.\n";
                }
            }
        }
        else if (choice == 14) {
            e.reset();
            std::cout << "Trace: " << e.lastTrace() << "\n";
        }
        else {
            std::cout << "Invalid choice.\n";
        }
    }
    
    return 0;
}
