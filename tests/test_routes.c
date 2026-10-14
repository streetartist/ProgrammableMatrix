#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "route_plan.h"
int main(void){
    routes_t r={0};int y=-1;
    assert(routes_validate(&r)==0);
    assert(routes_connect(&r,0,20,&y)==0 && y==0);
    assert(routes_connect(&r,20,40,&y)==0 && y==0);
    assert(routes_connect(&r,1,21,&y)==0 && y==1);
    assert(routes_connect(&r,2,22,&y)==0 && y==2);
    assert(routes_connect(&r,0,1,&y)==0 && y==0);
    assert(r.net[1]==0);assert(routes_net_of(&r,21)==0);
    assert(routes_remove(&r,40)==0 && routes_net_of(&r,40)==-1);
    // power collisions, MCU 5V denial
    routes_t old=r;
    assert(routes_connect(&r,49,50,NULL)==J_FORBIDDEN);
    assert(memcmp(&r,&old,sizeof r)==0);
    assert(routes_connect(&r,49,40,NULL)==J_FORBIDDEN);
    assert(memcmp(&r,&old,sizeof r)==0);
    // The default safe policy prevents shorting two ESP push-pull drivers.
    assert(routes_connect(&r,40,41,NULL)==J_FORBIDDEN);
    assert(memcmp(&r,&old,sizeof r)==0);
    // exact 8 independent nets: on the ninth, fail
    memset(&r,0,sizeof r);
    for(unsigned i=0;i<8;i++) assert(routes_connect(&r,2*i,2*i+1,NULL)==0);
    assert(routes_connect(&r,30,31,NULL)==J_OVERFLOW);
    // No shared nodes even if duplicate manually provided
    memset(&r,0,sizeof r);r.net[0]=1;r.net[1]=1;
    assert(routes_validate(&r)==J_BAD_ARG);
    // mapping of all 52 nodes: all distinct physical X and chip
    bool used[4][16]={0};
    for(unsigned i=0;i<52;i++){
        unsigned c=mapped_chip(i),x=mapped_x(i);
        assert(c<4 && x<16 && !used[c][x]);used[c][x]=true;
    }
    puts("route allocator and pin mapping: PASS");
}
