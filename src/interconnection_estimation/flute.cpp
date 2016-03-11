//
// Created by renan on 3/10/16.
//

#include "flute.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#if FLUTE_D<=7
#define MGROUP 5040/4  // Max. # of groups, 7! = 5040
#define MPOWV 15  // Max. # of POWVs per group
#elif FLUTE_D==8
#define MGROUP 40320/4  // Max. # of groups, 8! = 40320
#define MPOWV 33  // Max. # of POWVs per group
#elif FLUTE_D==9
#define MGROUP 362880/4  // Max. # of groups, 9! = 362880
#define MPOWV 79  // Max. # of POWVs per group
#endif

#include <algorithm>
using std::max;
using std::min;
using std::pair;
using std::make_pair;

namespace openeda {
    namespace interconnection_estimation {
        int numgrp[10]={0,0,0,0,6,30,180,1260,10080,90720};

        typedef struct point {
            DTYPE x, y;
            int o;
        } POINT;

        typedef POINT* POINTptr;

        class compare_xcoord
        {
        public:
            bool operator()(const POINT &pt_a , const POINT &pt_b) const
            {
                return (pt_a.x < pt_b.x);
            }
        };

        class compare_ycoord
        {
        public:
            bool operator()(const POINT &pt_a , const POINT &pt_b) const
            {
                return (pt_a.y < pt_b.y);
            }
        };

        struct csoln
        {
            unsigned char parent;
            unsigned char seg[12];  // Add: 0..i, Sub: j..11; seg[i+1]=seg[j-1]=0
            unsigned char row[FLUTE_D-2], col[FLUTE_D-2];
            unsigned char neighbor[2*FLUTE_D-2];
        };
        struct csoln *LUT[FLUTE_D+1][MGROUP];  // storing 4 .. FLUTE_D
        int numsoln[FLUTE_D+1][MGROUP];

