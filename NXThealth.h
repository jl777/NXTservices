//
//  NXThealth.h
//  Created by jl777, April 2014
//  MIT License
//

#ifndef gateway_NXThealth_h
#define gateway_NXThealth_h

/*
marcus03
I remembered that CfB and BCNext were monitoring the NXT network when we were under DDOS and asked him how they did it. Here's his answer:
"We had nodes in different datacenters. All these nodes were connected to Nxt network (to different peers!) and had direct connections to each other. Once a node received a block or an unconfirmed transaction it sent this to other nodes (not Nxt peers). The others measured time between the signal and time when they received the same block/transaction from Nxt peers. The delay is the main indicator that shows time of network convergence. During DDoS attacks average convergence time becomes much higher. Forks r also easily detected.
We also measure load on other peers by sending transactions to one of them from one node and receiving on another one. U can easily extended this approach and add extra functionality.
We used tool written by BCNext and I can't share it without his explicit permission, which is hard to get coz I'm not in touch with him anymore."
*/

int32_t NXTnetwork_healthy(struct NXThandler_info *mp)
{
    if ( mp->fractured_prob > .01 )
        return(-1);
    if ( mp->RTflag >= MIN_NXTCONFIRMS && mp->deadman < (60/mp->pollseconds)+1 )
        return(1);
    else return(0);
}


#endif
