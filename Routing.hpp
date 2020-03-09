
struct Routing {

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        pair<Value, vector<Key> > parent;
        pair<Value, vector<Key> > dummy;

        if (catchWay(parent.first, osmid, tags)) { // fills in basis way data
            pair<Value, vector<Key> > * wa;
            pair<Value, vector<Key> > * waref;
            
            wa = ways->get(osmid);
            
            if (wa == nullptr) {
                for (auto r : refs) parent.second.push_back(r);
                ways->set(osmid, parent.first, parent.second);
            }

            for (size_t i=0; i < refs.size(); ++i) {
                waref = ways->get(refs[i]);
                
                if ((ways->size()%1024 == 0) && (isPrinted == false)) logEntry(mseconds, start, isPrinted);

                if (waref == nullptr) {
                    // new!
                    ValueSet(
                        parent.first,
                        (i==0 ? osmid : refs[i-1]), 
                        parent.first._lon, 
                        parent.first._lat, 
                        parent.first._type, 
                        0
                    );
                    // prevent a log print, if size not changes
                    isPrinted = false;
                    
                } else {
                    // update
                    ValueSet(
                        dummy.first,
                        waref->first._parent,
                        waref->first._lon,
                        waref->first._lat,
                        waref->first._type,
                        waref->first._uses+1
                    );
                }
                ways->set(refs[i], dummy.first);
            }
        }
    }

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {
        pair<Value, vector<Key> > * wa = ways->get(osmid);
        Value dummy;
        
        if (wa == nullptr) {
            // it seams to be not a way: we store it as town?
            if (catchTown(dummy, osmid, lon, lat, tags)) {
                ways->set(osmid, dummy);
            }
        } else {
            // this osmid node is part of a way, because we read way first and it exist now
            // -> set lon and lat!
            ValueSet(dummy, wa->first._parent, lon, lat, wa->first._type, wa->first._uses);
            snprintf(dummy._name, 256, "%s", wa->first._name);
            ways->set(osmid, dummy);
            isPrinted = false;
        }

        if ((ways->statistic.updates%1024 == 0) && (isPrinted == false)) logEntry(mseconds, start, isPrinted);
    }
    
    void relation_callback (uint64_t /*osmid*/, const Tags &/*tags*/, const References & refs){}
};