        void readLUT();
        DTYPE flute_wl(int d, DTYPE x[], DTYPE y[], int acc);
        DTYPE flutes_wl_LD(int d, DTYPE xs[], DTYPE ys[], int s[]);
        DTYPE flutes_wl_MD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
        DTYPE flutes_wl_RDP(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
        tree flute(int d, DTYPE x[], DTYPE y[], int acc);
        tree flutes_LD(int d, DTYPE xs[], DTYPE ys[], int s[]);
        tree flutes_MD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
        tree flutes_RDP(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
        tree dmergetree(tree t1, tree t2);
        tree hmergetree(tree t1, tree t2, int s[]);
        tree vmergetree(tree t1, tree t2);
        DTYPE wirelength(tree t);
        void printtree(tree t);

        void readLUT()
        {
            static bool LUTread = false;
            if(LUTread) return;
            LUTread = true;

            FILE *fpwv, *fprt;
            struct csoln *p;
            int d, i, j, k, kk, ns, nn, ne;
            unsigned char line[99], *linep, c;
            unsigned char charnum[256];
            unsigned char divd[256], modd[256], div16[256], mod16[256], dsq;

            for (i=0; i<=255; i++) {
                if ('0'<=i && i<='9')
                    charnum[i] = i - '0';
                else if (i>='A')
                    charnum[i] = i - 'A' + 10;
                else // if (i=='$' || i=='\n' || ... )
                    charnum[i] = 0;

                div16[i] = i/16;
                mod16[i] = i%16;
            }

            fpwv=fopen(POWVFILE, "r");
            if (fpwv == NULL) {
                printf("Error in opening %s\n", POWVFILE);
                printf("Please make sure %s and %s\n"
                               "are in the current working directory.\n", POWVFILE, PORTFILE);
                exit(1);
            }

#if FLUTEROUTING==1
            fprt=fopen(PORTFILE, "r");
            if (fprt == NULL) {
                printf("Error in opening %s\n", PORTFILE);
                printf("Please make sure %s and %s\n(found in the Flute directory of UMpack)\n"
                               "are in the current working directory.\n", POWVFILE, PORTFILE);
                exit(1);
            }
#endif

            for (d=4; d<=FLUTE_D; d++) {
                for (i=0; i<=255; i++) {
                    divd[i] = i/d;
                    modd[i] = i%d;
                }
                dsq=d*d;

                fscanf(fpwv, "d=%d\n", &d);
#if FLUTEROUTING==1
                fscanf(fprt, "d=%d\n", &d);
#endif
                for (k=0; k<numgrp[d]; k++) {
                    ns = (int) charnum[fgetc(fpwv)];

                    if (ns==0) {  // same as some previous group
                        fscanf(fpwv, "%d\n", &kk);
                        numsoln[d][k] = numsoln[d][kk];
                        LUT[d][k] = LUT[d][kk];
                    }
                    else {
                        fgetc(fpwv);  // '\n'
                        numsoln[d][k] = ns;
                        p = (struct csoln*) malloc(ns*sizeof(struct csoln));
                        LUT[d][k] = p;
                        for (i=1; i<=ns; i++) {
                            linep = (unsigned char*)fgets((char*)line, 99, fpwv);
                            p->parent = charnum[*(linep++)];
                            j = 0;
                            while ((p->seg[j++] = charnum[*(linep++)]) != 0) ;
                            j = 10;
                            while ((p->seg[j--] = charnum[*(linep++)]) != 0) ;
#if FLUTEROUTING==1
                            nn = 2*d-2;
                            ne = 2*d-3;
                            fread(line, 1, d-2, fprt); linep=line;
                            for (j=d; j<nn; j++) {
                                c = *(linep++);
                                if (c >= dsq) {
                                    c -= dsq;
                                    p->neighbor[divd[c]] = j;
                                    ne--;
                                }
                                p->row[j-d] = divd[c];
                                p->col[j-d] = modd[c];
                                p->neighbor[j] = j;  // initialized
                            }
                            fread(line, 1, ne+1, fprt); linep=line;  // last char = \n
                            for (j=0; j<ne; j++) {
                                c = *(linep++);
                                p->neighbor[div16[c]] = mod16[c];
                            }
#endif
                            p++;
                        }
                    }
                }
            }
        }

        DTYPE flute_wl(int d, DTYPE x[], DTYPE y[], int acc)
        {
            unsigned allocateSize = MAXD;
            if (d > MAXD)
                allocateSize = d+1;
            DTYPE*  xs  = (DTYPE*)  malloc(sizeof(DTYPE)*allocateSize);
            DTYPE*  ys  = (DTYPE*)  malloc(sizeof(DTYPE)*allocateSize);
            int*     s  = (int*)    malloc(sizeof(int)*allocateSize);
            POINT*  pt  = (POINT*)  malloc(sizeof(POINT)*allocateSize);
            /* replaced the selection sort with stl stable_sort. mckim
          POINTptr *ptp = (POINTptr*) malloc(sizeof(POINTptr)*allocateSize); */
            POINT* tmpp;
            DTYPE minval, l, xu, xl, yu, yl;
            int i, j, minidx;
//  DTYPE xs[MAXD], ys[MAXD];
//  int s[MAXD];
//  struct point {
//    DTYPE x, y;
//    int o;
//  } pt[MAXD], *ptp[MAXD], *tmpp;

            if (d==2)
                l = ADIFF(x[0], x[1]) + ADIFF(y[0], y[1]);
            else if (d==3) {
                if (x[0] > x[1]) {
                    xu = max(x[0], x[2]);
                    xl = min(x[1], x[2]);
                }
                else {
                    xu = max(x[1], x[2]);
                    xl = min(x[0], x[2]);
                }
                if (y[0] > y[1]) {
                    yu = max(y[0], y[2]);
                    yl = min(y[1], y[2]);
                }
                else {
                    yu = max(y[1], y[2]);
                    yl = min(y[0], y[2]);
                }
                l = (xu-xl) + (yu-yl);
            }
            else {
                for (i=0; i<d; i++) {
                    pt[i].x = x[i];
                    pt[i].y = y[i];
                    /* replaced the selection sort with stl stable_sort. mckim
              ptp[i] = &pt[i]; */
                }

                // sort x
                /* replaced the selection sort with stl stable_sort. mckim
            for (i=0; i<d-1; i++) {
              minval = ptp[i]->x;
              minidx = i;
              for (j=i+1; j<d; j++) {
                if (minval > ptp[j]->x) {
                  minval = ptp[j]->x;
                  minidx = j;
                }
              }
              tmpp = ptp[i];
              ptp[i] = ptp[minidx];
              ptp[minidx] = tmpp;
            } */
                std::stable_sort(&pt[0], &pt[d], compare_xcoord());

#if REMOVE_DUPLICATE_PIN==1
                ptp[d] = &pt[d];
    ptp[d]->x = ptp[d]->y = -999999;
    j = 0;
    for (i=0; i<d; i++) {
      for (k=i+1; ptp[k]->x == ptp[i]->x; k++)
        if (ptp[k]->y == ptp[i]->y)  // pins k and i are the same
          break;
      if (ptp[k]->x != ptp[i]->x)
        ptp[j++] = ptp[i];
    }
    d = j;
#endif

                for (i=0; i<d; i++) {
                    xs[i] = pt[i].x;//ptp[i]->x;
                    pt[i].o = i;
                    /* replaced the selection sort with stl stable_sort. mckim
              ptp[i]->o = i; */
                }

                // sort y to find s[]
                /* replaced the selection sort with stl stable_sort. mckim
            for (i=0; i<d-1; i++) {
              minval = ptp[i]->y;
              minidx = i;
              for (j=i+1; j<d; j++) {
                if (minval > ptp[j]->y) {
                  minval = ptp[j]->y;
                  minidx = j;
                }
              }
              ys[i] = ptp[minidx]->y;
              s[i] = ptp[minidx]->o;
              ptp[minidx] = ptp[i];
            }
            ys[d-1] = ptp[d-1]->y;
            s[d-1] = ptp[d-1]->o;
                */
                std::stable_sort(&pt[0], &pt[d], compare_ycoord());
                for (i=0; i<d; i++) {
                    ys[i] = pt[i].y;
                    s[i] = pt[i].o;
                }

                l = flutes_wl(d, xs, ys, s, acc);
            }
            free(xs);
            free(ys);
            free(s);
            free(pt);
//  free(ptp);
            return l;
        }

// xs[] and ys[] are coords in x and y in sorted order
// s[] is a list of nodes in increasing y direction
//   if nodes are indexed in the order of increasing x coord
//   i.e., s[i] = s_i in defined in paper
// The points are (xs[s[i]], ys[i]) for i=0..d-1
//             or (xs[i], ys[si[i]]) for i=0..d-1

        DTYPE flutes_wl_RDP(int d, DTYPE xs[], DTYPE ys[], int s[], int acc)
        {
            int i, j, ss;

            for (i=0; i<d-1; i++) {
                if (xs[s[i]]==xs[s[i+1]] && ys[i]==ys[i+1]) {
                    if (s[i] < s[i+1])
                        ss = s[i+1];
                    else {
                        ss = s[i];
                        s[i] = s[i+1];
                    }
                    for (j=i+2; j<d; j++) {
                        ys[j-1] = ys[j];
                        s[j-1] = s[j];
                    }
                    for (j=ss+1; j<d; j++)
                        xs[j-1] = xs[j];
                    for (j=0; j<=d-2; j++)
                        if (s[j] > ss) s[j]--;
                    i--;
                    d--;
                }
            }
            return flutes_wl_ALLD(d, xs, ys, s, acc);
        }

// For low-degree, i.e., 2 <= d <= FLUTE_D
        DTYPE flutes_wl_LD(int d, DTYPE xs[], DTYPE ys[], int s[])
        {
            int k, pi, i, j;
            struct csoln *rlist;
            DTYPE dd[2*FLUTE_D-2];  // 0..FLUTE_D-2 for v, FLUTE_D-1..2*FLUTE_D-3 for h
            DTYPE minl, sum, l[MPOWV+1];

            if (d <= 3)
                minl = xs[d-1]-xs[0]+ys[d-1]-ys[0];
            else {
                k = 0;
                if (s[0] < s[2]) k++;
                if (s[1] < s[2]) k++;

                for (i=3; i<=d-1; i++) {  // p0=0 always, skip i=1 for symmetry
                    pi = s[i];
                    for (j=d-1; j>i; j--)
                        if (s[j] < s[i])
                            pi--;
                    k = pi + (i+1)*k;
                }

                if (k < numgrp[d])  // no horizontal flip
                    for (i=1; i<=d-3; i++) {
                        dd[i]=ys[i+1]-ys[i];
                        dd[d-1+i]=xs[i+1]-xs[i];
                    }
                else {
                    k=2*numgrp[d]-1-k;
                    for (i=1; i<=d-3; i++) {
                        dd[i]=ys[i+1]-ys[i];
                        dd[d-1+i]=xs[d-1-i]-xs[d-2-i];
                    }
                }

                minl = l[0] = xs[d-1]-xs[0]+ys[d-1]-ys[0];
                rlist = LUT[d][k];
                for (i=0; rlist->seg[i]>0; i++)
                    minl += dd[rlist->seg[i]];

                l[1] = minl;
                j = 2;
                while (j <= numsoln[d][k]) {
                    rlist++;
                    sum = l[rlist->parent];
                    for (i=0; rlist->seg[i]>0; i++)
                        sum += dd[rlist->seg[i]];
                    for (i=10; rlist->seg[i]>0; i--)
                        sum -= dd[rlist->seg[i]];
                    minl = min(minl, sum);
                    l[j++] = sum;
                }
            }

            return minl;
        }

// For medium-degree, i.e., FLUTE_D+1 <= d
        DTYPE flutes_wl_MD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc)
        {
            unsigned allocateSize = MAXD;
            if (d > MAXD)
                allocateSize = d+1;
            DTYPE *x1      = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            DTYPE *x2      = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            DTYPE *y1      = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            DTYPE *y2      = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            int   *si      = (int*)   malloc(sizeof(int)*allocateSize);
            int   *s1      = (int*)   malloc(sizeof(int)*allocateSize);
            int   *s2      = (int*)   malloc(sizeof(int)*allocateSize);
            float *score   = (float*) malloc(sizeof(float)*2*allocateSize);
            float *penalty = (float*) malloc(sizeof(float)*allocateSize);
            DTYPE *distx   = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            DTYPE *disty   = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);

//  DTYPE x1[MAXD], x2[MAXD], y1[MAXD], y2[MAXD];
//  int si[MAXD], s1[MAXD], s2[MAXD];
//  float score[2*MAXD], penalty[MAXD];
//  DTYPE distx[MAXD], disty[MAXD];
            float pnlty, dx, dy;
            DTYPE ll, minl, extral;
            int i, r, p, maxbp, nbp, bp, ub, lb, n1, n2, ms, newacc;
            int mins, maxs, minsi, maxsi;
            DTYPE xydiff;
            DTYPE retVal;

            if (s[0] < s[d-1]) {
                ms = max(s[0], s[1]);
                for (i=2; i<=ms; i++)
                    ms = max(ms, s[i]);
                if (ms <= d-3) {
                    for (i=0; i<=ms; i++) {
                        x1[i] = xs[i];
                        y1[i] = ys[i];
                        s1[i] = s[i];
                    }
                    x1[ms+1] = xs[ms];
                    y1[ms+1] = ys[ms];
                    s1[ms+1] = ms+1;

                    s2[0] = 0;
                    for (i=1; i<=d-1-ms; i++)
                        s2[i] = s[i+ms]-ms;

                    retVal = 0;
                    retVal += flutes_wl_LMD(ms+2, x1, y1, s1, acc);
                    retVal += flutes_wl_LMD(d-ms, xs+ms, ys+ms, s2, acc);
                    free(x1);
                    free(x2);
                    free(y1);
                    free(y2);
                    free(si);
                    free(s1);
                    free(s2);
                    free(score);
                    free(penalty);
                    free(distx);
                    free(disty);
                    return retVal;
//      return flutes_wl_LMD(ms+2, x1, y1, s1, acc)
//        + flutes_wl_LMD(d-ms, xs+ms, ys+ms, s2, acc);
                }
            }
            else {  // (s[0] > s[d-1])
                ms = min(s[0], s[1]);
                for (i=2; i<=d-1-ms; i++)
                    ms = min(ms, s[i]);
                if (ms >= 2) {
                    x1[0] = xs[ms];
                    y1[0] = ys[0];
                    s1[0] = s[0]-ms+1;
                    for (i=1; i<=d-1-ms; i++) {
                        x1[i] = xs[i+ms-1];
                        y1[i] = ys[i];
                        s1[i] = s[i]-ms+1;
                    }
                    x1[d-ms] = xs[d-1];
                    y1[d-ms] = ys[d-1-ms];
                    s1[d-ms] = 0;

                    s2[0] = ms;
                    for (i=1; i<=ms; i++)
                        s2[i] = s[i+d-1-ms];

                    retVal = 0;
                    retVal += flutes_wl_LMD(d+1-ms, x1, y1, s1, acc);
                    retVal += flutes_wl_LMD(ms+1, xs, ys+d-1-ms, s2, acc);
                    free(x1);
                    free(x2);
                    free(y1);
                    free(y2);
                    free(si);
                    free(s1);
                    free(s2);
                    free(score);
                    free(penalty);
                    free(distx);
                    free(disty);
                    return retVal;

//      return flutes_wl_LMD(d+1-ms, x1, y1, s1, acc)
//        + flutes_wl_LMD(ms+1, xs, ys+d-1-ms, s2, acc);
                }
            }

            // Find inverse si[] of s[]
            for (r=0; r<d; r++)
                si[s[r]] = r;

            // Determine breaking directions and positions dp[]
            lb=2;
            ub=d-1-lb;

            // Compute scores
#define AA 0.6
#define BB 0.3
#define CC 0.32
#define DD 4.8
            float DDD = DD/(d-1);

            // Compute penalty[]
            dx = CC*(xs[d-2]-xs[1])/(d-3);
            dy = CC*(ys[d-2]-ys[1])/(d-3);
            for (r = d/2, pnlty = 0; r>=0; r--, pnlty += dx)
                penalty[r] = pnlty,  penalty[d-1-r] = pnlty;
            for (r = d/2-1, pnlty = dy; r>=0; r--, pnlty += dy)
                penalty[s[r]] += pnlty,  penalty[s[d-1-r]] += pnlty;
            //#define CC 0.16
            //    for (r=0; r<d; r++)
            //        penalty[r] = abs(d-1-r-r)*dx + abs(d-1-si[r]-si[r])*dy;

            // Compute distx[], disty[]
            xydiff = (xs[d-1] - xs[0]) - (ys[d-1] - ys[0]);
            if (s[0] < s[1])
                mins = s[0], maxs = s[1];
            else mins = s[1], maxs = s[0];
            if (si[0] < si[1])
                minsi = si[0], maxsi = si[1];
            else minsi = si[1], maxsi = si[0];
            for (r=2; r<=ub; r++) {
                if (s[r] < mins)
                    mins = s[r];
                else if (s[r] > maxs)
                    maxs = s[r];
                distx[r] = xs[maxs] - xs[mins];
                if (si[r] < minsi)
                    minsi = si[r];
                else if (si[r] > maxsi)
                    maxsi = si[r];
                disty[r] = ys[maxsi] - ys[minsi] + xydiff;
            }

            if (s[d-2] < s[d-1])
                mins = s[d-2], maxs = s[d-1];
            else mins = s[d-1], maxs = s[d-2];
            if (si[d-2] < si[d-1])
                minsi = si[d-2], maxsi = si[d-1];
            else minsi = si[d-1], maxsi = si[d-2];
            for (r=d-3; r>=lb; r--) {
                if (s[r] < mins)
                    mins = s[r];
                else if (s[r] > maxs)
                    maxs = s[r];
                distx[r] += xs[maxs] - xs[mins];
                if (si[r] < minsi)
                    minsi = si[r];
                else if (si[r] > maxsi)
                    maxsi = si[r];
                disty[r] += ys[maxsi] - ys[minsi];
            }

            nbp=0;
            for (r=lb; r<=ub; r++) {
                if (si[r]==0 || si[r]==d-1)
                    score[nbp] = (xs[r+1] - xs[r-1]) - penalty[r]
                                 - AA*(ys[d-2]-ys[1]) - DDD*disty[r];
                else score[nbp] = (xs[r+1] - xs[r-1]) - penalty[r]
                                  - BB*(ys[si[r]+1]-ys[si[r]-1]) - DDD*disty[r];
                nbp++;

                if (s[r]==0 || s[r]==d-1)
                    score[nbp] = (ys[r+1] - ys[r-1]) - penalty[s[r]]
                                 - AA*(xs[d-2]-xs[1]) - DDD*distx[r];
                else score[nbp] = (ys[r+1] - ys[r-1]) - penalty[s[r]]
                                  - BB*(xs[s[r]+1]-xs[s[r]-1]) - DDD*distx[r];
                nbp++;
            }

            if (acc <= 3)
                newacc = 1;
            else {
                newacc = acc/2;
                if (acc >= nbp) acc = nbp-1;
            }

            minl = (DTYPE) INT_MAX;
            for (i=0; i<acc; i++) {
                maxbp = 0;
                for (bp=1; bp<nbp; bp++)
                    if (score[maxbp] < score[bp]) maxbp = bp;
                score[maxbp] = -9e9f;

#define BreakPt(bp) ((bp)/2+lb)
#define BreakInX(bp) ((bp)%2==0)
                p = BreakPt(maxbp);
                // Breaking in p
                if (BreakInX(maxbp)) {  // break in x
                    n1 = n2 = 0;
                    for (r=0; r<d; r++) {
                        if (s[r] < p) {
                            s1[n1] = s[r];
                            y1[n1] = ys[r];
                            n1++;
                        }
                        else if (s[r] > p) {
                            s2[n2] = s[r]-p;
                            y2[n2] = ys[r];
                            n2++;
                        }
                        else { // if (s[r] == p)  i.e.,  r = si[p]
                            s1[n1] = p;  s2[n2] = 0;
                            if (r == d-1 || r == d-2) {
                                y1[n1] = y2[n2] = ys[r-1];
                                extral = ys[r] - ys[r-1];
                            }
                            if (r == 0 || r == 1) {
                                y1[n1] = y2[n2] = ys[r+1];
                                extral = ys[r+1] - ys[r];
                            }
                            else {
                                y1[n1] = y2[n2] = ys[r];
                                extral = 0;
                            }
                            n1++;  n2++;
                        }
                    }
                    ll = extral + flutes_wl_LMD(p+1, xs, y1, s1, newacc)
                         + flutes_wl_LMD(d-p, xs+p, y2, s2, newacc);
                }
                else {  // if (!BreakInX(maxbp))
                    n1 = n2 = 0;
                    for (r=0; r<d; r++) {
                        if (si[r] < p) {
                            s1[si[r]] = n1;
                            x1[n1] = xs[r];
                            n1++;
                        }
                        else if (si[r] > p) {
                            s2[si[r]-p] = n2;
                            x2[n2] = xs[r];
                            n2++;
                        }
                        else { // if (si[r] == p)  i.e.,  r = s[p]
                            s1[p] = n1;  s2[0] = n2;
                            if (r == d-1 || r == d-2) {
                                x1[n1] = x2[n2] = xs[r-1];
                                extral = xs[r] - xs[r-1];
                            }
                            if (r == 0 || r == 1) {
                                x1[n1] = x2[n2] = xs[r+1];
                                extral = xs[r+1] - xs[r];
                            }
                            else {
                                x1[n1] = x2[n2] = xs[r];
                                extral = 0;
                            }
                            n1++;  n2++;
                        }
                    }
                    ll = extral + flutes_wl_LMD(p+1, x1, ys, s1, newacc)
                         + flutes_wl_LMD(d-p, x2, ys+p, s2, newacc);
                }
                if (minl > ll) minl = ll;
            }
            free(x1);
            free(x2);
            free(y1);
            free(y2);
            free(si);
            free(s1);
            free(s2);
            free(score);
            free(penalty);
            free(distx);
            free(disty);
            return minl;
        }

