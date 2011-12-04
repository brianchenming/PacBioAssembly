/*
 * ===========================================================================
 *
 *       Filename:  seq_aligner.h
 *         Author:  Ming Chen, brianchenming@gmail.com
 *        Created:  11/10/2011 11:03:08 PM
 *
 *    Description:  perform hurestic local alignment
 *
 *       Revision:  none
 *
 *
 * ===========================================================================
 */

#ifndef SEQ_ALIGNER_H
#define SEQ_ALIGNER_H

#include	<string.h>
#include	<math.h>
#include	<limits.h>
#include	<vector>
#include	<algorithm>
#include	"dna_seq.h"
#include	"common.h"

// ratio is the maximum bias between segment and reference
//#define DEBUG_ALIGNER
#define MAXR 0.3
#define MAXN 20000
#define MAXM (int)(MAXN*MAXR)

enum OP {
    MATCH = 1,
    INSERT,
    DELETE
};

typedef struct {
    int cost;        
    int parent;      
} state;

typedef struct {
    short op;
    char val;
} edit;

class seq_aligner {
public:
    seq_aligner() : R(MAXR) {};
    seq_aligner(double r) : R(r) {};
    double R;                   // ratio of difference allowed
    int len_a;                  // max possible length of match in seg_a
    int len_b;                  // max possible length of match in seg_b
    int max_dst;                // max distance allowed
    int matlen_a;               // length of match in seg_a
    int matlen_b;               // length of match in seg_b
    state mat[MAXN][MAXM];      // DP matrix
    int nedit;                  // number of edits
    edit edits[MAXN + MAXM];// edits of transform seg_a to seg_b[beg:end]
    int align(seq_accessor *seg_a, seq_accessor *seg_b) {
        // work out parameters
        if (seg_b->length() >= seg_a->length()) { 
            len_a = seg_a->length();
            max_dst = 1 + (int)(len_a * R);
            len_b = std::min(seg_b->length(), len_a + max_dst);
        } else {
            len_b = seg_b->length();
            max_dst = 1 + (int)(len_b * R);
            len_a = std::min(seg_a->length(), len_b + max_dst);
        }

        if (len_a >= MAXN || max_dst >= MAXM) {
            LOG("segment too long: %d\n", len_a);
            return -1;
        }

        init_cell();

        if (!search(seg_a, seg_b)) return -1;

        goal_cell();
        if (matlen_b < len_b*(1-R)) return -1;
        nedit = 0;
        find_path(matlen_a, matlen_b, seg_b);

#ifdef DEBUG_ALIGNER
        print_matrix(seg_a, seg_b);
        printf("(%d, %d)\n", matlen_a, matlen_b);
        printf("len_a: %d, len_b: %d\n", len_a, len_b);
#endif

        return matlen_b;
    };
    int get_cost(int i, int j) { return mat[i][j-i+max_dst].cost; };
    void set_cost(int i, int j, int v) { mat[i][j-i+max_dst].cost = v; };
    int get_parent(int i, int j) { return mat[i][j-i+max_dst].parent; }
    void set_parent(int i, int j, int p) { mat[i][j-i+max_dst].parent = p;}
    int final_cost() { return get_cost(matlen_a, matlen_b); }
private:
    int match(char c, char d) { return c != d; };
    int indel(char c) { return 1; };
    // translate index into rectangle matrix to index into diagonal stripe
    void init_cell() {
//        for (int i=0; i<=len_a; ++i) {
//            int beg = std::max(0, i - max_dst);
//            int end = std::min(len_b, i + max_dst);
//            for (int j=beg; j<=end; ++j) {
//                set_cost(i, j, std::max(i, j));
//                set_parent(i, j, i>j ? DELETE : (i==j ? MATCH : INSERT));
//            } 
//        }
        for (int i = 1; i <= max_dst; ++i) {
            set_cost(i, 0, i);
            set_parent(i, 0, DELETE);
        }
        for (int j = 1; j <= max_dst; ++j) {
            set_cost(0, j, j);
            set_parent(0, j, INSERT);
        }
        set_cost(0, 0, 0);
        set_parent(0, 0, 0);
    };
    bool search(seq_accessor *seg_a, seq_accessor *seg_b) {
        seg_a->reset(0);     // start from the first
        for (int i=1; i<=len_a; ++i) {
            int best_cost = len_a;
            char c = seg_a->next();
            int beg = std::max(1, i - max_dst);
            int end = std::min(len_b, i + max_dst);
            seg_b->reset(beg-1);     // start from the beg-th element
            for (int j=beg; j<=end; ++j) {
                char d = seg_b->next();
                int t; 
//                int cost = get_cost(i, j);
                int cost = MAX_SEQ_LEN;
                if ((t = get_cost(i-1, j-1) + match(c, d)) < cost) {
                    set_cost(i, j, t);
                    set_parent(i, j, MATCH);
                    cost = t;
                }
                if (i-j<max_dst && (t=get_cost(i,j-1)+indel(c)) < cost) {
                    set_cost(i, j, t);
                    set_parent(i, j, INSERT);
                    cost = t;
                }
                if (j-i<max_dst && (t=get_cost(i-1,j)+indel(d)) < cost) {
                    set_cost(i, j, t);
                    set_parent(i, j, DELETE);
                    cost = t;
                }
                best_cost = std::min(cost, best_cost);
            }
#ifdef DEBUG_ALIGNER
            LOG("i = %d, best_cost = %d\n", i, best_cost);
#endif
            // early failure 
            if (i > 10 && get_cost(i, i) > i*R) {
//                printf("failed at %d\n", i);
                return false;
            }
        }
        return true;
    };
    void goal_cell() {
        matlen_a = len_a;
        matlen_b = len_b;
        while (matlen_a > len_b && get_cost(matlen_a-1, matlen_b) 
                <= get_cost(matlen_a, matlen_b))
            --matlen_a;
        while (matlen_b > len_a && get_cost(matlen_a, matlen_b-1) 
                <= get_cost(matlen_a, matlen_b))
            --matlen_b;
    };
    void find_path(int i, int j, seq_accessor *seg_b) {
        int p = get_parent(i, j);
        if (p == MATCH) {
            find_path(i-1, j-1, seg_b);
            edits[nedit].op = MATCH;
            edits[nedit].val = seg_b->at(j-1);
            ++nedit;
        }
        if (p == INSERT) {
            find_path(i, j-1, seg_b);
            edits[nedit].op = INSERT;
            edits[nedit].val = seg_b->at(j-1);
            ++nedit;
        } 
        if (p == DELETE) {
            find_path(i-1, j, seg_b);
            edits[nedit].op = DELETE;
            ++nedit;
        }
    };
    void print_matrix(seq_accessor *seg_a, seq_accessor *seg_b) {
        printf(" \t \t");
        for (int j = 1; j <= len_b; ++j) {
            printf("%c\t", seg_b->at(j-1));
        }
        printf("\n");
        for (int i = 0; i <= len_a; ++i) {
            printf("%c\t", i == 0 ? ' ' : seg_a->at(i-1));
            for (int j = 0; j <= len_b; ++j) {
                if (j < i-max_dst || j > i+max_dst) 
                    printf("%d\t", -1);
                else
                    printf("%d\t", get_cost(i, j));
            }
            printf("\n");
        }
    }
};

#endif
