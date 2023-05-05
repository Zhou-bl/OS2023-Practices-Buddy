#include "buddy.h"
#define NULL ((void *)0)

#define BUDDY_MAXRANK (16)
#define BUDDY_TESTSIZE (128)
#define BUDDY_MAXRANK0PAGE (BUDDY_TESTSIZE * 1024 / 4)
#define RANK0_PAGE_SIZE (1024 << 2) //一个页的大小

struct node{
    int is_allocated; //是否已经被分配出去了
    int longest_rank; //记录当前结点最多还能开多少rank的页, 当rank = 0时，说明该节点已经被分配出去了
    int index; //在数组中的位置
    void *p; //该节点的起始地址；
};

struct node buddy_tree[BUDDY_MAXRANK0PAGE << 2]; //二叉树管理buddy伙伴算法

int ls(int x){return x << 1;}
int rs(int x){return (x << 1) + 1;}
int parent(int x){
    if(x % 2) x--;
    return x / 2;
}

int is_power_of_2(int x){
    return (!(x & (x - 1))) ? 1 : 0;
}

int get_offset(int rank){
    return RANK0_PAGE_SIZE * (1 << (rank - 1));
}

int init_page(void *p, int pgcount){
    int cur_rank = BUDDY_MAXRANK + 1;
    for(int i = 1; i < (pgcount << 1); ++i){
        if(is_power_of_2(i)){
            cur_rank--;
        }
        buddy_tree[i].longest_rank = cur_rank;
        buddy_tree[i].index = i;
        buddy_tree[i].is_allocated = 0;
        if(is_power_of_2(i)){
            buddy_tree[i].p = p;
        } else {
            buddy_tree[i].p = buddy_tree[i - 1].p + get_offset(cur_rank);
        }
    }
    buddy_tree[1].is_allocated = 1;
    //check();
    return OK;
}

void check(){
    for(int i = 1; i < (BUDDY_MAXRANK0PAGE << 1); ++i){
        if(is_power_of_2(i)){
            printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        }
        printf("index: %d, is_allocated: %d, longest_rank: %d, p: %d\n", buddy_tree[i].index, buddy_tree[i].is_allocated, buddy_tree[i].longest_rank, buddy_tree[i].p);
    }
}

void *alloc_pages(int rank){
    //若传入的rank非法则返回-EINVAL
    if(rank < 1 || rank > BUDDY_MAXRANK){
        return (void *)-EINVAL;
    }
    //若没有足够的空间则返回-ENOSPC
    if(buddy_tree[1].longest_rank < rank){
        return (void *)-ENOSPC;
    }
    //若有足够的空间则返回分配的地址
    int index = 1;

    for(int node_rank = BUDDY_MAXRANK; node_rank != rank; node_rank--){
        //printf("index: %d\n", index);
        buddy_tree[index].is_allocated = 0;
        //将左儿子分配出来：
        buddy_tree[ls(index)].is_allocated = 1;
        //将右儿子分配出来：
        buddy_tree[rs(index)].is_allocated = 1;
        if(buddy_tree[ls(index)].longest_rank >= rank){
            index = ls(index);
        } else {
            index = rs(index);
        }
    }
    //printf("target index: %d\n", index);
/*
    while(buddy_tree[index].longest_rank > rank){
        //说明至少有一个子节点满足 longest_rank >= rank
        buddy_tree[index].is_allocated = 0;
        //将左儿子分配出来：
        buddy_tree[ls(index)].is_allocated = 1;
        //将右儿子分配出来：
        buddy_tree[rs(index)].is_allocated = 1;
        if(buddy_tree[ls(index)].longest_rank >= rank){
            index = ls(index);
        } else {
            index = rs(index);
        }
    }
*/    
    buddy_tree[index].longest_rank = 0;
    void *res = buddy_tree[index].p;
    int tmp = parent(index);
    while(index){
        index = parent(index);
        //更新父节点的longest_rank为两个儿子中较大的那个
        buddy_tree[index].longest_rank = 
        buddy_tree[ls(index)].longest_rank > buddy_tree[rs(index)].longest_rank ? 
        buddy_tree[ls(index)].longest_rank : buddy_tree[rs(index)].longest_rank;
    }
    index = tmp;
    //printf("%d\n", index);
    //printf("par: %d\n", buddy_tree[index].longest_rank);
    //printf("ls: %d\n", buddy_tree[ls(index)].longest_rank);
    //printf("rs: %d\n", buddy_tree[rs(index)].longest_rank);
    return res;
    return NULL;
}