        tree flute(int d, DTYPE x[], DTYPE y[], int acc)
        {
            unsigned allocateSize = MAXD;
            if (d > MAXD)
            {
                allocateSize = d+1;
//    printf("setting allocateSize = %d\n", allocateSize);
            }
            DTYPE  *xs  = (DTYPE*)      malloc(sizeof(DTYPE)*allocateSize);
            DTYPE  *ys  = (DTYPE*)      malloc(sizeof(DTYPE)*allocateSize);
            int    *s   = (int*)        malloc(sizeof(int)*allocateSize);
            POINT  *pt  = (POINT*)      malloc(sizeof(POINT)*allocateSize);
            POINTptr *ptp = (POINTptr*) malloc(sizeof(POINTptr)*allocateSize);

            POINT* tmpp;
            DTYPE minval;
            int i, j, minidx;
            tree t;

            if (d==2) {
                t.deg = 2;
                t.length = ADIFF(x[0], x[1]) + ADIFF(y[0], y[1]);
                t.branch = (tree_branch *) malloc(2*sizeof(tree_branch));
                t.branch[0].x = x[0];
                t.branch[0].y = y[0];
                t.branch[0].n = 1;
                t.branch[1].x = x[1];
                t.branch[1].y = y[1];
                t.branch[1].n = 1;
            }
            else {
                for (i=0; i<d; i++) {
                    pt[i].x = x[i];
                    pt[i].y = y[i];
                    ptp[i] = &pt[i];
                }

                // sort x
                for (i=0; i<d-1; i++) {
                    minval = ptp[i]->x;
                    minidx = i;
                    for (j=i+1; j<d; j++) {
                        if (minval > ptp[j]->x || (minval == ptp[j]->x && ptp[minidx]->y < ptp[j]->y)) {
                            minval = ptp[j]->x;
                            minidx = j;
                        }
                    }
                    tmpp = ptp[i];
                    ptp[i] = ptp[minidx];
                    ptp[minidx] = tmpp;
                }

#if REMOVE_DUPLICATE_PIN==1
                ptp[d] = &pt[d];
    ptp[d]->x = ptp[d]->y = -999999;
    j = 0;
    for (i=0; i<d; i++) {
      for (k=i+1; ptp[k]->x == ptp[i]->x; k++)
        if (ptp[k]->y == ptp[i]->y)  // pins k and i are the same
          break;
      if (ptp[k]->x != ptp[i]->x)
        ptp[j++] = ptp[i];
    }
    d = j;
#endif

                for (i=0; i<d; i++) {
                    xs[i] = ptp[i]->x;
                    ptp[i]->o = i;
                }

                // sort y to find s[]
                for (i=0; i<d-1; i++) {
                    minval = ptp[i]->y;
                    minidx = i;
                    for (j=i+1; j<d; j++) {
                        if (minval > ptp[j]->y) {
                            minval = ptp[j]->y;
                            minidx = j;
                        }
                    }
                    ys[i] = ptp[minidx]->y;
                    s[i] = ptp[minidx]->o;
                    ptp[minidx] = ptp[i];
                }
                ys[d-1] = ptp[d-1]->y;
                s[d-1] = ptp[d-1]->o;

                t = flutes(d, xs, ys, s, acc);
            }
            free(xs);
            free(ys);
            free(s);
            free(pt);
            free(ptp);
            return t;
        }

// xs[] and ys[] are coords in x and y in sorted order
// s[] is a list of nodes in increasing y direction
//   if nodes are indexed in the order of increasing x coord
//   i.e., s[i] = s_i in defined in paper
// The points are (xs[s[i]], ys[i]) for i=0..d-1
//             or (xs[i], ys[si[i]]) for i=0..d-1

