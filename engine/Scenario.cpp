#include "Engine.h"
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>

OpResult Engine::loadScenario(const std::string& text) {
    reset();
    
    int lineNum = 0;
    
    int campsCount = 0;
    int roadsCount = 0;
    int requestsCount = 0;
    std::vector<std::string> allRotations;
    
    size_t pos = 0;
    while (pos < text.length()) {
        size_t endPos = text.find('\n', pos);
        std::string line = text.substr(pos, endPos == std::string::npos ? std::string::npos : endPos - pos);
        if (endPos == std::string::npos) pos = text.length();
        else pos = endPos + 1;
        
        lineNum++;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty() || line[0] == '#') continue;
        
        std::vector<std::string> words;
        size_t wPos = 0;
        while (wPos < line.length()) {
            while (wPos < line.length() && std::isspace(static_cast<unsigned char>(line[wPos]))) wPos++;
            if (wPos == line.length()) break;
            size_t wEnd = wPos;
            while (wEnd < line.length() && !std::isspace(static_cast<unsigned char>(line[wEnd]))) wEnd++;
            words.push_back(line.substr(wPos, wEnd - wPos));
            wPos = wEnd;
        }
        
        if (words.empty()) continue;
        
        auto fail = [&](const std::string& reason) -> OpResult {
            reset();
            return {Err::Invalid, "line " + std::to_string(lineNum) + ": " + reason, 0};
        };
        
        std::string cmd = words[0];
        
        auto parseDouble = [&](const std::string& s, double& out) {
            char* end = nullptr;
            out = std::strtod(s.c_str(), &end);
            // strtod also accepts "nan" and "inf"; those are not valid map values here.
            return end != s.c_str() && *end == '\0' && std::isfinite(out);
        };
        auto parseInt = [&](const std::string& s, int& out) {
            char* end = nullptr;
            long v = std::strtol(s.c_str(), &end, 10);
            if (end == s.c_str() || *end != '\0') return false;
            if (v < INT_MIN || v > INT_MAX) return false;   // would be silently truncated otherwise
            out = static_cast<int>(v);
            return true;
        };

        if (cmd == "depot") {
            if (words.size() != 3) return fail("wrong number of words");
            double x, y;
            if (!parseDouble(words[1], x) || !parseDouble(words[2], y)) return fail("bad number");
            OpResult r = setDepotPosition(x, y);
            if (!r.ok()) return fail(r.message);
        }
        else if (cmd == "speed") {
            if (words.size() != 2) return fail("wrong number of words");
            double s;
            if (!parseDouble(words[1], s)) return fail("bad number");
            OpResult r = setAverageSpeed(s);
            if (!r.ok()) return fail(r.message);
        }
        else if (cmd == "stock") {
            if (words.size() != 4) return fail("wrong number of words");
            int f, w_res, m;
            if (!parseInt(words[1], f) || !parseInt(words[2], w_res) || !parseInt(words[3], m)) return fail("bad number");
            OpResult r = setStock(f, w_res, m);
            if (!r.ok()) return fail(r.message);
        }
        else if (cmd == "camp") {
            if (words.size() != 6) return fail("wrong number of words");
            Camp c;
            c.name = words[2];
            if (!parseInt(words[1], c.id) || !parseDouble(words[3], c.x) || !parseDouble(words[4], c.y) || !parseInt(words[5], c.families)) return fail("bad number");
            c.receivedFood = 0;
            c.receivedWater = 0;
            c.receivedMedicine = 0;
            
            OpResult r = addCamp(c, {});
            if (!r.ok()) return fail(r.message);
            campsCount++;
            
            const auto& rots = camps_.lastRotations();
            for (const auto& rot : rots) {
                std::string rName;
                switch (rot.type) {
                    case RotationType::LL: rName = "LL"; break;
                    case RotationType::LR: rName = "LR"; break;
                    case RotationType::RR: rName = "RR"; break;
                    case RotationType::RL: rName = "RL"; break;
                }
                allRotations.push_back(rName + " at node " + std::to_string(rot.nodeId));
            }
        }
        else if (cmd == "road") {
            if (words.size() != 3) return fail("wrong number of words");
            int a, b;
            if (!parseInt(words[1], a) || !parseInt(words[2], b)) return fail("bad number");
            OpResult r = addRoad(a, b);
            if (!r.ok()) return fail(r.message);
            roadsCount++;
        }
        else if (cmd == "request") {
            if (words.size() != 6) return fail("wrong number of words");
            int campId;
            if (!parseInt(words[1], campId)) return fail("bad number");
            
            std::string resStr = words[2];
            ResourceType res;
            if (resStr == "food") res = ResourceType::Food;
            else if (resStr == "water") res = ResourceType::Water;
            else if (resStr == "medicine") res = ResourceType::Medicine;
            else return fail("unknown resource");
            
            int qty, urg, fam;
            if (!parseInt(words[3], qty) || !parseInt(words[4], urg) || !parseInt(words[5], fam)) return fail("bad number");
            
            OpResult r = submitRequest(campId, res, qty, urg, fam);
            if (!r.ok()) return fail(r.message);
            requestsCount++;
        }
        else {
            return fail("unknown command");
        }
    }
    
    std::string rotList;
    if (allRotations.empty()) {
        rotList = "none";
    } else {
        for (size_t i = 0; i < allRotations.size(); ++i) {
            if (i > 0) rotList += ", ";
            rotList += allRotations[i];
        }
    }
    
    lastTrace_ = "Scenario loaded: " + std::to_string(campsCount) + " camps, " + 
                 std::to_string(roadsCount) + " roads, " + 
                 std::to_string(requestsCount) + " requests queued | AVL rotations: " + rotList;
                 
    return {Err::None, "Scenario loaded", requestsCount};
}
