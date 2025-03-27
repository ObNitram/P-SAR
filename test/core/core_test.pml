#ifndef N
#define N 3
#endif

#define NIL (N+1)

mtype = { ASK_LOCK, GET_LOCK, UNLOCK, WRITE, READ, NONE };
chan node[N] = [N] of { mtype, int, mtype }; // type, initiator, mode

mtype mode[N];
int have_token[N];
int read_request[N * N];
int write_request[N];

inline ask_lock(id, m) {
    assert(m == WRITE || m == READ);

    int req_i;
    mtype req_m;

    mode[id] = m;
    node[have_token[id]] ! ASK_LOCK, id, m;
    node[have_token[id]] ? GET_LOCK(req_i, req_m);

    assert(req_m == m);

    if
    :: (m == WRITE) -> have_token[id] = id
    :: (m == READ) -> have_token[id] = req_i
    fi;
}

inline unlock(int id, mtype m) {
    if
    :: (mode == WRITE) -> 
        
    :: (mode == READ) -> 
    fi
}

// inline handle_ask_lock(int id, int initiator, mtype m) {

// }

// inline handle_unlock(int id, int initiator, mtype m) {

// }

proctype client(int id){
    printf("hello from client %d\n",_pid);
    ask_lock(id, READ);
}

proctype handler(int id){
    printf("hello from handler %d\n",_pid);
}

init{
    int i = 0;
    int j;
    do
    ::(i < N)->
        mode[i] = NONE;
        have_token[i] = 0;
        write_request[i] = NIL;
        j = 0;
        do
        :: (j < N) ->
            read_request[(i) * N + (j)] = NIL;
            j++
        od;
        i++
    od;
    i = 0;
    atomic{
        do
        :: (i < N) ->
            run client(i);
            run handler(i);
            i++
        od;
    }
}