        tree flutes_RDP(int d, DTYPE xs[], DTYPE ys[], int s[], int acc)
        {
            int i, j, ss;

            for (i=0; i<d-1; i++) {
                if (xs[s[i]]==xs[s[i+1]] && ys[i]==ys[i+1]) {
                    if (s[i] < s[i+1])
                        ss = s[i+1];
                    else {
                        ss = s[i];
                        s[i] = s[i+1];
                    }
                    for (j=i+2; j<d; j++) {
                        ys[j-1] = ys[j];
                        s[j-1] = s[j];
                    }
                    for (j=ss+1; j<d; j++)
                        xs[j-1] = xs[j];
                    for (j=0; j<=d-2; j++)
                        if (s[j] > ss) s[j]--;
                    i--;
                    d--;
                }
            }
            return flutes_ALLD(d, xs, ys, s, acc);
        }

// For low-degree, i.e., 2 <= d <= FLUTE_D
        tree flutes_LD(int d, DTYPE xs[], DTYPE ys[], int s[])
        {
            int k, pi, i, j;
            struct csoln *rlist, *bestrlist;
            DTYPE dd[2*FLUTE_D-2];  // 0..FLUTE_D-2 for v, FLUTE_D-1..2*FLUTE_D-3 for h
            DTYPE minl, sum;
            DTYPE l[MPOWV+1];
            int hflip;
            tree t;

            t.deg = d;
            t.branch = (tree_branch *) malloc((2*d-2)*sizeof(tree_branch));
            if (d == 2) {
                minl = xs[1]-xs[0]+ys[1]-ys[0];
                t.branch[0].x = xs[s[0]];
                t.branch[0].y = ys[0];
                t.branch[0].n = 1;
                t.branch[1].x = xs[s[1]];
                t.branch[1].y = ys[1];
                t.branch[1].n = 1;
            }
            else if (d == 3) {
                minl = xs[2]-xs[0]+ys[2]-ys[0];
                t.branch[0].x = xs[s[0]];
                t.branch[0].y = ys[0];
                t.branch[0].n = 3;
                t.branch[1].x = xs[s[1]];
                t.branch[1].y = ys[1];
                t.branch[1].n = 3;
                t.branch[2].x = xs[s[2]];
                t.branch[2].y = ys[2];
                t.branch[2].n = 3;
                t.branch[3].x = xs[1];
                t.branch[3].y = ys[1];
                t.branch[3].n = 3;
            }
            else {
                k = 0;
                if (s[0] < s[2]) k++;
                if (s[1] < s[2]) k++;

                for (i=3; i<=d-1; i++) {  // p0=0 always, skip i=1 for symmetry
                    pi = s[i];
                    for (j=d-1; j>i; j--)
                        if (s[j] < s[i])
                            pi--;
                    k = pi + (i+1)*k;
                }

                if (k < numgrp[d]) { // no horizontal flip
                    hflip = 0;
                    for (i=1; i<=d-3; i++) {
                        dd[i]=ys[i+1]-ys[i];
                        dd[d-1+i]=xs[i+1]-xs[i];
                    }
                }
                else {
                    hflip = 1;
                    k=2*numgrp[d]-1-k;
                    for (i=1; i<=d-3; i++) {
                        dd[i]=ys[i+1]-ys[i];
                        dd[d-1+i]=xs[d-1-i]-xs[d-2-i];
                    }
                }

                minl = l[0] = xs[d-1]-xs[0]+ys[d-1]-ys[0];
                rlist = LUT[d][k];
                for (i=0; rlist->seg[i]>0; i++)
                    minl += dd[rlist->seg[i]];
                bestrlist = rlist;
                l[1] = minl;
                j = 2;
                while (j <= numsoln[d][k]) {
                    rlist++;
                    sum = l[rlist->parent];
                    for (i=0; rlist->seg[i]>0; i++)
                        sum += dd[rlist->seg[i]];
                    for (i=10; rlist->seg[i]>0; i--)
                        sum -= dd[rlist->seg[i]];
                    if (sum < minl) {
                        minl = sum;
                        bestrlist = rlist;
                    }
                    l[j++] = sum;
                }

                t.branch[0].x = xs[s[0]];
                t.branch[0].y = ys[0];
                t.branch[1].x = xs[s[1]];
                t.branch[1].y = ys[1];
                for (i=2; i<d-2; i++) {
                    t.branch[i].x = xs[s[i]];
                    t.branch[i].y = ys[i];
                    t.branch[i].n = bestrlist->neighbor[i];
                }
                t.branch[d-2].x = xs[s[d-2]];
                t.branch[d-2].y = ys[d-2];
                t.branch[d-1].x = xs[s[d-1]];
                t.branch[d-1].y = ys[d-1];
                if (hflip) {
                    if (s[1] < s[0]) {
                        t.branch[0].n = bestrlist->neighbor[1];
                        t.branch[1].n = bestrlist->neighbor[0];
                    }
                    else {
                        t.branch[0].n = bestrlist->neighbor[0];
                        t.branch[1].n = bestrlist->neighbor[1];
                    }
                    if (s[d-1] < s[d-2]) {
                        t.branch[d-2].n = bestrlist->neighbor[d-1];
                        t.branch[d-1].n = bestrlist->neighbor[d-2];
                    }
                    else {
                        t.branch[d-2].n = bestrlist->neighbor[d-2];
                        t.branch[d-1].n = bestrlist->neighbor[d-1];
                    }
                    for (i=d; i<2*d-2; i++) {
                        t.branch[i].x = xs[d-1-bestrlist->col[i-d]];
                        t.branch[i].y = ys[bestrlist->row[i-d]];
                        t.branch[i].n = bestrlist->neighbor[i];
                    }
                }
                else {  // !hflip
                    if (s[0] < s[1]) {
                        t.branch[0].n = bestrlist->neighbor[1];
                        t.branch[1].n = bestrlist->neighbor[0];
                    }
                    else {
                        t.branch[0].n = bestrlist->neighbor[0];
                        t.branch[1].n = bestrlist->neighbor[1];
                    }
                    if (s[d-2] < s[d-1]) {
                        t.branch[d-2].n = bestrlist->neighbor[d-1];
                        t.branch[d-1].n = bestrlist->neighbor[d-2];
                    }
                    else {
                        t.branch[d-2].n = bestrlist->neighbor[d-2];
                        t.branch[d-1].n = bestrlist->neighbor[d-1];
                    }
                    for (i=d; i<2*d-2; i++) {
                        t.branch[i].x = xs[bestrlist->col[i-d]];
                        t.branch[i].y = ys[bestrlist->row[i-d]];
                        t.branch[i].n = bestrlist->neighbor[i];
                    }
                }
            }
            t.length = minl;

            return t;
        }

// For medium-degree, i.e., FLUTE_D+1 <= d <= D2
        tree flutes_MD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc)
        {
            unsigned allocateSize = MAXD;
            if (d > MAXD)
                allocateSize = d+1;
            DTYPE *x1      = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            DTYPE *x2      = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            DTYPE *y1      = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            DTYPE *y2      = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            int   *si      = (int*)   malloc(sizeof(int)*allocateSize);
            int   *s1      = (int*)   malloc(sizeof(int)*allocateSize);
            int   *s2      = (int*)   malloc(sizeof(int)*allocateSize);
            float *score   = (float*) malloc(sizeof(float)*2*allocateSize);
            float *penalty = (float*) malloc(sizeof(float)*allocateSize);
            DTYPE *distx   = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);
            DTYPE *disty   = (DTYPE*) malloc(sizeof(DTYPE)*allocateSize);

//  DTYPE x1[MAXD], x2[MAXD], y1[MAXD], y2[MAXD];
//  int si[MAXD], s1[MAXD], s2[MAXD];
//  float score[2*MAXD], penalty[MAXD];
//  DTYPE distx[MAXD], disty[MAXD];
            float pnlty, dx, dy;
            DTYPE ll, minl, coord1, coord2;
            int i, r, p, maxbp, bestbp, bp, nbp, ub, lb, n1, n2, nn1, nn2, ms, newacc;
            tree t, t1, t2, bestt1, bestt2;
            int mins, maxs, minsi, maxsi;
            DTYPE xydiff;

