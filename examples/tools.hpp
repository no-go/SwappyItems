// we need it for PRId32 in snprintf
#define __STDC_FORMAT_MACROS

// -------------------------------------------------------------------------
string replacer(string& s, const string& toReplace, const string& replaceWith) {
    size_t pos = 0;
    bool endless = true;
    while(endless) {
        pos = s.find(toReplace, pos);
        if (pos == std::string::npos) {
            endless = false;
        } else {
            s = s.replace(pos, toReplace.length(), replaceWith);
        }
    }
    
    return s;
}

// -------------------------------------------------------------------------
int parseLine(char* line) {
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

// -------------------------------------------------------------------------
int getUsedKB() {
    FILE* fi = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, fi) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(fi);
    return result;
}

// -------------------------------------------------------------------------
void my_handler(int s) {
    printf("# Caught signal %d\n", s);
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("#end (before hibernate): %.f ms\n", mseconds);
    
    delete places;
    delete ways;
    delete nodes;
    
    exit(1);
}

// -------------------------------------------------------------------------
void catchSig() {
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
}

// -------------------------------------------------------------------------
void logHead() {
    printf("# cores:                  %12d\n", thread::hardware_concurrency());
    printf("# items in a single file: %12d\n", FILE_ITEMS);
    printf("# swap if more than       %12d items in RAM are reached\n",  RAM_MULTI * FILE_MULTI * FILE_ITEMS);
    printf("# swap into               %12d files\n", FILE_MULTI);
    printf("# use                     %12d Bloom bits for a key in a file bitmask\n", BBITS);
    printf("# use a bitmask with      %12d bits for each file\n", BMASK);
}

// -------------------------------------------------------------------------
void logEntry(double & msec, std::chrono::time_point<std::chrono::system_clock> & old, atomic<bool> & isprnt) {
    isprnt = true;
    auto stat = nodes->getStatistic();
    auto now = std::chrono::high_resolution_clock::now();
    msec     = std::chrono::duration<double, std::milli>(now-old).count();
    printf(
        "%10.f ms "
        "%10ld i "
        "%10ld q "
        
        "%10" PRId64 " u "
        "%10" PRId64 " d "

        "%10" PRId64 " r "
        "%10" PRId64 " b "
        "%10" PRId64 " e "
        
        "%10" PRId64 " s "
        "%10" PRId64 " l "
        "%10d kB "
        "%10ld filekB "

        "%10" PRId64 " "
        "%10" PRId64 "\n",
        
        msec,
        stat.size,
        stat.queue,
        
        stat.updates,
        stat.deletes,
        
        stat.rangeSaysNo, 
        stat.bloomSaysNotIn,
        stat.rangeFails,
        
        stat.swaps,
        stat.fileLoads,
        getUsedKB(),
        stat.fileKB,
        
        stat.minKey,
        stat.maxKey
    );
}

// -------------------------------------------------------------------------
bool catchTown(PlaceData & dummy, const uint64_t & osmid, double & lon, double & lat, const Tags & tags) {
    if (tags.find("place") == tags.end()) return false;
    
    string pt = tags.at("place");
    dummy._type = 1;
    
    if (pt.compare("village") == 0) {
        dummy._type = 2;
    } else if (pt.compare("town") == 0) {
        dummy._type = 3;
    } else if (pt.compare("city") == 0) {
        dummy._type = 4;
    }
    dummy._lon = lon;
    dummy._lat = lat;

    if (tags.find("name") != tags.end()) {
        pt = tags.at("name");
        pt = replacer(pt, "&", "und");
        pt = replacer(pt, "\"", "'");
        pt = replacer(pt, "<", "");
        pt = replacer(pt, ">", "");
    } else {
        pt = (string) "Dingenskirchen";
    }
    snprintf(dummy._name, 256, "%s", pt.c_str());
    return true;
}

// -------------------------------------------------------------------------
bool catchWay(WayData & dummy, const uint64_t & osmid, const Tags & tags) {
    if (tags.find("highway") == tags.end()) return false;

    dummy._type = 0;
    string wt = tags.at("highway");
    
    if (wt.compare("motorway") == 0) {
        dummy._type = 10;
    } else if (wt.compare("motorway_link") == 0) {
        dummy._type = 10;
    } else if (wt.compare("trunk") == 0) {
        dummy._type = 20;
    } else if (wt.compare("trunk_link") == 0) {
        dummy._type = 20;
    } else if (wt.compare("primary") == 0) {
        dummy._type = 30;
    } else if (wt.compare("primary_link") == 0) {
        dummy._type = 30;
    } else if (wt.compare("secondary") == 0) {
        dummy._type = 40;
    } else if (wt.compare("unclassified") == 0) {
        dummy._type = 50;
    } else if (wt.compare("tertiary") == 0) {
        dummy._type = 50;
    } else if (wt.compare("residential") == 0) {
        dummy._type = 60;
    }
    
    if (dummy._type == 0) return false;

    string wayName = "";
    if(tags.find("name") != tags.end()){
        wayName = tags.at("name");
    } else if (tags.find("ref") != tags.end()) {
        wayName = tags.at("ref");
    } else if (tags.find("destination:ref") != tags.end() && tags.find("destination") != tags.end()) {
        wayName = tags.at("destination:ref") + (string) " Richtung " + tags.at("destination");
    }
    wayName = replacer(wayName, "&", "und");
    wayName = replacer(wayName, "\"", "'");
    wayName = replacer(wayName, "<", "");
    wayName = replacer(wayName, ">", "");

    snprintf(dummy._name, 256, "%s", wayName.c_str());

    return true;
}



