/******************************************************************************
 * @file vmsim.c
 * @brief simulates different paging algorithms
 * @author Joshua Spisak <jjs231@pitt.edu>
 * @note I had a lot of fun with templates with this lol. There is one
 *      particularly good usage of templates and one that could have been
 *      much more well done with just interfaces. Overall I just wanted to
 *      challenge myself to use only templates rather than inheritance
 *      for this.
 * @usage ./vmsim –n <numframes> -a <opt|lru|second> <tracefile>
 *****************************************************************************/
#include <cstdio> // printf()
#include <string> // string
#include <fstream> // ifstream
#include <vector> // vector
#include <unordered_map> // unordered_map
#include <exception> // runtime_error
#include <queue> // queue

/************************** Utility Functions    *****************************/
namespace util {
//! Used to convert char* command line arguments to a given type
template<typename arg_type>
bool convert(char* argument, arg_type& ret_val);

//! converts a char* command line argument to a string 
bool convert(char* argument, std::string& ret_val) {
    ret_val = std::string(argument);
    return true;
}

//! converts a char* command line argument to an integer 
bool convert(char* argument, int& ret_val) {
    ret_val = std::atoi(argument);
    return true;
}

// NOTE: this was copied over from project 2
/**
 * @brief gets a command line argument of a given type
 * @param argc cli argc
 * @param argv cli argv
 * @param flag the flag to use to find this argument
 * @param ret_val argument to return by reference
 * @return true if successfully loaded, false if not
 **/
template<typename arg_type>
bool get_arg(int argc, char** argv, std::string flag, arg_type& ret_val) {
    for(int i = 0; i < argc; ++i) {
        if(argv[i] == flag) {
            if(i + 1 >= argc) {
                return false;
            }
            return convert(argv[i + 1], ret_val);
        }
    }
    return false;
}

/**
 * @brief gets the last nth argument from command line
 * @param argc cli argc
 * @param argv cli argv
 * @param n the how far from the last argument to select (0 for last, argc - 1 for first)
 * @param ret_val argument to return by reference
 * @return true if successfully loaded, false if not
 * @note really dumb rn, but eventually could skip over flag arguments
 **/
template<typename arg_type>
bool get_last_nth_arg(int argc, char** argv, int n, arg_type& ret_val) {
    // prevent underflow
    if(n + 1 > argc) {
        return false;
    }
    return convert(argv[argc - n - 1], ret_val);
}
} /* namespace util */
/************************** Trace Declarations   *****************************/
//! A single trace entry
struct trace_entry_t {
    //! Mode (l: load, s: store)
    std::string mode;
    //! Address to perform the mode on
    unsigned int address;
};

//! Make a Trace a vector that can be loaded from a file
class Trace : public std::vector<trace_entry_t> {
public:
    /**
     * @brief creates a Trace from a file name
     * @param file_name file to load traces from
     **/
    Trace(std::string file_name) {
        std::ifstream input(file_name);
        if(!input.is_open()) {
            // If file does not exist trace will be empty
            return;
        }

        // Read in entries
        trace_entry_t new_entry;
        while(true) {
            if(input.eof())
                return;

            input >> std::skipws >> new_entry.mode >> std::hex >> new_entry.address;
            if(input.fail())
                return;

            push_back(new_entry);
        }
    } /* Trace() */
}; /* class Trace */

/************************** Page Table Declarations **************************/
/**
 * @brief contains the necessary PageTable data to do this sim
 *      also executes all the trace entries and tracks statistics
 * @param Algorithm algorithm to use when handling page faults
 * @param page_offset_bits number of bits in the address for the page offset
 **/