            for(i = 0; i < allocateSize; ++i)
            {
                distx[i] = disty[i] = 0;
            }

            if (s[0] < s[d-1]) {
                ms = max(s[0], s[1]);
                for (i=2; i<=ms; i++)
                    ms = max(ms, s[i]);
                if (ms <= d-3) {
                    for (i=0; i<=ms; i++) {
                        x1[i] = xs[i];
                        y1[i] = ys[i];
                        s1[i] = s[i];
                    }
                    x1[ms+1] = xs[ms];
                    y1[ms+1] = ys[ms];
                    s1[ms+1] = ms+1;

                    s2[0] = 0;
                    for (i=1; i<=d-1-ms; i++)
                        s2[i] = s[i+ms]-ms;

                    t1 = flutes_LMD(ms+2, x1, y1, s1, acc);
                    t2 = flutes_LMD(d-ms, xs+ms, ys+ms, s2, acc);
                    t = dmergetree(t1, t2);
                    free(t1.branch);
                    free(t2.branch);

                    free(x1);
                    free(x2);
                    free(y1);
                    free(y2);
                    free(si);
                    free(s1);
                    free(s2);
                    free(score);
                    free(penalty);
                    free(distx);
                    free(disty);
                    return t;
                }
            }
            else {  // (s[0] > s[d-1])
                ms = min(s[0], s[1]);
                for (i=2; i<=d-1-ms; i++)
                    ms = min(ms, s[i]);
                if (ms >= 2) {
                    x1[0] = xs[ms];
                    y1[0] = ys[0];
                    s1[0] = s[0]-ms+1;
                    for (i=1; i<=d-1-ms; i++) {
                        x1[i] = xs[i+ms-1];
                        y1[i] = ys[i];
                        s1[i] = s[i]-ms+1;
                    }
                    x1[d-ms] = xs[d-1];
                    y1[d-ms] = ys[d-1-ms];
                    s1[d-ms] = 0;

                    s2[0] = ms;
                    for (i=1; i<=ms; i++)
                        s2[i] = s[i+d-1-ms];

                    t1 = flutes_LMD(d+1-ms, x1, y1, s1, acc);
                    t2 = flutes_LMD(ms+1, xs, ys+d-1-ms, s2, acc);
                    t = dmergetree(t1, t2);
                    free(t1.branch);
                    free(t2.branch);

                    free(x1);
                    free(x2);
                    free(y1);
                    free(y2);
                    free(si);
                    free(s1);
                    free(s2);
                    free(score);
                    free(penalty);
                    free(distx);
                    free(disty);
                    return t;
                }
            }

            // Find inverse si[] of s[]
            for (r=0; r<d; r++)
                si[s[r]] = r;

            // Determine breaking directions and positions dp[]
            lb=max((d-acc)/5, 2);
            ub=d-1-lb;

            // Compute scores
#define AA 0.6  // 2.0*BB
#define BB 0.3
            //#define CCC max(0.425-0.005*d-0.015*acc, 0.1)
            //#define CCC max(0.43-0.005*d-0.01*acc, 0.1)
#define CCC max(0.41-0.005*d, 0.1)
#define DD 4.8
            float DDD = DD/(d-1);

            // Compute penalty[]
            dx = CCC*(xs[d-2]-xs[1])/(d-3);
            dy = CCC*(ys[d-2]-ys[1])/(d-3);
            for (r = d/2, pnlty = 0; r>=2; r--, pnlty += dx)
                penalty[r] = pnlty,  penalty[d-1-r] = pnlty;
            penalty[1] = pnlty,  penalty[d-2] = pnlty;
            penalty[0] = pnlty,  penalty[d-1] = pnlty;
            for (r = d/2-1, pnlty = dy; r>=2; r--, pnlty += dy)
                penalty[s[r]] += pnlty,  penalty[s[d-1-r]] += pnlty;
            penalty[s[1]] += pnlty,  penalty[s[d-2]] += pnlty;
            penalty[s[0]] += pnlty,  penalty[s[d-1]] += pnlty;
            //#define CC 0.16
            //#define v(r) ((r==0||r==1||r==d-2||r==d-1) ? d-3 : abs(d-1-r-r))
            //    for (r=0; r<d; r++)
            //        penalty[r] = v(r)*dx + v(si[r])*dy;

