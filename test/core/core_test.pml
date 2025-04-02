#ifndef N
#define N 3
#endif

#define NIL (N+1)

mtype = { ASK_LOCK, GET_LOCK, UNLOCK, WRITE, READ, NONE };
chan nodes[N] = [N] of { mtype, int, mtype }; // type, initiator, mode

proctype node(int id){
    printf("hello from client %d\n",id);
    mtype mode = NONE;
    int have_token = 0;
    int read_request[2 * N];
    int read_request_size[2];
    int write_request[2];
    int current_list = 0;

    int i = 0;
    int req_i;
    mtype req_m;

    int count = 0

    for(i : 0 .. 1){
        read_request_size[i] = 0;
        write_request[i] = NIL;
    }
    for(i : 0 .. (N-1)*2){
        read_request[i] = NIL;
    }

    do
    :: (count < 10) ->
        //on envoie une action ou pas
        if
        ::(mode == NONE) ->
            printf("%d test lock write\n", id);
            ask_lock(id, WRITE);
            count++
        ::(mode == NONE) -> 
            printf("%d test lock read\n", id);
            ask_lock(id, READ);
            count++
        ::(mode != NONE) -> 
            printf("%d test unlock\n", id);
            unlock(id, mode);
        fi;

        //on traite les messages
        do
        :: nodes[id] ? ASK_LOCK(req_i, req_m) -> handle_ask_lock(id, req_i, req_m);
        :: nodes[id] ? UNLOCK(req_i, req_m) -> handle_unlock(id, req_i, req_m);
        :: empty(nodes[id]) -> break;
        od;
    :: else -> break;
    od
}

init{
    int i = 0;
    atomic{
        for(i : 0 .. (N-1)){
            run node(i);
        }
    }
}