template<typename Algorithm, int page_offset_bits = 12>
class PageTable {
public:
    //! Stats on memory accesses
    struct Stats {
        //! Number of hits
        int hits = 0;
        //! Number of page faults without evictions (frame_count_ not hit yet)
        int pf_evict_none = 0;
        //! Number of page faults with a clean eviction
        int pf_evict_clean = 0;
        //! Number of page faults with a dirty eviction
        int pf_evict_dirty = 0;
        //! Number of accesses that were reads
        int loads = 0;
        //! Number of accesses that were writes
        int stores = 0;
    };
    //! An entry for a single frame
    struct Frame {
        //! Frame ID
        unsigned int frame_id;
        //! Whether or not this frame has been dirtied
        bool dirty;
        //! Info specific to the algorithm
        typename Algorithm::FrameInfo info;
    };
    //! Map of frame_id -> Frame info
    typedef std::unordered_map<int, Frame> FrameMap;

    /**
     * @brief creates a page table with a given frame count
     **/
    PageTable(int frame_count):
        frame_count_(frame_count),
        available_frames_(frame_count)
    {}

    /**
     * @brief exectues a given trace entry
     * @param trace what to execute
     **/
    void execute(trace_entry_t trace) {
        unsigned int frame_id = trace.address >> page_offset_bits;
        
        auto it = frame_map_.find(frame_id);
        if(it == frame_map_.end()) {
            // we do not have that frame paged in...
            // see if we need to evict a frame first
            if(available_frames_ <= 0) {
                // Find which frame via algorithm
                unsigned int evict_id = Algorithm::nextEviction(frame_map_, algorithm_state_);
                auto frame = frame_map_[evict_id]; // we assume it gives us something our map
                // Collect stats and evict!
                if(frame.dirty)
                    ++stats_.pf_evict_dirty;
                else
                    ++stats_.pf_evict_clean;
                frame_map_.erase(evict_id);
            } else {
                // empty frame is available already, mark stats
                ++stats_.pf_evict_none;
                --available_frames_;
            }
            // Insert the frame to the map
            frame_map_.insert({frame_id, Frame()});
            it = frame_map_.find(frame_id);
            it->second.dirty = false;
            it->second.frame_id = frame_id;
        } else {
            ++stats_.hits;
        }

        // Execute read / write on frame and update stats
        if(trace.mode == "s") {
            it->second.dirty = true;
            ++stats_.stores;
        } else if (trace.mode == "l") {
            ++stats_.loads;
        } else {
            std::printf("WARNING: Invalid mode- %s\n", trace.mode.c_str());
        }
        Algorithm::update(it->second, algorithm_state_);
    } /* execute */

    Stats getStats() {
        return stats_;
    }

private:
    //! Total number of frames in table
    int frame_count_;
    //! Next frame to fill (range: [0, frame_count_) )
    int available_frames_;
    //! Currently paged in frames
    FrameMap frame_map_;
    //! State specific to the frame_map (any linked lists, ints, etc..)
    typename Algorithm::State algorithm_state_;
    //! Collection of statistics about page operation
    Stats stats_;
};

/************************** Algorithm Declarations  **************************/
/**
 * @brief interface for different paging algorithms to be tested with the Page Table
 * @note this could have been done better using actual class inheritance, but I'm
 *      just having fun with templating so meh.
 **/
class IAlgorithm {
public:
    //! Define any extra information to be in the Frame Entry
    typedef void *FrameInfo;
    //! Define any extra state that should be maintained in the PageTable
    typedef void *State;
    /**
     * @brief from a given frame mapping, determine the next eviction
     * @param frame_map the mapping to use to determine this
     * @param state state that can be maintained in the page table
     **/
    static unsigned int nextEviction(PageTable<IAlgorithm>::FrameMap& frame_map, State& state)
    { throw std::runtime_error("Interface not implemented"); }
    /**
     * @brief updates a given frame on an access
     * @param frame the frame that was accessed
     * @param state state that can be maintained in the page table
     **/
    static void update(PageTable<IAlgorithm>::Frame& frame, State& state)
    { throw std::runtime_error("Interface not implemented"); }
}; /* interface IAlgorithm */

/**
 * @brief implementation of LRU Paging algorithm
 **/