            // Compute distx[], disty[]
            xydiff = (xs[d-1] - xs[0]) - (ys[d-1] - ys[0]);
            if (s[0] < s[1])
                mins = s[0], maxs = s[1];
            else mins = s[1], maxs = s[0];
            if (si[0] < si[1])
                minsi = si[0], maxsi = si[1];
            else minsi = si[1], maxsi = si[0];
            for (r=2; r<=ub; r++) {
                if (s[r] < mins)
                    mins = s[r];
                else if (s[r] > maxs)
                    maxs = s[r];
                distx[r] = xs[maxs] - xs[mins];
                if (si[r] < minsi)
                    minsi = si[r];
                else if (si[r] > maxsi)
                    maxsi = si[r];
                disty[r] = ys[maxsi] - ys[minsi] + xydiff;
            }

            if (s[d-2] < s[d-1])
                mins = s[d-2], maxs = s[d-1];
            else mins = s[d-1], maxs = s[d-2];
            if (si[d-2] < si[d-1])
                minsi = si[d-2], maxsi = si[d-1];
            else minsi = si[d-1], maxsi = si[d-2];
            for (r=d-3; r>=lb; r--) {
                if (s[r] < mins)
                    mins = s[r];
                else if (s[r] > maxs)
                    maxs = s[r];
                distx[r] += xs[maxs] - xs[mins];
                if (si[r] < minsi)
                    minsi = si[r];
                else if (si[r] > maxsi)
                    maxsi = si[r];
                disty[r] += ys[maxsi] - ys[minsi];
            }

            nbp=0;
            for (r=lb; r<=ub; r++) {
                if (si[r]<=1)
                    score[nbp] = (xs[r+1] - xs[r-1]) - penalty[r]
                                 - AA*(ys[2]-ys[1]) - DDD*disty[r];
                else if (si[r]>=d-2)
                    score[nbp] = (xs[r+1] - xs[r-1]) - penalty[r]
                                 - AA*(ys[d-2]-ys[d-3]) - DDD*disty[r];
                else score[nbp] = (xs[r+1] - xs[r-1]) - penalty[r]
                                  - BB*(ys[si[r]+1]-ys[si[r]-1]) - DDD*disty[r];
                nbp++;

                if (s[r]<=1)
                    score[nbp] = (ys[r+1] - ys[r-1]) - penalty[s[r]]
                                 - AA*(xs[2]-xs[1]) - DDD*distx[r];
                else if (s[r]>=d-2)
                    score[nbp] = (ys[r+1] - ys[r-1]) - penalty[s[r]]
                                 - AA*(xs[d-2]-xs[d-3]) - DDD*distx[r];
                else score[nbp] = (ys[r+1] - ys[r-1]) - penalty[s[r]]
                                  - BB*(xs[s[r]+1]-xs[s[r]-1]) - DDD*distx[r];
                nbp++;
            }

            if (acc <= 3)
                newacc = 1;
            else {
                newacc = acc/2;
                if (acc >= nbp) acc = nbp-1;
            }

            minl = (DTYPE) INT_MAX;
            bestt1.branch = bestt2.branch = NULL;
            for (i=0; i<acc; i++) {
                maxbp = 0;
                for (bp=1; bp<nbp; bp++)
                    if (score[maxbp] < score[bp]) maxbp = bp;
                score[maxbp] = -9e9f;

#define BreakPt(bp) ((bp)/2+lb)
#define BreakInX(bp) ((bp)%2==0)
                p = BreakPt(maxbp);
                // Breaking in p
                if (BreakInX(maxbp)) {  // break in x
                    n1 = n2 = 0;
                    for (r=0; r<d; r++) {
                        if (s[r] < p) {
                            s1[n1] = s[r];
                            y1[n1] = ys[r];
                            n1++;
                        }
                        else if (s[r] > p) {
                            s2[n2] = s[r]-p;
                            y2[n2] = ys[r];
                            n2++;
                        }
                        else { // if (s[r] == p)  i.e.,  r = si[p]
                            s1[n1] = p;  s2[n2] = 0;
                            y1[n1] = y2[n2] = ys[r];
                            nn1 = n1;  nn2 = n2;
                            n1++;  n2++;
                        }
                    }

                    t1 = flutes_LMD(p+1, xs, y1, s1, newacc);
                    t2 = flutes_LMD(d-p, xs+p, y2, s2, newacc);
                    ll = t1.length + t2.length;
                    coord1 = t1.branch[t1.branch[nn1].n].y;
                    coord2 = t2.branch[t2.branch[nn2].n].y;
                    if (t2.branch[nn2].y > max(coord1, coord2))
                        ll -= t2.branch[nn2].y - max(coord1, coord2);
                    else if (t2.branch[nn2].y < min(coord1, coord2))
                        ll -= min(coord1, coord2) - t2.branch[nn2].y;
                }
                else {  // if (!BreakInX(maxbp))
                    n1 = n2 = 0;
                    for (r=0; r<d; r++) {
                        if (si[r] < p) {
                            s1[si[r]] = n1;
                            x1[n1] = xs[r];
                            n1++;
                        }
                        else if (si[r] > p) {
                            s2[si[r]-p] = n2;
                            x2[n2] = xs[r];
                            n2++;
                        }
                        else { // if (si[r] == p)  i.e.,  r = s[p]
                            s1[p] = n1;  s2[0] = n2;
                            x1[n1] = x2[n2] = xs[r];
                            n1++;  n2++;
                        }
                    }

                    t1 = flutes_LMD(p+1, x1, ys, s1, newacc);
                    t2 = flutes_LMD(d-p, x2, ys+p, s2, newacc);
                    ll = t1.length + t2.length;
                    coord1 = t1.branch[t1.branch[p].n].x;
                    coord2 = t2.branch[t2.branch[0].n].x;
                    if (t2.branch[0].x > max(coord1, coord2))
                        ll -= t2.branch[0].x - max(coord1, coord2);
                    else if (t2.branch[0].x < min(coord1, coord2))
                        ll -= min(coord1, coord2) - t2.branch[0].x;
                }
                if (minl > ll) {
                    minl = ll;
                    free(bestt1.branch);
                    free(bestt2.branch);
                    bestt1 = t1;
                    bestt2 = t2;
                    bestbp = maxbp;
                }
                else {
                    free(t1.branch);
                    free(t2.branch);
                }
            }

            if (BreakInX(bestbp))
                t = hmergetree(bestt1, bestt2, s);
            else t = vmergetree(bestt1, bestt2);
            free(bestt1.branch);
            free(bestt2.branch);

