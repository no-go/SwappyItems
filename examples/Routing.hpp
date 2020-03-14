
struct Routing {

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        pair<WayData, vector<Key> > way;
        
        if (catchWay(way.first, osmid, tags)) { // fills in basis way data
            
            for (Key r : refs) {
                way.second.push_back(r);
                pair<NodeData, vector<Key> > * nodeptr = nodes->get(r);
                if (nodeptr == nullptr) {
                    pair<NodeData, vector<Key> > node;
                    node.first._used = 0;
                    node.first._lon = 0.0;
                    node.first._lat = 0.0;
                    nodes->set(r, node.first);
                } else {
                    // node is also used in another way!
                    nodeptr->first._used++;
                    nodes->set(r, nodeptr->first);
                    isPrinted = false;
                }
            }
            // tests: osmid of the way is always new OR you read a 2nd time because of hibernate!
            ways->set(osmid, way.first, way.second);
            
            if ((nodes->getStatistic().updates%1024 == 0) && (isPrinted == false)) logEntry(mseconds, start, isPrinted);
        }
    }

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {
        pair<NodeData, vector<Key> > * nodeptr = nodes->get(osmid);
        pair<PlaceData, vector<Key> > place;
        
        if (nodeptr == nullptr) {
            // it seams to be not a way: we store it as place?
            if (catchTown(place.first, osmid, lon, lat, tags)) {
                places->set(osmid, place.first);
            }
        } else {
            // this osmid node is part of a way, because we read way first and it exist now
            // -> set lon and lat!
            nodeptr->first._used++;
            nodeptr->first._lon = lon;
            nodeptr->first._lat = lat;
            nodes->set(osmid, nodeptr->first);
            isPrinted = false;
        }

        if ((nodes->getStatistic().updates%1024 == 0) && (isPrinted == false)) logEntry(mseconds, start, isPrinted);
    }
    
    void relation_callback (uint64_t /*osmid*/, const Tags &/*tags*/, const References & refs){}
};