class LRUAlgorithm : public IAlgorithm {
public:
    //! Frame info for the LRU Algorithm
    struct FrameInfo {
        unsigned int use_stamp; // NOTE: my usage of an unsigned int for the stamp
                                // limits the program to MAX UINTish trace entries
                                // IDK, seemed reasonable to me.
    };
    //! State for LRU Algorithm
    struct State {
        //! Current access stamp (higher means later access) 
        int current_stamp = 0;
    };
    /**
     * @brief performs LRU eviction on a page table
     * @param frame_map frames to select from when choosing which to evict
     * @param state unused, part of interface
     * @details finds the lowest stamped frame and evicts it
     **/
    static unsigned int nextEviction(PageTable<LRUAlgorithm>::FrameMap& frame_map, State& state) {
        unsigned int lowest_val = -1;
        unsigned int ret_val;
        for(auto it : frame_map) {
            if(it.second.info.use_stamp < lowest_val) {
                lowest_val = it.second.info.use_stamp;
                ret_val = it.first;
            }
        }
        return ret_val;
    };
    /**
     * @brief updates a frames stamp when it is accessed
     * @param frame the frame to update
     * @param state maintains the current stamp in the state
     **/
    static void update(PageTable<LRUAlgorithm>::Frame& frame, State& state) {
        frame.info.use_stamp = state.current_stamp;
        ++state.current_stamp;
    };
}; /* class LRUAlgorithm */

/**
 * @brief implementation of Second Change Paging algorithm
 **/
class SecondAlgorithm : public IAlgorithm {
public:
    //! Frame info for the LRU Algorithm
    struct FrameInfo {
        //! Simply used to 
        bool inserted = false;
        //! Whether or not this Frame has been referenced
        bool referenced = false;
    };
    //! State for Second Algorithm
    struct State {
        //! Queue of frames to remove
        //! Note: A more efficient circular buffer could be written / used but meh
        std::queue<unsigned int> frame_queue;
    };
    /**
     * @brief performs LRU eviction on a page table
     * @param frame_map frames to select from when choosing which to evict
     * @param state unused, part of interface
     * @details finds the lowest stamped frame and evicts it
     **/
    static unsigned int nextEviction(PageTable<SecondAlgorithm>::FrameMap& frame_map, State& state) {
        unsigned int result;
        while(true) {
            // Check the front of the queue
            result = state.frame_queue.front();
            state.frame_queue.pop();
            auto& frame = frame_map[result];
            // If they haven't been referenced then we'll evict that one
            if(!frame.info.referenced) {
                break;
            }
            // Otherwise we set them to unreferenced and add them to the back of the queue
            frame.info.referenced = false;
            state.frame_queue.push(result);
            // NOTE: if they are all referenced then it will circle back to the first entry
            //  again which will be unreferenced the second time around
        }
        return result;
    }
    /**
     * @brief updates a frames stamp when it is accessed
     * @param frame the frame to update
     * @param state maintains the current stamp in the state
     **/
    static void update(PageTable<SecondAlgorithm>::Frame& frame, State& state) {
        if(!frame.info.inserted) {
            state.frame_queue.push(frame.frame_id);
            frame.info.inserted = true;
        } else {
            frame.info.referenced = true;
        }
    };
}; /* class SecondAlgorithm */

//! Allows global access to the trace for the OPT Algorithm
Trace *gtrace;
/**
 * @brief implementation of OPT Paging algorithm
 **/