            free(x1);
            free(x2);
            free(y1);
            free(y2);
            free(si);
            free(s1);
            free(s2);
            free(score);
            free(penalty);
            free(distx);
            free(disty);
            return t;
        }

        tree dmergetree(tree t1, tree t2)
        {
            int i, d, prev, curr, next, offset1, offset2;
            tree t;

            t.deg = d = t1.deg + t2.deg - 2;
            t.length = t1.length + t2.length;
            t.branch = (tree_branch *) malloc((2*d-2)*sizeof(tree_branch));
            offset1 = t2.deg-2;
            offset2 = 2*t1.deg-4;

            for (i=0; i<=t1.deg-2; i++) {
                t.branch[i].x = t1.branch[i].x;
                t.branch[i].y = t1.branch[i].y;
                t.branch[i].n = t1.branch[i].n + offset1;
            }
            for (i=t1.deg-1; i<=d-1; i++) {
                t.branch[i].x = t2.branch[i-t1.deg+2].x;
                t.branch[i].y = t2.branch[i-t1.deg+2].y;
                t.branch[i].n = t2.branch[i-t1.deg+2].n + offset2;
            }
            for (i=d; i<=d+t1.deg-3; i++) {
                t.branch[i].x = t1.branch[i-offset1].x;
                t.branch[i].y = t1.branch[i-offset1].y;
                t.branch[i].n = t1.branch[i-offset1].n + offset1;
            }
            for (i=d+t1.deg-2; i<=2*d-3; i++) {
                t.branch[i].x = t2.branch[i-offset2].x;
                t.branch[i].y = t2.branch[i-offset2].y;
                t.branch[i].n = t2.branch[i-offset2].n + offset2;
            }

            prev = t2.branch[0].n + offset2;
            curr = t1.branch[t1.deg-1].n + offset1;
            next = t.branch[curr].n;
            while (curr != next) {
                t.branch[curr].n = prev;
                prev = curr;
                curr = next;
                next = t.branch[curr].n;
            }
            t.branch[curr].n = prev;

            return t;
        }

        tree hmergetree(tree t1, tree t2, int s[])
        {
            int i, prev, curr, next, extra, offset1, offset2;
            int p, ii, n1, n2, nn1, nn2;
            DTYPE coord1, coord2;
            tree t;

            t.deg = t1.deg + t2.deg - 1;
            t.length = t1.length + t2.length;
            t.branch = (tree_branch *) malloc((2*t.deg-2)*sizeof(tree_branch));
            offset1 = t2.deg-1;
            offset2 = 2*t1.deg-3;

            p = t1.deg - 1;
            n1 = n2 = 0;
            for (i=0; i<t.deg; i++) {
                if (s[i] < p) {
                    t.branch[i].x = t1.branch[n1].x;
                    t.branch[i].y = t1.branch[n1].y;
                    t.branch[i].n = t1.branch[n1].n + offset1;
                    n1++;
                }
                else if (s[i] > p) {
                    t.branch[i].x = t2.branch[n2].x;
                    t.branch[i].y = t2.branch[n2].y;
                    t.branch[i].n = t2.branch[n2].n + offset2;
                    n2++;
                }
                else {
                    t.branch[i].x = t2.branch[n2].x;
                    t.branch[i].y = t2.branch[n2].y;
                    t.branch[i].n = t2.branch[n2].n + offset2;
                    nn1 = n1;  nn2 = n2;  ii = i;
                    n1++;  n2++;
                }
            }
            for (i=t.deg; i<=t.deg+t1.deg-3; i++) {
                t.branch[i].x = t1.branch[i-offset1].x;
                t.branch[i].y = t1.branch[i-offset1].y;
                t.branch[i].n = t1.branch[i-offset1].n + offset1;
            }
            for (i=t.deg+t1.deg-2; i<=2*t.deg-4; i++) {
                t.branch[i].x = t2.branch[i-offset2].x;
                t.branch[i].y = t2.branch[i-offset2].y;
                t.branch[i].n = t2.branch[i-offset2].n + offset2;
            }
            extra = 2*t.deg-3;
            coord1 = t1.branch[t1.branch[nn1].n].y;
            coord2 = t2.branch[t2.branch[nn2].n].y;
            if (t2.branch[nn2].y > max(coord1, coord2)) {
                t.branch[extra].y = max(coord1, coord2);
                t.length -= t2.branch[nn2].y - t.branch[extra].y;
            }
            else if (t2.branch[nn2].y < min(coord1, coord2)) {
                t.branch[extra].y = min(coord1, coord2);
                t.length -= t.branch[extra].y - t2.branch[nn2].y;
            }
            else t.branch[extra].y = t2.branch[nn2].y;
            t.branch[extra].x = t2.branch[nn2].x;
            t.branch[extra].n = t.branch[ii].n;
            t.branch[ii].n = extra;

            prev = extra;
            curr = t1.branch[nn1].n + offset1;
            next = t.branch[curr].n;
            while (curr != next) {
                t.branch[curr].n = prev;
                prev = curr;
                curr = next;
                next = t.branch[curr].n;
            }
            t.branch[curr].n = prev;

            return t;
        }

        tree vmergetree(tree t1, tree t2)
        {
            int i, prev, curr, next, extra, offset1, offset2;
            DTYPE coord1, coord2;
            tree t;

            t.deg = t1.deg + t2.deg - 1;
            t.length = t1.length + t2.length;
            t.branch = (tree_branch *) malloc((2*t.deg-2)*sizeof(tree_branch));
            offset1 = t2.deg-1;
            offset2 = 2*t1.deg-3;

            for (i=0; i<=t1.deg-2; i++) {
                t.branch[i].x = t1.branch[i].x;
                t.branch[i].y = t1.branch[i].y;
                t.branch[i].n = t1.branch[i].n + offset1;
            }
            for (i=t1.deg-1; i<=t.deg-1; i++) {
                t.branch[i].x = t2.branch[i-t1.deg+1].x;
                t.branch[i].y = t2.branch[i-t1.deg+1].y;
                t.branch[i].n = t2.branch[i-t1.deg+1].n + offset2;
            }
            for (i=t.deg; i<=t.deg+t1.deg-3; i++) {
                t.branch[i].x = t1.branch[i-offset1].x;
                t.branch[i].y = t1.branch[i-offset1].y;
                t.branch[i].n = t1.branch[i-offset1].n + offset1;
            }
            for (i=t.deg+t1.deg-2; i<=2*t.deg-4; i++) {
                t.branch[i].x = t2.branch[i-offset2].x;
                t.branch[i].y = t2.branch[i-offset2].y;
                t.branch[i].n = t2.branch[i-offset2].n + offset2;
            }
            extra = 2*t.deg-3;
            coord1 = t1.branch[t1.branch[t1.deg-1].n].x;
            coord2 = t2.branch[t2.branch[0].n].x;
            if (t2.branch[0].x > max(coord1, coord2)) {
                t.branch[extra].x = max(coord1, coord2);
                t.length -= t2.branch[0].x - t.branch[extra].x;
            }
            else if (t2.branch[0].x < min(coord1, coord2)) {
                t.branch[extra].x = min(coord1, coord2);
                t.length -= t.branch[extra].x - t2.branch[0].x;
            }
            else t.branch[extra].x = t2.branch[0].x;
            t.branch[extra].y = t2.branch[0].y;
            t.branch[extra].n = t.branch[t1.deg-1].n;
            t.branch[t1.deg-1].n = extra;

            prev = extra;
            curr = t1.branch[t1.deg-1].n + offset1;
            next = t.branch[curr].n;
            while (curr != next) {
                t.branch[curr].n = prev;
                prev = curr;
                curr = next;
                next = t.branch[curr].n;
            }
            t.branch[curr].n = prev;

            return t;
        }

        DTYPE wirelength(tree t)
        {
            int i, j;
            DTYPE l=0;

            for (i=0; i<2*t.deg-2; i++) {
                j = t.branch[i].n;
                l += ADIFF(t.branch[i].x, t.branch[j].x)
                     + ADIFF(t.branch[i].y, t.branch[j].y);
            }

            return l;
        }

        void printtree(tree t)
        {
            int i;

            for (i=0; i<t.deg; i++)
                printf(" %-2d:  x=%4g  y=%4g  e=%d\n",
                       i, (float) t.branch[i].x, (float) t.branch[i].y, t.branch[i].n);
            for (i=t.deg; i<2*t.deg-2; i++)
                printf("s%-2d:  x=%4g  y=%4g  e=%d\n",
                       i, (float) t.branch[i].x, (float) t.branch[i].y, t.branch[i].n);
            printf("\n");
        }

