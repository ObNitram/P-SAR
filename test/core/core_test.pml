#ifndef N
#define N 3
#endif

#define NIL (N+1)

mtype = { ASK_LOCK, GET_LOCK, UNLOCK, WRITE, READ, NONE };
chan nodes[N] = [N] of { mtype, int, mtype }; // type, initiator, mode

inline add_reader(item){
    bool success = false;
    atomic{
        for (i : 0 .. (N-1)) {
            if 
            ::(read_request[current_list * N + i] == NIL) ->
                read_request[current_list * N + i] = item;
                read_request_size[current_list]++;
                success = true;
            :: else
            fi
        }
    }
    assert(success);
}

inline rem_reader(item){
    bool success = false;
    atomic {
        for (i : 0 .. (N-1)) {
            if 
            ::(read_request[current_list * N + i] == item) ->
                read_request[current_list * N + i] = NIL;
                read_request_size[current_list]--;
                success = true;
                break;
            fi
        }
    }
    assert(success);
}

inline add_writer(item){
    atomic {
        if
        :: (write_request[current_list] != NIL) ->
            assert(write_request[(current_list + 1) % 2] == NIL);
            write_request[(current_list + 1) % 2] = item
        :: else -> write_request[current_list] = item
        fi
    }
}

inline rem_writer(){
    atomic{
        write_request[current_list] = NIL;
        current_list = (current_list + 1) % 2
    }
}

inline ask_lock(id, m) {
    assert(m == WRITE || m == READ);

    mode = m;

    if
    :: (have_token == id) ->
        if
        :: (m == WRITE && read_request_size[current_list] > 0) -> add_writer(id) // il faut attendre 
        :: (m == READ) -> add_reader(id)
        :: else
        fi
    :: else -> 
        nodes[have_token] ! ASK_LOCK, id, m;
        do
        :: nodes[id] ? ASK_LOCK(req_i, req_m) -> handle_ask_lock(id, req_i, req_m);
        :: nodes[id] ? UNLOCK(req_i, req_m) -> handle_unlock(id, req_i, req_m);
        :: nodes[id] ? GET_LOCK(req_i, req_m) -> break;
        od;
        
        assert(req_m == m);
        if
        :: (m == WRITE) -> have_token = id
        :: (m == READ) -> have_token = req_i
        fi;
    fi
}

inline unlock(id, m) {
    assert(m == WRITE || m == READ);

    if
    :: (mode == WRITE) -> 
        if
        :: (read_request_size[current_list] > 0) -> 
            for (i : 0 .. (N-1)) {
                if 
                :: (read_request[i] != NIL) -> nodes[read_request[i]] ! GET_LOCK, id, READ;
                :: else
                fi
            }
        :: else ->
            if
            :: (write_request[current_list] != NIL) -> 
                if
                :: (write_request[current_list] != id) -> nodes[write_request[current_list]] ! GET_LOCK, id, WRITE;
                :: else
                fi;
                have_token = write_request[current_list];
                rem_writer()
            :: else
            fi
        fi
    :: (mode == READ) ->
        if
        :: (have_token == id) -> rem_reader(id)
        :: else -> nodes[have_token] ! UNLOCK, id, READ;
        fi
    fi;

    mode = NONE
}

inline handle_ask_lock(id, initiator, m) {
    assert(m == WRITE || m == READ);

    if
    :: write_request[current_list] != NIL -> 
        if
        :: (write_request[current_list] != id) -> nodes[write_request[current_list]] ! GET_LOCK, initiator, m;
        :: else ->
            if
            :: (m == WRITE) -> add_writer(initiator)
            :: (m == READ) -> add_reader(initiator)
            fi
        fi
    :: else ->
        if
        :: (have_token == id) ->
            if
            :: (mode == NONE) ->
                if
                :: (m == WRITE) ->
                    if
                    :: (read_request_size[current_list] == 0) ->
                        nodes[write_request[current_list]] ! GET_LOCK, id, WRITE;
                        have_token = initiator
                    :: else -> add_writer(initiator)
                    fi
                :: (m == READ) ->
                    nodes[initiator] ! GET_LOCK, id, READ;
                    add_reader(initiator)
                fi
            :: else -> 
                if
                :: (m == WRITE) -> add_writer(initiator)
                :: (m == READ) -> add_reader(initiator)
                fi
            fi
        :: else -> nodes[have_token] ! ASK_LOCK, initiator, m;
        fi
    fi
}

inline handle_unlock(id, initiator, m) {
    assert(m == READ);

    rem_reader(initiator)

    if
    :: (read_request_size[current_list] == 0 && write_request[current_list] != NIL) ->
        nodes[write_request[current_list]] ! GET_LOCK, id, WRITE;
        have_token = write_request[current_list];
        rem_writer()
    fi
}

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