class OPTAlgorithm : public IAlgorithm {
public:
    //! Frame info for the OPT Algorithm (we dont use it)
    typedef int FrameInfo;
    //! State for Second Algorithm
    struct State {
        //! Record of different frames and indexes of their next reference
        //! We could be more runtime efficient by remembering all
        //! references we traverse over
        std::unordered_map<unsigned int, unsigned int> frame_indexes;
        //! Where in the trace list we are
        unsigned int current_index = 0;
    };
    /**
     * @brief performs OPT eviction on a page table
     * @param frame_map frames to select from when choosing which to evict
     * @param state 
     * @details finds the lowest stamped frame and evicts it
     **/
    static unsigned int nextEviction(PageTable<OPTAlgorithm>::FrameMap& frame_map, State& state) {
        unsigned int furthest_val = 0;
        unsigned int ret_val;
        for(auto it : frame_map) {
            if(state.frame_indexes[it.first] > furthest_val) {
                furthest_val = state.frame_indexes[it.first];
                ret_val = it.first;
            }
        }
        return ret_val;
    }
    /**
     * @brief inserts a new frame into the page table
     * @param frame the frame to be inserted
     * @param state state that can be maintained in the page table
     **/
    static void insert(PageTable<OPTAlgorithm>::Frame& frame, State& state) {
    }
    /**
     * @brief updates a frames stamp when it is accessed
     * @param frame the frame to update
     * @param state maintains the current stamp in the state
     **/
    static void update(PageTable<OPTAlgorithm>::Frame& frame, State& state) {
        // Whenever a frame is updated we find it's next occurance
        ++state.current_index;
        unsigned int i;
        for(i = state.current_index; i < gtrace->size(); ++i) {
            if((*gtrace)[i].address >> 12 == frame.frame_id) {
                break;
            }
        }
        state.frame_indexes[frame.frame_id] = i;
        
    };
}; /* class OPTAlgorithm */

/************************** Core Program            **************************/
//! Command line arguments
class Arguments {
public:
    /**
     * @brief inits the arguments from the arguments to main
     **/
    Arguments(int argc, char** argv):
        argc_(argc),
        argv_(argv)
    {}
    /**
     * @brief loads the arguments from CLI arguments
     * @return true if successfully loaded, false if not
     **/
    bool load() {
        bool success = true;
        if(!util::get_arg(argc_, argv_, "-n", num_frames)) {
            std::printf("ERROR: -n argument not provided.\n");
            success = false;
        }
        if(!util::get_arg(argc_, argv_, "-a", algorithm)) {
            std::printf("ERROR: -a argument not provided.\n");
            success = false;
        }
        // NOTE: Since I didn't really flesh out get_last_nth_arg to ignore
        //  flagged arguments this doesn't really work :P but I don't feel
        //  the need to fully flesh this deeply for error checking.
        //  If it's not provided the file won't load and will be caught later on
        if(!util::get_last_nth_arg(argc_, argv_, 0, trace_file)) {
            std::printf("ERROR: trace file not provided.\n");
            success = false;
        }
        return success;
    }

    //! Number of frames to use in the page table
    int num_frames;
    //! Algorithm to simulate
    std::string algorithm;
    //! Trace file to load
    std::string trace_file;

private:
    int argc_;
    char** argv_;
}; /* class Arguments */

template<typename table_type>
void runTests(table_type& table, Arguments args, std::string algorithm_string) {
    Trace trace(args.trace_file);
    gtrace = &trace;
    for(auto entry : trace) {
        table.execute(entry);
    }

    std::printf("Algorithm: %s\n", algorithm_string.c_str());
    std::printf("Number of frames: %d\n", args.num_frames);
    auto stats = table.getStats();
    std::printf("Total memory accesses: %d\n", stats.loads + stats.stores);
    std::printf("Total page faults: %d\n", stats.pf_evict_none + stats.pf_evict_clean + stats.pf_evict_dirty);
    std::printf("Total writes to disk: %d\n", stats.pf_evict_dirty);
}


int main(int argc, char** argv) {
    // First get all arguments
    // –n <numframes> -a <opt|lru|second> <tracefile>
    Arguments args(argc, argv);
    if(!args.load())
        return 1;

    if(args.algorithm == "lru") {
        PageTable<LRUAlgorithm> table(args.num_frames);
        runTests(table, args, "LRU");
    } else if(args.algorithm == "opt") {
        PageTable<OPTAlgorithm> table(args.num_frames);
        runTests(table, args, "OPT");
    } else if(args.algorithm == "second") {
        PageTable<SecondAlgorithm> table(args.num_frames);
        runTests(table, args, "SECOND");
    } else {
        std::printf("ERROR: Unknown algorithm.");
    }
    return 0;
}