int return_pages(void *p){
    if(p < buddy_tree[1].p || p > buddy_tree[1].p + get_offset(BUDDY_MAXRANK) - get_offset(1)){
        return -EINVAL;
    }
    //计算出该地址的页在数组中的下标：
    int index = BUDDY_MAXRANK0PAGE + (p - buddy_tree[1].p) / RANK0_PAGE_SIZE;
    //printf("return index: %d\n", index);
    if(buddy_tree[index].longest_rank != 0){
        //说明该地址并不是被分配出去的,则非法地址
        return -EINVAL;
    }
    //合法地址，进行释放：
    buddy_tree[index].longest_rank = 1;
    /*
    if(index == 32769){
        int tmp = parent(index);
        printf("parent_rank: %d\n", buddy_tree[tmp].longest_rank);
        printf("ls_rank: %d\n", buddy_tree[ls(tmp)].longest_rank);
        printf("rs_rank: %d\n", buddy_tree[rs(tmp)].longest_rank);
    }
    */
    while(index){
        index = parent(index);
        int ls_rank = buddy_tree[ls(index)].longest_rank;
        int rs_rank = buddy_tree[rs(index)].longest_rank;
        if(ls_rank == rs_rank && ls_rank == buddy_tree[index].longest_rank && 
        buddy_tree[ls(index)].is_allocated == 1 && buddy_tree[rs(index)].is_allocated == 1){
            //说明两个儿子都是空闲的，可以合并
            buddy_tree[index].longest_rank = ls_rank + 1;
            //将左右儿子被分配状态改变：
            buddy_tree[ls(index)].is_allocated = 0;
            buddy_tree[rs(index)].is_allocated = 0;
            buddy_tree[index].is_allocated = 1;
        } else {
            //说明两个儿子中至少有一个不是空闲的，不可以合并
            buddy_tree[index].longest_rank = ls_rank > rs_rank ? ls_rank : rs_rank;
        }
    }
    return OK;
}

int query_ranks(void *p){//查询一个页面的rank
    if(p < 0 || p > buddy_tree[1].p + get_offset(BUDDY_MAXRANK) - get_offset(1)){
        return -EINVAL;
    }
    //计算下标：
    int index = BUDDY_MAXRANK0PAGE + (p - buddy_tree[1].p) / RANK0_PAGE_SIZE;
    if(buddy_tree[index].longest_rank == 0){
        return 1;
    }
    //若未被分配：计算从p开始最多可以分配rank为多少的页面：
    while(index){
        int par = parent(index);
        if(buddy_tree[par].p != buddy_tree[index].p){
            //说明该节点是右儿子
            return buddy_tree[index].longest_rank;
        }
        index = par;
    }
    if(!index) index = 1;
    return buddy_tree[index].longest_rank;
}

/*
int recursive_query_page_counts(int cur_index, int rank){
    if(buddy_tree[cur_index].longest_rank < rank){
        return 0;
    }
    if(buddy_tree[cur_index].longest_rank == rank){
        return 1;
    }
    return recursive_query_page_counts(ls(cur_index), rank) + recursive_query_page_counts(rs(cur_index), rank);
}
*/

int query_page_counts(int rank){
    if(rank < 1 || rank > BUDDY_MAXRANK){
        return -EINVAL;
    }
    int res = 0;
    int index_rank = BUDDY_MAXRANK - rank;
    for(int i = (1 << index_rank); i < (1 << (index_rank + 1)); ++i){
        //只有被分出去且longest_rank == rank的才算：
        if(buddy_tree[i].is_allocated && buddy_tree[i].longest_rank == rank){
            //printf("i = %d\n", i);
            res += 1;
        }

        //res += (buddy_tree[i].is_allocated && buddy_tree[i].longest_rank == rank) ? 1 : 0;
    }
    return res;
}