/*
double calcIllegality(const vector< pair<Point,Point> > &tempEdges, const vector<BBox> &obs, const vector<unsigned> &relevantObs)
{
   double totalIllegality = 0.;

   for(unsigned i = 0; i < tempEdges.size(); ++i)
   {
     double xFirstIntersectionLen = 0.;
     double yFirstIntersectionLen = 0.;

     double xMin = min(tempEdges[i].first.x, tempEdges[i].second.x);
     double xMax = max(tempEdges[i].first.x, tempEdges[i].second.x);
     double yMin = min(tempEdges[i].first.y, tempEdges[i].second.y);
     double yMax = max(tempEdges[i].first.y, tempEdges[i].second.y);

     for(unsigned j = 0; j < relevantObs.size(); ++j)
     {
       // test the x first path
       if(lessThanDouble(tempEdges[i].first.y, obs[relevantObs[j]].yMax) &&
          greaterThanDouble(tempEdges[i].first.y, obs[relevantObs[j]].yMin) &&
          lessThanDouble(xMin, obs[relevantObs[j]].xMax) &&
          greaterThanDouble(xMax, obs[relevantObs[j]].xMin))
       {
         // x segment of x first path fails
         xFirstIntersectionLen += min(xMax, obs[relevantObs[j]].xMax) - max(xMin, obs[relevantObs[j]].xMin);
       }
       if(lessThanDouble(tempEdges[i].second.x, obs[relevantObs[j]].xMax) &&
          greaterThanDouble(tempEdges[i].second.x, obs[relevantObs[j]].xMin) &&
          lessThanDouble(yMin, obs[relevantObs[j]].yMax) &&
          greaterThanDouble(yMax, obs[relevantObs[j]].yMin))
       {
         // y segment of x first path fails
         xFirstIntersectionLen += min(yMax, obs[relevantObs[j]].yMax) - max(yMin, obs[relevantObs[j]].yMin);
       }

       // test the y first path
       if(lessThanDouble(tempEdges[i].second.y, obs[relevantObs[j]].yMax) &&
          greaterThanDouble(tempEdges[i].second.y, obs[relevantObs[j]].yMin) &&
          lessThanDouble(xMin, obs[relevantObs[j]].xMax) &&
          greaterThanDouble(xMax, obs[relevantObs[j]].xMin))
       {
         // x segment of y first path fails
         yFirstIntersectionLen += min(xMax, obs[relevantObs[j]].xMax) - max(xMin, obs[relevantObs[j]].xMin);
       }
       else if(lessThanDouble(tempEdges[i].first.x, obs[relevantObs[j]].xMax) &&
               greaterThanDouble(tempEdges[i].first.x, obs[relevantObs[j]].xMin) &&
               lessThanDouble(yMin, obs[relevantObs[j]].yMax) &&
               greaterThanDouble(yMax, obs[relevantObs[j]].yMin))
       {
         // y segment of y first path fails
         yFirstIntersectionLen += min(yMax, obs[relevantObs[j]].yMax) - max(yMin, obs[relevantObs[j]].yMin);
       }
     }

     totalIllegality += min(xFirstIntersectionLen, yFirstIntersectionLen);
   }

   return totalIllegality;
}
*/

/*
tree flautist(int d, DTYPE x[], DTYPE y[], int acc, const vector<BBox> &obs, const vector<unsigned> &relevantObs, unsigned &legal)
{
   abkfatal(d <= MAXD, "Flautist called with too many points");

   // set legal to default to false
   legal = 0;

   // build a tree with no additional points and see if it intersects any obstacles
   tree currtree = flute(d, x, y, acc);

   if(relevantObs.size() == 0)
   {
     // if there are no relevant obstacles, just return the tree
     legal = 1;
     return currtree;
   }

   vector< pair<Point,Point> > tempEdges;
   for(int i = 0; i < 2*currtree.deg - 2; ++i)
   {
     int n = currtree.branch[i].n;
     if(i == n) continue;
     Point first (currtree.branch[i].x, currtree.branch[i].y);
     Point second(currtree.branch[n].x, currtree.branch[n].y);

     if(first != second) { tempEdges.push_back(make_pair(first,second)); }
   }


   double bestLen = currtree.length;
   double bestIllegality = calcIllegality(tempEdges, obs, relevantObs);

   if(equalDouble(bestIllegality,0.))
   {
     legal = 1;
     return currtree;
   }

   unsigned maxAdditionalPoints = max(4*relevantObs.size(), static_cast<unsigned>(MAXD - d));

   double *xcoords = new double[d + maxAdditionalPoints];
   double *ycoords = new double[d + maxAdditionalPoints];

   for(int i = 0; i < d; ++i)
   {
     xcoords[i] = x[i];
     ycoords[i] = y[i];
   }

   set<Point> oldPoints;
   for(int i = 0; i < d; ++i)
   {
     oldPoints.insert(Point(x[i],y[i]));
   }

   set<Point> newPoints;
   for(set<Point>::const_iterator i = oldPoints.begin(); i != oldPoints.end(); ++i)
   {
     BBox basebox;
     basebox += *i;
     for(unsigned j = 0; j < relevantObs.size(); ++j)
     {
       Point newPoint;
       // for each of these pairs, (one orig point and one obstacle
       // corner) add the unique points

       // bot left
       BBox tempbox = basebox;
       tempbox += Point(obs[relevantObs[j]].xMin, obs[relevantObs[j]].yMin);

       newPoint.x = tempbox.xMin;
       newPoint.y = tempbox.yMin;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.x = tempbox.xMax;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.y = tempbox.yMax;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.x = tempbox.xMin;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);

       // bot right
       tempbox = basebox;
       tempbox += Point(obs[relevantObs[j]].xMax, obs[relevantObs[j]].yMin);

       newPoint.x = tempbox.xMin;
       newPoint.y = tempbox.yMin;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.x = tempbox.xMax;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.y = tempbox.yMax;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.x = tempbox.xMin;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);

       // top left
       tempbox = basebox;
       tempbox += Point(obs[relevantObs[j]].xMin, obs[relevantObs[j]].yMax);

       newPoint.x = tempbox.xMin;
       newPoint.y = tempbox.yMin;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.x = tempbox.xMax;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.y = tempbox.yMax;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.x = tempbox.xMin;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);

       // top right
       tempbox = basebox;
       tempbox += Point(obs[relevantObs[j]].xMax, obs[relevantObs[j]].yMax);

       newPoint.x = tempbox.xMin;
       newPoint.y = tempbox.yMin;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.x = tempbox.xMax;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.y = tempbox.yMax;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
       newPoint.x = tempbox.xMin;
       if(oldPoints.find(newPoint) == oldPoints.end()) newPoints.insert(newPoint);
     }
   }


   tree testtree;

   for(unsigned addedPoints = 1; addedPoints <= maxAdditionalPoints; ++addedPoints)
   {
     Point addedPoint(DBL_MAX,DBL_MAX);

     for(set<Point>::const_iterator j = newPoints.begin(); j != newPoints.end(); ++j)
     {
        xcoords[d + addedPoints - 1] = j->x;
        ycoords[d + addedPoints - 1] = j->y;
        testtree = flute(d + addedPoints, xcoords, ycoords, acc);
        tempEdges.clear();
        for(int i = 0; i < 2*testtree.deg - 2; ++i)
        {
          int n = testtree.branch[i].n;
          if(i == n) continue;
          Point first (testtree.branch[i].x, testtree.branch[i].y);
          Point second(testtree.branch[n].x, testtree.branch[n].y);

          if(first != second) { tempEdges.push_back(make_pair(first,second)); }
        }
        double illegality = calcIllegality(tempEdges, obs, relevantObs);
        if(lessThanDouble(illegality, bestIllegality) ||
           (equalDouble(illegality, bestIllegality) && (testtree.length < bestLen)))
        {
          free(currtree.branch);
          currtree = testtree;
          bestLen = currtree.length;
          bestIllegality = illegality;
          addedPoint = *j;
        }
        else
        {
          free(testtree.branch);
        }
     }

     if(addedPoint.x == DBL_MAX && addedPoint.y == DBL_MAX) break;
     xcoords[d + addedPoints - 1] = addedPoint.x;
     ycoords[d + addedPoints - 1] = addedPoint.y;

     newPoints.erase(newPoints.find(addedPoint));

     if(equalDouble(bestIllegality, 0.)) break;
   }

   delete [] xcoords;
   delete [] ycoords;

   legal = equalDouble(bestIllegality, 0.);

   return currtree;
}
*/
    }
}