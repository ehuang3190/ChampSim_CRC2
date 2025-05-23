////////////////////////////////////////////
//                                        //
//        LeCaR replacement policy        //
//                                        //
////////////////////////////////////////////

#include "../inc/champsim_crc2.h"
#include <deque>
#include <cmath>
#include <algorithm>


#define NUM_CORE 1
#define LLC_SETS NUM_CORE*2048
#define LLC_WAYS 16

float learning_rate = 0.45;
float prob = 0.5;//should be same as wlru

float wlru = 0.5;
float wsrrip = 0.5;

float d = pow(0.005, 1.0/(LLC_SETS*LLC_WAYS));

typedef struct _history_entry {
    uint64_t addr;
    bool type;
    uint64_t time;
} history_entry;

//history of evictions
std::deque<history_entry> history;

//generate a random number between 0 and 1
float rand_float()
{
    return (float)rand() / (float)RAND_MAX;
}




uint32_t lru[LLC_SETS][LLC_WAYS];

// initialize replacement state
void InitReplacementState_LRU()
{
    cout << "Initialize LRU replacement state" << endl;

    for (int i=0; i<LLC_SETS; i++) {
        for (int j=0; j<LLC_WAYS; j++) {
            lru[i][j] = j;
        }
    }
}

// find replacement victim
// return value should be 0 ~ 15 or 16 (bypass)
uint32_t GetVictimInSet_LRU (uint32_t cpu, uint32_t set, const BLOCK *current_set, uint64_t PC, uint64_t paddr, uint32_t type)
{
    for (int i=0; i<LLC_WAYS; i++)
        if (lru[set][i] == (LLC_WAYS-1))
            return i;

    return 0;
}

// called on every cache hit and cache fill
void UpdateReplacementState_LRU (uint32_t cpu, uint32_t set, uint32_t way, uint64_t paddr, uint64_t PC, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    // update lru replacement state
    for (uint32_t i=0; i<LLC_WAYS; i++) {
        if (lru[set][i] < lru[set][way]) {
            lru[set][i]++;

            if (lru[set][i] == LLC_WAYS)
                assert(0);
        }
    }
    lru[set][way] = 0; // promote to the MRU position
}


#define maxRRPV 3
uint32_t rrpv[LLC_SETS][LLC_WAYS];

// initialize replacement state
void InitReplacementState_SRRIP()
{
    cout << "Initialize SRRIP state" << endl;

    for (int i=0; i<LLC_SETS; i++) {
        for (int j=0; j<LLC_WAYS; j++) {
            rrpv[i][j] = maxRRPV;
        }
    }
}

// find replacement victim
// return value should be 0 ~ 15 or 16 (bypass)
uint32_t GetVictimInSet_SRRIP (uint32_t cpu, uint32_t set, const BLOCK *current_set, uint64_t PC, uint64_t paddr, uint32_t type)
{
    // look for the maxRRPV line
    while (1)
    {
        for (int i=0; i<LLC_WAYS; i++)
            if (rrpv[set][i] == maxRRPV)
                return i;

        for (int i=0; i<LLC_WAYS; i++)
            rrpv[set][i]++;
    }

    // WE SHOULD NOT REACH HERE
    assert(0);
    return 0;
}

// called on every cache hit and cache fill
void UpdateReplacementState_SRRIP (uint32_t cpu, uint32_t set, uint32_t way, uint64_t paddr, uint64_t PC, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    if (hit)
        rrpv[set][way] = 0;
    else
        rrpv[set][way] = maxRRPV-1;
}


// initialize replacement state
void InitReplacementState()
{
    //initialize LRU and SRRIP replacement states
    InitReplacementState_LRU();
    InitReplacementState_SRRIP();
}

// find replacement victim
uint32_t GetVictimInSet (uint32_t cpu, uint32_t set, const BLOCK *current_set, uint64_t PC, uint64_t paddr, uint32_t type)
{
    if (rand_float() < prob) { //LRU selected
        //empty history queue if maxed out
        if (history.size() == LLC_SETS * LLC_WAYS) {
            history.pop_front();
        }
        //add new entry to history queue
        history_entry entry;
        entry.addr = paddr;
        entry.type = 0;
        entry.time = get_cycle_count();
        history.push_back(entry);
        //identify victim using LRU
        return GetVictimInSet_LRU(cpu, set, current_set, PC, paddr, type);
    } else { //SRRIP selected
        //empty history queue if maxed out
        if (history.size() == LLC_SETS * LLC_WAYS) {
            history.pop_front();
        }
        //add new entry to history queue
        history_entry entry;
        entry.addr = paddr;
        entry.type = 1;
        entry.time = get_cycle_count();
        history.push_back(entry);
        //identify victim using SRRIP
        return GetVictimInSet_SRRIP(cpu, set, current_set, PC, paddr, type);
    }
}

// called on every cache hit and cache fill
void UpdateReplacementState (uint32_t cpu, uint32_t set, uint32_t way, uint64_t paddr, uint64_t PC, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    if (hit == false){
        //see if same address is in history
        auto it = find_if(history.begin(), history.end(), [paddr](const history_entry& entry) {
            return entry.addr == paddr;
        });

        if (it != history.end()) {
            if (it->type == 0) {
                //history entry was LRU so we should increase SRRIP weight
                wsrrip = wsrrip * exp(-learning_rate * pow(d, (get_cycle_count() - it->time)));
            } else if (it->type == 1) {
                //history entry was SRRIP so we should increase LRU weight
                wlru = wlru * exp(-learning_rate * pow(d, (get_cycle_count() - it->time)));
            } else {
                //history entry was not found
                assert(0);
            }
            //normalize weights
            wlru = wlru / (wlru + wsrrip);
            wsrrip = wsrrip / (wlru + wsrrip);
            prob = wlru;  
            //remove the entry from history
            if (history.size() == LLC_SETS * LLC_WAYS) {
                history.erase(it);
            }
        }
    }
    UpdateReplacementState_LRU(cpu, set, way, paddr, PC, victim_addr, type, hit);
    UpdateReplacementState_SRRIP(cpu, set, way, paddr, PC, victim_addr, type, hit);
}

// use this function to print out your own stats on every heartbeat 
void PrintStats_Heartbeat()
{

}

// use this function to print out your own stats at the end of simulation
void PrintStats()
{

}
